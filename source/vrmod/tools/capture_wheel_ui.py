"""Read logged input frame and request native UI-layer captures at known wheel phases."""
import argparse,time,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('launch',type=Path);a=p.parse_args();deadline=time.monotonic()+220;pending=[(790,850),(1090,1150),(1390,1450)];requests=[]
while pending and time.monotonic()<deadline:
 probes=sorted(a.launch.glob('view-resource-probe-*'))
 if len(probes)<2:time.sleep(.5);continue
 path=probes[-1]/'view-resource-probe.jsonl'
 if not path.exists():time.sleep(.5);continue
 with path.open('rb') as f:f.seek(max(0,path.stat().st_size-1048576));lines=f.read().splitlines()
 frames=[]
 for l in lines:
  if b'"event":"controller_live_sample"' in l:
   try:frames.append(json.loads(l)['frame'])
   except ValueError:pass
 if frames:
  fr=frames[-1];lo,hi=pending[0]
  if fr>=lo:
   pending.pop(0)
   if fr<=hi and not (a.launch/'ui-capture.request').exists():
    (a.launch/'ui-capture.request').touch();requests.append(fr);print('UI capture request',fr,flush=True)
 time.sleep(.15)
(a.launch/'wheel-ui-requests.json').write_text(json.dumps(dict(frames=requests),indent=2)+'\n',encoding='utf-8');print('done',requests,flush=True)
