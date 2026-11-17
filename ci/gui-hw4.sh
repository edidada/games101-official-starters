#!/usr/bin/env bash
set -euo pipefail
./build-normal/BezierCurve > evidence/gui.stdout 2> evidence/gui.stderr &
app_pid=$!
trap 'kill "$app_pid" 2>/dev/null || true' EXIT
window=$(timeout 15 xdotool search --sync --onlyvisible --name '^Bezier Curve$' | head -n1)
xdotool windowfocus "$window"
sleep 1
# OpenCV Qt may center the 700px image in a wider toolbar window. Find its
# black canvas bounds from a screenshot before translating image coordinates.
import -window "$window" evidence/gui-empty.png
read -r offset_x offset_y < <(/usr/bin/python3 - <<'PY'
import cv2,numpy as np
im=cv2.imread('evidence/gui-empty.png');black=np.all(im==0,axis=2).astype(np.uint8)
_,_,stats,_=cv2.connectedComponentsWithStats(black)
x,y,w,h,area=max(stats[1:],key=lambda row:row[4])
assert (w,h,area)==(700,700,490000),(x,y,w,h,area)
print(x,y)
PY
)
for xy in '80 550' '180 80' '500 120' '620 550'; do
 read -r x y <<< "$xy"
 xdotool mousemove --window "$window" "$((x+offset_x))" "$((y+offset_y))" click 1
 sleep .3
done
sleep 1
import -window "$window" evidence/gui-curve.png
xdotool key --window "$window" Escape
wait "$app_pid"
trap - EXIT
