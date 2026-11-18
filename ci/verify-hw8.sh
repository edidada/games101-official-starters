#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
evidence="$(pwd)/evidence"
git rev-parse HEAD > "$evidence/commit.txt"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
for mode in normal sanitized;do
 sanitize=OFF
 if [ "$mode" = sanitized ];then sanitize=ON;fi
 cmake -S hw8 -B "build-$mode" -DCMAKE_BUILD_TYPE=Release -DHW8_GUI=OFF -DHW8_SANITIZE="$sanitize" 2>&1 | tee "$evidence/configure-$mode.log"
 cmake --build "build-$mode" --parallel 2 2>&1 | tee "$evidence/build-$mode.log"
 timeout 120 "./build-$mode/rope_tests" 2>&1 | tee "$evidence/tests-$mode.log"
 timeout 120 "./build-$mode/rope_trace" "$evidence/trace-$mode.csv"
done
cmp "$evidence/trace-normal.csv" "$evidence/trace-sanitized.csv"
# Reuse the canonical Ubuntu mirror and bounded network retries.
sudo sed -i -E 's|http://[^ /]*archive.ubuntu.com/ubuntu/?|https://archive.ubuntu.com/ubuntu/|g;s|http://security.ubuntu.com/ubuntu/?|https://security.ubuntu.com/ubuntu/|g' /etc/apt/sources.list.d/ubuntu.sources
sudo apt-get -o Acquire::Retries=2 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30 update
sudo apt-get -o Acquire::Retries=2 -o Acquire::http::Timeout=30 -o Acquire::https::Timeout=30 install -y --no-install-recommends libgl1-mesa-dev libfreetype6-dev libglew-dev libglfw3-dev xvfb xauth xdotool imagemagick python3-pil python3-matplotlib
cmake -S hw8 -B build-gui -DCMAKE_BUILD_TYPE=Release -DHW8_GUI=ON 2>&1 | tee "$evidence/configure-gui.log"
cmake --build build-gui --parallel 2 2>&1 | tee "$evidence/build-gui.log"
LIBGL_ALWAYS_SOFTWARE=1 timeout 40 xvfb-run -a bash ci/hw8-gui.sh "$(pwd)/build-gui/ropesim" "$evidence/gui"
for args in '-s 0' '-s 3x' '-m 0' '-g 0' '-f 0';do
 if ./build-gui/ropesim $args > "$evidence/invalid-${args// /_}.log" 2>&1;then echo 'Invalid CLI accepted' >&2;exit 1;fi
done
/usr/bin/python3 - <<'PY'
from pathlib import Path
import csv,hashlib,json,subprocess,tempfile,zipfile
from PIL import Image
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
root=Path('evidence');rows=list(csv.DictReader((root/'trace-normal.csv').open()))
assert len(rows)==384
methods=['explicit','semi','verlet','constraints'];times=[0,1,2,4,8,15]
for method in methods:
 for time in times:
  sample=[r for r in rows if r['method']==method and float(r['time'])==time]
  assert len(sample)==16
  pin=next(r for r in sample if r['node']=='0');assert (float(pin['x']),float(pin['y']))==(0,200)
# Fixed equal axes expose differences rather than rescaling each method.
fig,axes=plt.subplots(4,4,figsize=(12,10),sharex=True,sharey=True)
colors=['#e28520','#2364ce','#169952','#a23ab8']
for index,method in enumerate(methods):
 for col,time in enumerate([1,4,8,15]):
  ax=axes[index,col];sample=[r for r in rows if r['method']==method and float(r['time'])==time]
  ax.plot([float(r['x']) for r in sample],[float(r['y']) for r in sample],'-o',color=colors[index],markersize=3)
  ax.plot(0,200,'rs',markersize=4);ax.set_xlim(-450,450);ax.set_ylim(-300,260);ax.set_aspect('equal');ax.grid(alpha=.25)
  ax.set_title(f'{method}, t={time}s')
fig.suptitle('16 nodes; gravity=-9.8; dt=1/4096; pin 0; default damping')
fig.tight_layout();fig.savefig(root/'rope-comparison.png',dpi=140);plt.close(fig)
metrics={}
for stage in ['initial','constraints']:
 im=Image.open(root/f'gui-{stage}.png').convert('RGB');w,h=im.size
 crop=im.crop((0,70,w,h-40));pixels=list(crop.getdata())
 blue=sum(b>150 and r<100 and g<100 for r,g,b in pixels)
 green=sum(g>150 and r<100 and b<100 for r,g,b in pixels)
 assert blue>30 and green>30,(stage,blue,green)
 metrics[stage]={'size':[w,h],'rope_blue_pixels':blue,'rope_green_pixels':green}
assert Image.open(root/'gui-initial.png').tobytes()!=Image.open(root/'gui-constraints.png').tobytes()
checks=(root/'tests-normal.log').read_text()
assert all(x in checks for x in ['TOPOLOGY','STEP explicit=pass semi=pass verlet=pass','DAMPING','CONSTRAINT'])
results={'trace_rows':len(rows),'trace_cross_build_exact':True,'methods':methods,'times':times,'gui':metrics,'actual_key_sequence':['1','2','C','=','-','N','Escape'],'gui_exit_code':0}
(root/'results.json').write_text(json.dumps(results,indent=2)+'\n')
source=Path('hw8')
with zipfile.ZipFile(root/'edidada_Homework8.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in source.rglob('*'):
  relative=p.relative_to(source)
  if not p.is_file() or p.suffix=='.pdf' or any(part.startswith('build') or part in ('.vs','.vscode','__MACOSX') for part in relative.parts):continue
  z.write(p,Path('edidada_Homework8')/relative)
 for name in ('rope-comparison.png','gui-initial.png','gui-constraints.png'):z.write(root/name,'edidada_Homework8/images/'+name)
 z.write(root/'trace-normal.csv','edidada_Homework8/trace.csv')
with tempfile.TemporaryDirectory(prefix='hw8-package-') as tmp:
 dest=Path(tmp)
 with zipfile.ZipFile(root/'edidada_Homework8.zip') as z:
  names=z.namelist();assert all(not name.endswith('.pdf') and not any(p.startswith('build') or p in ('.vs','.vscode') for p in Path(name).parts) for name in names)
  assert all('edidada_Homework8/'+name in names for name in ('CMakeLists.txt','README.md','src/rope.cpp','images/gui-initial.png'))
  z.extractall(dest)
 source=dest/'edidada_Homework8';build=dest/'build'
 with (root/'package.log').open('w') as log:
  for command in (['cmake','-S',str(source),'-B',str(build),'-DCMAKE_BUILD_TYPE=Release','-DHW8_GUI=ON'],['cmake','--build',str(build),'--parallel','2'],[str(build/'rope_tests')],[str(build/'rope_trace'),str(dest/'trace.csv')]):
   subprocess.run(command,check=True,stdout=log,stderr=subprocess.STDOUT,timeout=180)
  assert (dest/'trace.csv').read_bytes()==(root/'trace-normal.csv').read_bytes()
  subprocess.run(['xvfb-run','-a','bash','ci/hw8-gui.sh',str(build/'ropesim'),str(root.resolve()/'package-gui')],check=True,stdout=log,stderr=subprocess.STDOUT,timeout=40)
(root/'package.result').write_text('PASS: clean ZIP, full GUI/numeric independent build, tests/trace exact, real window keys/Escape.\n')
print(json.dumps(results,indent=2))
PY
