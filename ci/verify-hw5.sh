#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
git rev-parse HEAD > evidence/commit.txt
for mode in normal sanitized; do
 flags=''
 if [ "$mode" = sanitized ]; then flags='-fsanitize=address,undefined -fno-omit-frame-pointer'; fi
 cmake -S hw5/Code -B "build-$mode" -DCMAKE_BUILD_TYPE=Release -DHW5_BUILD_CHECKS=ON -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_EXE_LINKER_FLAGS="$flags"
 cmake --build "build-$mode" --parallel 2
 "./build-$mode/hw5-checks" | tee "evidence/checks-$mode.txt"
 timeout 120 "./build-$mode/RayTracing" > "evidence/render-$mode.log"
 mv binary.ppm "evidence/$mode.ppm"
done
python3 - <<'PY'
from pathlib import Path
import struct,zlib,json,zipfile,tempfile,subprocess

def png(path,w,h,data):
 def chunk(kind,body):return struct.pack('!I',len(body))+kind+body+struct.pack('!I',zlib.crc32(kind+body)&0xffffffff)
 raw=b''.join(b'\0'+data[y*w*3:(y+1)*w*3] for y in range(h))
 Path(path).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('!IIBBBBB',w,h,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
rows=[];buffers=[]
for mode in ['normal','sanitized']:
 ppm=Path(f'evidence/{mode}.ppm').read_bytes();magic,dim,maxv,data=ppm.split(b'\n',3);w,h=map(int,dim.split())
 assert magic==b'P6' and (w,h)==(1280,960) and maxv==b'255' and len(data)==w*h*3
 background=data[:3];nonbackground=sum(data[i:i+3]!=background for i in range(0,len(data),3));assert nonbackground>100000
 # Distinct foreground in both sphere regions and the checkerboard floor.
 counts=[]
 for x0,y0,x1,y1 in [(430,400,600,540),(600,430,790,590),(300,650,980,850)]:
  n=sum(data[3*(y*w+x):3*(y*w+x)+3]!=background for y in range(y0,y1) for x in range(x0,x1));assert n>1000;counts.append(n)
 png(f'evidence/{mode}.png',w,h,data);buffers.append(data)
 rows.append({'build':mode,'size':[w,h],'non_background':nonbackground,'sphere_floor_region_counts':counts})
assert buffers[0]==buffers[1]
Path('evidence/render-results.json').write_text(json.dumps(rows,indent=2)+'\n')
images=[]
for mode in ['normal','sanitized']:
 raw=Path(f'evidence/{mode}.ppm').read_bytes();magic,dimensions,maximum,data=raw.split(b'\n',3)
 w,h=map(int,dimensions.split());assert magic==b'P6' and maximum==b'255' and (w,h)==(1280,960) and len(data)==w*h*3
 background=data[:3];foreground=sum(data[j:j+3]!=background for j in range(0,len(data),3));assert foreground>50000
 def pixel(x,y):return tuple(data[(y*w+x)*3:(y*w+x+1)*3])
 # Fixed diagnostic regions on the visible diffuse sphere, refractive sphere
 # and checkerboard floor. Preserve samples for visual review as well.
 probes={name:pixel(x,y) for name,x,y in [('left_sphere',550,470),('right_sphere',700,540),('floor',600,700)]}
 assert all(bytes(value)!=background for value in probes.values()),probes
 png(Path(f'evidence/{mode}.png'),w,h,data)
 images.append({'build':mode,'width':w,'height':h,'foreground_pixels':foreground,'probes':probes})
assert Path('evidence/normal.ppm').read_bytes()==Path('evidence/sanitized.ppm').read_bytes()
Path('evidence/render-results.json').write_text(json.dumps(images,indent=2)+'\n')
with zipfile.ZipFile('evidence/edidada_Homework5.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in Path('hw5/Code').iterdir():
  if p.is_file():z.write(p,'edidada_Homework5/'+p.name)
 z.write('evidence/normal.png','edidada_Homework5/images/result.png')
with tempfile.TemporaryDirectory(prefix='hw5-package-') as tmp:
 root=Path(tmp)
 with zipfile.ZipFile('evidence/edidada_Homework5.zip') as z:
  assert not any('/build/' in name or '.vscode' in name or name.endswith('.pdf') for name in z.namelist());z.extractall(root)
 subprocess.run(['cmake','-S',str(root/'edidada_Homework5'),'-B',str(root/'build'),'-DCMAKE_BUILD_TYPE=Release'],check=True)
 subprocess.run(['cmake','--build',str(root/'build'),'--parallel','2'],check=True)
 subprocess.run([str(root/'build/RayTracing')],cwd=root,check=True,stdout=subprocess.DEVNULL,timeout=120)
 assert (root/'binary.ppm').read_bytes()==Path('evidence/normal.ppm').read_bytes()
Path('evidence/package.result').write_text('PASS: source ZIP includes images/ and README; clean independent build renders identical complete PPM.\n')
PY
