#!/usr/bin/env bash
set -euo pipefail
./build/Rasterizer > evidence/gui.stdout 2> evidence/gui.stderr &
app_pid=$!
trap 'kill "$app_pid" 2>/dev/null || true' EXIT
window=$(timeout 15 xdotool search --sync --onlyvisible --name '^image$' | head -n1)
xdotool windowfocus "$window"
sleep 1
import -window "$window" evidence/gui-initial.png
xdotool key --window "$window" a
sleep 1
import -window "$window" evidence/gui-a.png
xdotool key --window "$window" d
sleep 1
import -window "$window" evidence/gui-d.png
xdotool key --window "$window" Escape
wait "$app_pid"
trap - EXIT
