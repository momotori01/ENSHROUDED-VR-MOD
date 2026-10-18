#!/bin/bash
# desk_caps.sh <run output txt> <seconds after first live pair> <count>: desktop present captures (what the desktop shows).
f=$1;delay=$2;count=$3
until [ -f "$f" ] && grep -q '== game-' "$f"; do sleep 1; done
g=$(grep -o 'game-2026[0-9-]*' "$f" | head -1);d=/s/SteamLibrary/steamapps/common/Enshrouded/vrmod/output/$g
until [ "$(ls -d $d/view-resource-probe-* 2>/dev/null | wc -l)" -ge 2 ]; do sleep 0.5; done
p=$(ls -d $d/view-resource-probe-* | tail -1)
until grep -q '"live_native_pair_complete"' $p/view-resource-probe.jsonl 2>/dev/null; do sleep 0.5; done
sleep $delay
for i in $(seq 1 $count); do touch "$d/capture.request"; for k in $(seq 1 100); do [ -f "$d/capture.request" ] || break; sleep 0.05; done; done
echo "desktop captures: $(ls $d | grep -c '^game_.*ppm')"
