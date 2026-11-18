#!/usr/bin/env bash
set -euo pipefail
binary="$1"
prefix="$2"
"$binary" -s 32 -f 40 > "$prefix.log" 2>&1 &
pid=$!
trap 'kill "$pid" 2>/dev/null || true' EXIT
window=''
for _ in $(seq 1 120);do
 window=$(xdotool search --pid "$pid" --name 'Rope Simulator' 2>/dev/null | head -n 1 || true)
 if [ -n "$window" ] && grep -q 'FREEZE frames=40' "$prefix.log";then break;fi
 sleep 0.1
done
test -n "$window"
grep -q 'FREEZE frames=40' "$prefix.log"
xdotool windowfocus --sync "$window"
sleep 0.2
import -window "$window" "$prefix-initial.png"
xdotool key --window "$window" 1 2 c equal minus n
sleep 0.3
import -window "$window" "$prefix-constraints.png"
xdotool key --window "$window" Escape
wait "$pid"
trap - EXIT
for text in 'KEY 1' 'KEY 2' 'KEY C' 'KEY =' 'KEY -' 'KEY N';do grep -q "$text" "$prefix.log";done
grep -q 'steps=64' "$prefix.log"
grep -q 'steps=32' "$prefix.log"
grep -q 'green=constraint' "$prefix.log"
printf '%s\n' 'PASS: real window, freeze40, 1/2/C/+/-/N and Escape exit0.' > "$prefix.result"
