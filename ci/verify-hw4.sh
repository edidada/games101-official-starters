#!/usr/bin/env bash
set -euo pipefail
mkdir -p evidence
git rev-parse HEAD > evidence/commit.txt
for mode in normal sanitized; do
 flags=''
 if [ "$mode" = sanitized ]; then flags='-fsanitize=address,undefined -fno-omit-frame-pointer'; fi
 cmake -S hw4/code -B "build-$mode" -DCMAKE_BUILD_TYPE=Release -DHW4_BUILD_CHECKS=ON -DCMAKE_CXX_FLAGS="$flags" -DCMAKE_EXE_LINKER_FLAGS="$flags"
 cmake --build "build-$mode" --parallel 2
 ASAN_OPTIONS=detect_leaks=0 "./build-$mode/hw4-checks" | tee "evidence/checks-$mode.txt"
 for render in green compare aa; do
  ASAN_OPTIONS=detect_leaks=0 "./build-$mode/BezierCurve" --render "evidence/$mode-$render.png" "$render" 80 550 180 80 500 120 620 550
 done
done
/usr/bin/python3 - <<'PY'
import cv2,numpy as np,json
from pathlib import Path
rows=[]
for mode in ['normal','sanitized']:
 a=cv2.imread(f'evidence/{mode}-green.png');b=cv2.imread(f'evidence/{mode}-aa.png');comparison=cv2.imread(f'evidence/{mode}-compare.png')
 green=(a[:,:,1]>0)&(a[:,:,0]==0)&(a[:,:,2]==0);assert green.sum()>500
 partial=(b[:,:,1]>0)&(b[:,:,1]<255)&(b[:,:,0]==0)&(b[:,:,2]==0);assert partial.sum()>500
 red=(comparison[:,:,2]>0)&(comparison[:,:,0]==0);g=(comparison[:,:,1]>0)&(comparison[:,:,0]==0)
 assert (red&g).sum()/max(red.sum(),g.sum())>.95
 rows.append({'build':mode,'green_pixels':int(green.sum()),'partial_aa_pixels':int(partial.sum()),'yellow_overlap':float((red&g).sum()/max(red.sum(),g.sum()))})
for render in ['green','aa','compare']:assert np.array_equal(cv2.imread(f'evidence/normal-{render}.png'),cv2.imread(f'evidence/sanitized-{render}.png'))
Path('evidence/render-results.json').write_text(json.dumps(rows,indent=2)+'\n')
PY
xvfb-run -a bash ci/gui-hw4.sh
/usr/bin/python3 - <<'PY'
from pathlib import Path
import cv2,numpy as np,json,zipfile,tempfile,subprocess
image=cv2.imread('my_bezier_curve.png');reference=cv2.imread('evidence/normal-green.png');assert np.array_equal(image,reference)
assert Path('evidence/gui.stdout').read_text().count('Left button')==4
Path('evidence/gui.result').write_text('PASS: four actual mouse clicks, generated image equals deterministic CLI image, exit 0.\n')
with zipfile.ZipFile('evidence/edidada_Homework4.zip','w',zipfile.ZIP_DEFLATED) as z:
 for p in Path('hw4/code').iterdir():
  if p.is_file():z.write(p,'edidada_Homework4/'+p.name)
 for mode in ['green','aa','compare']:z.write(f'evidence/normal-{mode}.png',f'edidada_Homework4/images/{mode}.png')
with tempfile.TemporaryDirectory(prefix='hw4-package-') as tmp:
 root=Path(tmp)
 with zipfile.ZipFile('evidence/edidada_Homework4.zip') as z:
  assert not any('/build/' in x or '.vscode' in x or x.endswith('.pdf') for x in z.namelist())
  z.extractall(root)
 subprocess.run(['cmake','-S',str(root/'edidada_Homework4'),'-B',str(root/'build')],check=True)
 subprocess.run(['cmake','--build',str(root/'build'),'--parallel','2'],check=True)
 subprocess.run([str(root/'build/BezierCurve'),'--render',str(root/'result.png'),'aa','80','550','180','80','500','120','620','550'],check=True)
 assert np.array_equal(cv2.imread(str(root/'result.png')),cv2.imread('evidence/normal-aa.png'))
Path('evidence/package.result').write_text('PASS: clean source ZIP with images/ and README, independent build reproduces AA image.\n')
PY
