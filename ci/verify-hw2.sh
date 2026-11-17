#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
git rev-parse HEAD > evidence/commit.txt
for mode in normal sanitized; do
 flags=''
 if [ "$mode" = sanitized ]; then flags='-fsanitize=address,undefined -fno-omit-frame-pointer'; fi
 cmake -S hw2/代码框架 -B "build-$mode" -DCMAKE_BUILD_TYPE=Debug -DHW2_BUILD_CHECKS=ON -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_EXE_LINKER_FLAGS="$flags"
 cmake --build "build-$mode" --parallel 2
 ASAN_OPTIONS=detect_leaks=0 "./build-$mode/Rasterizer" "evidence/$mode-single.png"
 ASAN_OPTIONS=detect_leaks=0 "./build-$mode/Rasterizer" "evidence/$mode-ssaa.png" --ssaa
done
ASAN_OPTIONS=detect_leaks=0 /usr/bin/python3 - <<'PY'
from pathlib import Path
import subprocess,json,itertools,numpy as np,cv2
red=[255,0,0];green=[0,255,0];blue=[0,0,255]
def tri(points,color):return {'points':points,'color':color}
a=[[8,7,-.5],[55,11,-.5],[19,54,-.5]]
b=[[7,18,.5],[53,8,.5],[51,56,.5]]
scenes={
 'single':[tri(a,red)],'reverse-winding':[tri(a[::-1],red)],
 'overlap':[tri(b,blue),tri(a,red)],'overlap-reverse':[tri(a,red),tri(b,blue)],
 'slope':[tri([[8,7,-.8],[55,11,.8],[19,54,.1]],red),tri([[7,18,.3],[53,8,-.2],[51,56,-.4]],blue)],
 'square':[tri([[16,16,0],[48,16,0],[48,48,0]],green),tri([[16,16,0],[48,48,0],[16,48,0]],green)],
 'offscreen':[tri([[-20,0,0],[40,20,0],[10,90,0]],red)],
 'degenerate':[tri([[5,5,0],[20,20,0],[50,50,0]],red)],
 'outside':[tri([[-30,5,0],[-10,10,0],[-5,50,0]],red)],
 'thin':[tri([[2.1,2.2,-.1],[61.7,60.2,-.1],[60.2,60.9,-.1]],blue)]}
scenes['slope-reverse']=scenes['slope'][::-1]
scenes['square-reverse']=scenes['square'][::-1]
# Independent ray crossing for coverage, plane fit for depth. Samples exactly on
# an edge are checked separately using the watertight square, not this oracle.
def reference(scene,samples):
 offsets=[(.5,.5)] if samples==1 else [(.25,.25),(.75,.25),(.25,.75),(.75,.75)]
 colors=np.zeros((64,64,samples,3));depth=np.full((64,64,samples),np.inf);valid=np.ones((64,64),bool)
 yy,xx=np.mgrid[0:64,0:64];yy=63-yy
 for t in scene:
  vertices=np.array(t['points'],float);matrix=np.column_stack([vertices[:,:2],np.ones(3)])
  if abs(np.linalg.det(matrix))<1e-9:continue
  plane=np.linalg.solve(matrix,vertices[:,2])
  for sample,(ox,oy) in enumerate(offsets):
   x=xx+ox;y=yy+oy;inside=np.zeros((64,64),bool);boundary=np.zeros((64,64),bool)
   for u,v in zip(vertices,np.roll(vertices,-1,axis=0)):
    dx,dy=v[:2]-u[:2]
    boundary|=(abs(dx*(y-u[1])-dy*(x-u[0]))<1e-4)&(x>=min(u[0],v[0])-1e-4)&(x<=max(u[0],v[0])+1e-4)&(y>=min(u[1],v[1])-1e-4)&(y<=max(u[1],v[1])+1e-4)
    if dy:inside^=((u[1]>y)!=(v[1]>y))&(x<(u[0]+(y-u[1])*dx/dy))
   valid&=~boundary
   z=plane[0]*x+plane[1]*y+plane[2];closer=inside&(z<depth[:,:,sample])
   depth[:,:,sample][closer]=z[closer];colors[:,:,sample][closer]=t['color']
 return colors.mean(axis=2),valid
rows=[];images={}
for build in ['normal','sanitized']:
 for samples in [1,4]:
  for name,scene in scenes.items():
   stem=f'{build}-{samples}-{name}';path=Path('evidence')/(stem+'.bin')
   text=str(len(scene))+'\n'+'\n'.join(' '.join(map(str,t['color']+[c for p in t['points'] for c in p])) for t in scene)+'\n'
   subprocess.run([f'./build-{build}/hw2-scenes','64','64',str(samples),str(path)],input=text,text=True,check=True,timeout=20)
   got=np.fromfile(path,dtype=np.float32).reshape(64,64,3);expected,valid=reference(scene,samples)
   assert valid.sum()>3900,(name,valid.sum())
   assert np.max(np.abs(got[valid]-expected[valid]))<.001,(stem,np.max(np.abs(got[valid]-expected[valid])))
   if name.startswith('square'):assert np.all(got[16:48,16:48]==green),(stem,'shared-edge crack')
   cv2.imwrite(str(path.with_suffix('.png')),np.rint(got[:,:,::-1]).astype(np.uint8))
   images[build,samples,name]=got
   rows.append({'build':build,'samples':samples,'scene':name,'oracle_pixels':int(valid.sum())})
  for x,y in [('single','reverse-winding'),('overlap','overlap-reverse'),('slope','slope-reverse'),('square','square-reverse')]:assert np.array_equal(images[build,samples,x],images[build,samples,y]),(build,samples,x,y)
  if samples==4:assert np.any((images[build,samples,'single'][:,:,0]>0)&(images[build,samples,'single'][:,:,0]<255))
for samples in [1,4]:
 for name in scenes:assert np.array_equal(images['normal',samples,name],images['sanitized',samples,name])
for mode in ['single','ssaa']:
 a=cv2.imread(f'evidence/normal-{mode}.png');b=cv2.imread(f'evidence/sanitized-{mode}.png')
 assert a.shape==(700,700,3) and np.array_equal(a,b) and np.count_nonzero(a)>10000
assert not np.array_equal(cv2.imread('evidence/normal-single.png'),cv2.imread('evidence/normal-ssaa.png'))
Path('evidence/scenes.json').write_text(json.dumps(scenes,indent=2)+'\n')
Path('evidence/results.json').write_text(json.dumps({'scene_checks':rows,'draw_order_winding_checks':True,'per_sample_depth_and_resolve':True,'official_images':4,'leak_detection':False},indent=2)+'\n')
PY
