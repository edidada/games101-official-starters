#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
evidence="$(pwd)/evidence"
source="$(pwd)/hw7/Assignment7"
git rev-parse HEAD > "$evidence/commit.txt"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
render() {
 local mode="$1" name="$2";shift 2
 (cd "$source/build-$mode"; timeout 300 ./RayTracing "$@" --output "$evidence/$name.ppm") > "$evidence/$name.log" 2>&1
}
for mode in normal sanitized; do
 sanitize=OFF
 if [ "$mode" = sanitized ];then sanitize=ON;fi
 cmake -S "$source" -B "$source/build-$mode" -DCMAKE_BUILD_TYPE=Release -DHW7_SANITIZE="$sanitize" 2>&1 | tee "$evidence/configure-$mode.log"
 cmake --build "$source/build-$mode" --parallel 2 2>&1 | tee "$evidence/build-$mode.log"
 timeout 180 "$source/build-$mode/hw7_tests" 2>&1 | tee "$evidence/checks-$mode.log"
 for threads in 1 4; do
  render "$mode" "check-$mode-t$threads" --size 128 --spp 8 --threads "$threads"
 done
done
# Same image/work, same runner, three rounds with alternating method order.
for round in 1 2 3; do
 order='1 4'
 if [ "$round" = 2 ];then order='4 1';fi
 for threads in $order;do
  render normal "diffuse-r$round-t$threads" --size 512 --spp 8 --threads "$threads"
 done
done
for roughness in 0.25 0.65;do
 render normal "metal-$roughness" --size 512 --spp 32 --threads 4 --material metal --roughness "$roughness"
done
render normal indirect --size 128 --spp 64 --threads 4
render normal direct --size 128 --spp 64 --threads 4 --rr 0
# Exercise input failures without building/running locally.
for args in '--threads 0' '--spp 0' '--size -1' '--roughness nan' '--rr 1' '--material invalid' '--size 128x';do
 if "$source/build-normal/RayTracing" $args > "$evidence/invalid-${args// /_}.log" 2>&1;then
  echo "Invalid CLI accepted: $args" >&2;exit 1
 fi
done
python3 - <<'PY'
from pathlib import Path
import array,hashlib,json,re,statistics,struct,subprocess,tempfile,zipfile,zlib
root=Path('evidence')
def read(name):
 ppm=(root/f'{name}.ppm').read_bytes();magic,dimensions,maximum,data=ppm.split(b'\n',3)
 w,h=map(int,dimensions.split());assert magic==b'P6' and maximum==b'255' and len(data)==3*w*h
 hdr=(root/f'{name}.ppm.linear').read_bytes();assert len(hdr)==12*w*h
 values=array.array('f');values.frombytes(hdr);assert all(0<=x<float('inf') for x in values)
 return ppm,w,h,data,values

