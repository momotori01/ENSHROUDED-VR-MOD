#!/bin/bash
# frame_caps.sh <run output txt> f1 f2 ...: request a manual XR capture when the live hand frame reaches each fi.
f=$1;shift
until [ -f "$f" ] && grep -q 'session running' "$f"; do sleep 1; done
g=$(grep -o 'game-2026[0-9-]*' "$f" | head -1);d=/s/SteamLibrary/steamapps/common/Enshrouded/vrmod/output/$g
until [ "$(ls -d $d/view-resource-probe-* 2>/dev/null | wc -l)" -ge 2 ]; do sleep 0.5; done
p=$(ls -d $d/view-resource-probe-* | tail -1)
for target in "$@"; do
  while true; do fr=$(grep -o '"event":"controller_live_sample","frame":[0-9]*' $p/view-resource-probe.jsonl 2>/dev/null | tail -1 | grep -o '[0-9]*$'); [ -n "$fr" ] && [ "$fr" -ge "$target" ] && break; sleep 0.1; done
  touch "$d/xr-capture.request"; while [ -f "$d/xr-capture.request" ]; do sleep 0.05; done; echo "capture at frame>=$target (seen $fr)"
done
