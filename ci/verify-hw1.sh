#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
source_dir=$(dirname "$(find hw1 -name main.cpp -print -quit)")
git rev-parse HEAD > evidence/commit.txt
cmake -S "$source_dir" -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
./build/hw1-checks | tee evidence/invariants.txt
cmake -S "$source_dir" -B build-sanitized -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
cmake --build build-sanitized --parallel 2
ASAN_OPTIONS=detect_leaks=0 ./build-sanitized/hw1-checks | tee evidence/invariants-sanitized.txt
ASAN_OPTIONS=detect_leaks=0 /usr/bin/python3 - <<'PY'
from pathlib import Path
import cv2,numpy as np,subprocess,math,json
results=[]
for build in ['build','build-sanitized']:
 for angle in [0,10,20,90,-30,180,360]:
  path=Path('evidence')/f'{build}-{angle}.png'
  subprocess.run([f'./{build}/Rasterizer','-r',str(angle),str(path)],check=True,timeout=20)
  image=cv2.imread(str(path));assert image.shape==(700,700,3)
  assert np.all((image==0)|(image==255)) and np.array_equal(image[:,:,0],image[:,:,1]) and np.array_equal(image[:,:,0],image[:,:,2])
  yy,xx=np.where(image[:,:,0]>0);points=np.column_stack([xx,yy]);assert 800<len(points)<2500
  a=math.radians(angle);c,s=math.cos(a),math.sin(a);scale=350/(7*math.tan(math.radians(22.5)))
  vertices=np.array([[350+scale*(c*x-s*y),699-(350+scale*(s*x+c*y))] for x,y in [(2,0),(0,2),(-2,0)]])
  distances=[]
  for u,v in zip(vertices,np.roll(vertices,-1,axis=0)):
   direction=v-u;t=np.clip((points-u)@direction/(direction@direction),0,1)
   distances.append(np.linalg.norm(points-(u+t[:,None]*direction),axis=1))
  assert np.max(np.min(distances,axis=0))<2.1,(build,angle)
  for v in vertices:assert np.min(np.linalg.norm(points-v,axis=1))<2.1,(build,angle,v)
  results.append({'build':build,'angle':angle,'white_pixels':len(points),'max_distance_to_analytic_edge':float(np.max(np.min(distances,axis=0)))})
 assert np.array_equal(cv2.imread(f'evidence/{build}-0.png'),cv2.imread(f'evidence/{build}-360.png'))
subprocess.run(['./build/Rasterizer','-r','20'],check=True,timeout=20)
assert np.array_equal(cv2.imread('output.png'),cv2.imread('evidence/build-20.png'))
for args in [['-r'],['-x','20'],['-r','no'],['-r','nan'],['-r','inf'],['-r','20junk'],['-r','1','a','b']]:
 q=subprocess.run(['./build/Rasterizer',*args],capture_output=True,text=True,timeout=10)
 assert q.returncode==1 and q.stderr,(args,q.returncode,q.stderr)
Path('evidence/render-results.json').write_text(json.dumps({'images':results,'default_output':True,'invalid_cli':7},indent=2)+'\n')
PY
xvfb-run -a bash ci/gui-hw1.sh
/usr/bin/python3 - <<'PY'
from pathlib import Path
import cv2,json,numpy as np
rows=[]
for state,angle in [('initial',0),('a',10),('d',0)]:
 image=cv2.imread(f'evidence/gui-{state}.png');reference=cv2.imread(f'evidence/build-{angle}.png')
 match=cv2.matchTemplate(image,reference,cv2.TM_SQDIFF_NORMED);minimum,_,where,_=cv2.minMaxLoc(match)
 assert minimum<.00001,(state,minimum,image.shape)
 rows.append({'state':state,'angle':angle,'match_error':minimum,'offset':where})
assert 'frame count:' in Path('evidence/gui.stdout').read_text()
Path('evidence/gui-results.json').write_text(json.dumps({'frames':rows,'esc_exit':0},indent=2)+'\n')
PY
