#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
evidence="$(pwd)/evidence"
source="$(pwd)/hw6/Assignment6"
git rev-parse HEAD > "$evidence/commit.txt"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
for mode in normal sanitized; do
 sanitize=OFF
 if [ "$mode" = sanitized ]; then sanitize=ON; fi
 cmake -S "$source" -B "$source/build-$mode" -DCMAKE_BUILD_TYPE=Release -DHW6_SANITIZE="$sanitize" 2>&1 | tee "$evidence/configure-$mode.log"
 cmake --build "$source/build-$mode" --parallel 2 2>&1 | tee "$evidence/build-$mode.log"
 (
  cd "$source/build-$mode"
  timeout 120 ./hw6_tests 2>&1 | tee "$evidence/checks-$mode.log"
  if [ "$mode" = normal ]; then
   timeout 180 ./hw6_tests --benchmark 2>&1 | tee "$evidence/benchmark.log"
   cp benchmark.csv "$evidence/benchmark.csv"
  fi
  for method in naive sah; do
   timeout 180 ./RayTracing "$method" > "$evidence/render-$mode-$method.log" 2>&1
   cp binary.ppm "$evidence/$mode-$method.ppm"
  done
  if ./RayTracing invalid > "$evidence/invalid-$mode.log" 2>&1; then
   echo 'invalid method incorrectly succeeded' >&2; exit 1
  fi
 )
done
python3 - <<'PY'
from pathlib import Path
import csv,hashlib,json,statistics,struct,subprocess,tempfile,zipfile,zlib

evidence=Path('evidence'); images=[]; baseline=None

def png(path,w,h,data):
 def chunk(kind,body):return struct.pack('!I',len(body))+kind+body+struct.pack('!I',zlib.crc32(kind+body)&0xffffffff)
 raw=b''.join(b'\0'+data[y*w*3:(y+1)*w*3] for y in range(h))
 path.write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

for mode in ('normal','sanitized'):
 for method in ('naive','sah'):
  stem=f'{mode}-{method}'
  ppm=(evidence/f'{stem}.ppm').read_bytes()
  magic,dimension,maximum,data=ppm.split(b'\n',3);w,h=map(int,dimension.split())
  assert magic==b'P6' and maximum==b'255' and (w,h)==(1280,960) and len(data)==3*w*h
  background=data[:3]
  foreground=sum(data[i:i+3]!=background for i in range(0,len(data),3))
  colors=len(set(data[i:i+3] for i in range(0,len(data),3)))
  assert 10000<foreground<1000000 and colors>20,(stem,foreground,colors)
  if baseline is None:baseline=ppm
  assert ppm==baseline,stem
  png(evidence/f'{stem}.png',w,h,data)
  images.append(dict(build=mode,method=method,width=w,height=h,foreground=foreground,colors=colors,sha256=hashlib.sha256(ppm).hexdigest()))
(evidence/'render-results.json').write_text(json.dumps(images,indent=2)+'\n')
rows=list(csv.DictReader((evidence/'benchmark.csv').open()))
assert len(rows)==6
summary={}
for method in ('naive','sah'):
 values=[r for r in rows if r['method']==method]
 assert [int(r['round']) for r in values]==[1,2,3]
 assert all(int(r['rays'])==192*144 and int(r['triangles'])>100 for r in values)
 summary[method]={metric:statistics.median(float(r[metric]) for r in values) for metric in ('build_ms','trace_ms')}
assert len({r['hits'] for r in rows})==1 and int(rows[0]['hits'])>100
assert max(float(r['distance_sum']) for r in rows)-min(float(r['distance_sum']) for r in rows)<1e-7
summary['trace_speedup_naive_over_sah']=summary['naive']['trace_ms']/summary['sah']['trace_ms']
summary['triangles']=int(rows[0]['triangles']);summary['rays']=int(rows[0]['rays']);summary['measured_rounds']=3
(evidence/'benchmark-summary.json').write_text(json.dumps(summary,indent=2)+'\n')
timing='\n## 本次远端实测（Release、三轮中位数）\n\n'
timing+='| 方法 | 构建 ms | 遍历 ms |\n|---|---:|---:|\n'
for method in ('naive','sah'):
 timing+=f"| {method} | {summary[method]['build_ms']:.3f} | {summary[method]['trace_ms']:.3f} |\n"
timing+=f"\n{summary['triangles']} 个三角形、{summary['rays']} 条固定射线；naive/SAH 遍历时间比 {summary['trace_speedup_naive_over_sah']:.3f}。构建更慢或遍历变慢也如实保留。\n"
(evidence/'timing.md').write_text(timing)
source=Path('hw6/Assignment6')
with zipfile.ZipFile(evidence/'edidada_Homework6.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in source.rglob('*'):
  relative=p.relative_to(source)
  if not p.is_file() or any(part.startswith('build') or part in ('.vs','.vscode') for part in relative.parts):continue
  if p.name=='README.md':z.writestr('edidada_Homework6/README.md',p.read_text()+timing)
  else:z.write(p,Path('edidada_Homework6')/relative)
 for method in ('naive','sah'):z.write(evidence/f'normal-{method}.png',f'edidada_Homework6/images/{method}.png')
 z.write(evidence/'benchmark.csv','edidada_Homework6/benchmark.csv')
with tempfile.TemporaryDirectory(prefix='hw6-package-') as tmp:
 root=Path(tmp)
 with zipfile.ZipFile(evidence/'edidada_Homework6.zip') as z:
  names=z.namelist()
  assert all(not any(p.startswith('build') or p in ('.vs','.vscode') for p in Path(name).parts) for name in names)
  assert all('edidada_Homework6/'+name in names for name in ('CMakeLists.txt','README.md','tests.cpp','models/bunny/bunny.obj','images/naive.png','images/sah.png'))
  z.extractall(root)
 source=root/'edidada_Homework6';build=source/'build'
 with (evidence/'package.log').open('w') as log:
  for command in (['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release'],['cmake','--build',str(build),'--parallel','2'],[str(build/'hw6_tests')]):
   subprocess.run(command,cwd=root,check=True,stdout=log,stderr=subprocess.STDOUT,timeout=180)
  for method in ('naive','sah'):
   subprocess.run([str(build/'RayTracing'),method],cwd=build,check=True,stdout=log,stderr=subprocess.STDOUT,timeout=180)
   assert (build/'binary.ppm').read_bytes()==baseline
(evidence/'package.result').write_text('PASS: clean ZIP builds, correctness passes, both complete PPMs match.\n')
print(json.dumps(summary,indent=2))
PY
