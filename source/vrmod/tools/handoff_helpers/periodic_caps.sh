#!/bin/bash
# periodic_caps.sh <run txt> <start s after first pair> <interval s> <count>: XR single captures over time.
f=$1;start=$2;iv=$3;n=$4
until [ -f "$f" ] && grep -q '== game-' "$f"; do sleep 1; done
g=$(grep -o 'game-2026[0-9-]*' "$f" | head -1);d=/s/SteamLibrary/steamapps/common/Enshrouded/vrmod/output/$g
until [ "$(ls -d $d/view-resource-probe-* 2>/dev/null | wc -l)" -ge 2 ]; do sleep 0.5; done
p=$(ls -d $d/view-resource-probe-* | tail -1)
until grep -q '"live_native_pair_complete"' $p/view-resource-probe.jsonl 2>/dev/null; do sleep 0.5; done
sleep $start
for i in $(seq 1 $n); do touch "$d/xr-capture.request"; sleep $iv; done
echo "periodic captures requested $n"
