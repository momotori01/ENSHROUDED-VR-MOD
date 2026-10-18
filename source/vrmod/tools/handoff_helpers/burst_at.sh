#!/bin/bash
# burst_at.sh <run output txt> <seconds after the first live pair> [count]: request a consecutive burst.
f=$1;delay=$2;count=${3:-10}
until [ -f "$f" ] && grep -q '== game-' "$f"; do sleep 1; done
g=$(grep -o 'game-2026[0-9-]*' "$f" | head -1);d=/s/SteamLibrary/steamapps/common/Enshrouded/vrmod/output/$g
until [ "$(ls -d $d/view-resource-probe-* 2>/dev/null | wc -l)" -ge 2 ]; do sleep 0.5; done
p=$(ls -d $d/view-resource-probe-* | tail -1)
until grep -q '"live_native_pair_complete"' $p/view-resource-probe.jsonl 2>/dev/null; do sleep 0.5; done
start=$(date +%s); sleep $delay
echo $count > "$d/xr-burst.request"; echo "burst requested $(( $(date +%s)-start ))s after the first pair, at $(date +%H:%M:%S)"
for i in $(seq 1 40); do [ -f "$d/xr-burst.request" ] || break; sleep 0.5; done
grep -o '"event":"manual_xr_burst"[^}]*' $d/native-xr/openxr.jsonl | head -1
