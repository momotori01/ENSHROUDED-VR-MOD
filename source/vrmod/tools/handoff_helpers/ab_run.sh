#!/bin/bash
# ab_run.sh <tag> <extra run_play_session args...>: grass walk + burst at 1750, then toggle metric.
H="$(cd "$(dirname "$0")" && pwd)"; SP="${SP:-$H/out}"; mkdir -p "$SP"
PW=pwsh
tag=$1;shift
cd /s/SteamLibrary/steamapps/common/Enshrouded
for i in $(seq 1 90); do tasklist //FI "IMAGENAME eq enshrouded.exe" | grep -q enshrouded || break; sleep 2; done
( "$H/burst_at.sh" "$SP/ab_$tag.txt" 30 10 > "$SP/ab_${tag}_burst.log" 2>&1 & )
$PW -NoProfile -File "$H/run_with_sound.ps1" -Script ./vrmod/tools/run_play_session.ps1 -ScriptArgs "-Minutes 2 -LiveInput -Hands ./vrmod/test_env/world-live-input-grass-walk-hands.csv -UiQuad $*" -Out "$SP/ab_$tag.txt"
for i in $(seq 1 90); do tasklist //FI "IMAGENAME eq enshrouded.exe" | grep -q enshrouded || break; sleep 2; done
G=$(grep -o 'game-2026[0-9-]*' "$SP/ab_$tag.txt" | head -1)
echo "== $tag $G $(grep -o '"status": "[A-Z]*"' $SP/ab_$tag.txt) $(grep -o '"pairs_per_s_last_third": [0-9.]*' $SP/ab_$tag.txt) $(cat $SP/ab_${tag}_burst.log)"
S:/stable/Python31011/python.exe "$H/toggles.py" vrmod/output/$G "$SP/ab_${tag}_heat.png"