def png(name,w,h,data):
 def chunk(k,b):return struct.pack('!I',len(b))+k+b+struct.pack('!I',zlib.crc32(k+b)&0xffffffff)
 raw=b''.join(b'\0'+data[y*w*3:(y+1)*w*3] for y in range(h))
 (root/f'{name}.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))

small=[read(f'check-{mode}-t{threads}')[0] for mode in ('normal','sanitized') for threads in (1,4)]
assert all(p==small[0] for p in small)
linear=[(root/f'check-{mode}-t{threads}.ppm.linear').read_bytes() for mode in ('normal','sanitized') for threads in (1,4)]
assert all(x==linear[0] for x in linear)
rows=[];baseline=None;baseline_hdr=None
for round in (1,2,3):
 for threads in (1,4):
  name=f'diffuse-r{round}-t{threads}';ppm,w,h,data,values=read(name)
  assert (w,h)==(512,512)
  hdr=(root/f'{name}.ppm.linear').read_bytes()
  if baseline is None:baseline=ppm;baseline_hdr=hdr
  assert ppm==baseline and hdr==baseline_hdr
  text=(root/f'{name}.log').read_text()
  record={key:float(value) for key,value in re.findall(r'(width|height|spp|threads|peak_workers|seed|render_ms)=([0-9.eE+-]+)',text)}
  assert record['spp']==8 and record['threads']==threads
  assert record['peak_workers']>1 if threads==4 else record['peak_workers']==1
  record['round']=round;rows.append(record)
summary={str(t):statistics.median(r['render_ms'] for r in rows if r['threads']==t) for t in (1,4)}
summary['time_ratio_1_over_4']=summary['1']/summary['4']
results={'render_rounds':rows,'render_medians_ms':summary,'small_cross_build_and_threads_exact':True,'full_ppm_and_linear_cross_threads_exact':True,'diffuse_size':[512,512],'diffuse_spp':8,'diffuse_sha256':hashlib.sha256(baseline).hexdigest()}
# Keep display images for all official/full renders, and verify visible ceiling light.
for name in ('diffuse-r1-t4','metal-0.25','metal-0.65','indirect','direct'):
 ppm,w,h,data,values=read(name);png(name,w,h,data)
 assert sum(x>0.02 for x in values)>w*h
 if name.startswith('metal'):assert (w,h)==(512,512)
 if name=='diffuse-r1-t4':
  bright=sum(all(values[3*i+c]>5 for c in range(3)) for i in range(w*h))
  assert bright>100,'camera-visible emitter missing'
  results['visible_emitter_pixels']=bright
sharp=read('metal-0.25');rough=read('metal-0.65')
changed=sum(sharp[3][i:i+3]!=rough[3][i:i+3] for i in range(0,len(sharp[3]),3));assert changed>1000
results['roughness_changed_pixels']=changed
full=read('indirect')[4];direct=read('direct')[4]
mask=[i for i in range(128*128) if max(full[3*i:3*i+3])<4 and max(direct[3*i:3*i+3])<4]
full_mean=sum(full[3*i+c] for i in mask for c in range(3))/(3*len(mask))
direct_mean=sum(direct[3*i+c] for i in mask for c in range(3))/(3*len(mask))
assert full_mean>direct_mean*1.05,(full_mean,direct_mean)
shadow_gain=sum(sum(direct[3*i:3*i+3])<0.03 and sum(full[3*i:3*i+3])>0.09 for i in mask)
assert shadow_gain>100,shadow_gain
results['indirect_comparison']={'size':[128,128],'spp':64,'nonemitter_mean_full':full_mean,'nonemitter_mean_direct':direct_mean,'shadow_gain_pixels':shadow_gain}
(root/'render-results.json').write_text(json.dumps(results,indent=2)+'\n')
timing='\n## 当次 Actions 结果\n\n'
timing+='Diffuse：512×512、8spp、seed=7，三轮按1/4、4/1、1/4线程顺序。完整PPM和线性float图均字节一致。\n\n'
timing+=f"单线程中位数 {summary['1']:.3f}ms；四线程 {summary['4']:.3f}ms；时间比 {summary['time_ratio_1_over_4']:.3f}。只代表当前runner，不保证加速。\n\n"
for roughness in ('0.25','0.65'):
 log=(root/f'metal-{roughness}.log').read_text();ms=re.search(r'render_ms=([0-9.eE+-]+)',log).group(1)
 timing+=f'Microfacet roughness={roughness}：512×512、32spp、4线程、seed=7，{ms}ms。\n\n'
timing+=f'间接光验证：128×128、64spp，非光源区域均值 full={full_mean:.6f} / direct={direct_mean:.6f}。\n'
(root/'timing.md').write_text(timing)
source=Path('hw7/Assignment7')
with zipfile.ZipFile(root/'edidada_Homework7.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in source.rglob('*'):
  relative=p.relative_to(source)
  if not p.is_file() or any(part.startswith('build') or part in ('.vs','.vscode') for part in relative.parts):continue
  if p.name=='README.md':z.writestr('edidada_Homework7/README.md',p.read_text()+timing)
  else:z.write(p,Path('edidada_Homework7')/relative)
 for name in ('diffuse-r1-t4','metal-0.25','metal-0.65'):z.write(root/f'{name}.png',f'edidada_Homework7/images/{name}.png')
 z.write(root/'render-results.json','edidada_Homework7/render-results.json')
with tempfile.TemporaryDirectory(prefix='hw7-package-') as tmp:
 dest=Path(tmp)
 with zipfile.ZipFile(root/'edidada_Homework7.zip') as z:
  assert all(not any(p.startswith('build') or p in ('.vs','.vscode') for p in Path(name).parts) for name in z.namelist())
  assert 'edidada_Homework7/images/metal-0.65.png' in z.namelist();z.extractall(dest)
 source=dest/'edidada_Homework7';build=source/'build'
 with (root/'package.log').open('w') as log:
  for cmd in (['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release'],['cmake','--build',str(build),'--parallel','2'],[str(build/'hw7_tests')]):
   subprocess.run(cmd,check=True,cwd=dest,stdout=log,stderr=subprocess.STDOUT,timeout=180)
  for material,spp,roughness,expected in [('diffuse',8,'0.35','diffuse-r1-t4'),('metal',32,'0.65','metal-0.65')]:
   subprocess.run([str(build/'RayTracing'),'--size','512','--spp',str(spp),'--threads','4','--material',material,'--roughness',roughness],cwd=build,check=True,stdout=log,stderr=subprocess.STDOUT,timeout=300)
   assert (build/'binary.ppm').read_bytes()==(root/f'{expected}.ppm').read_bytes()
   assert (build/'binary.ppm.linear').read_bytes()==(root/f'{expected}.ppm.linear').read_bytes()
(root/'package.result').write_text('PASS: clean source ZIP, independent build/tests, full diffuse and microfacet PPM/HDR reproduction.\n')
print(json.dumps(results,indent=2))
PY
