"""Request bounded XR evidence at specified replay frames; never changes inputs."""
import argparse,json,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('launch',type=Path);p.add_argument('--burst',type=int,default=2600);p.add_argument('--frames',default='1700,2000,2300,3000,3600,4000');p.add_argument('--timeout',type=int,default=300);a=p.parse_args()
pending=sorted(int(x) for x in a.frames.split(','));events=[];burst=False;offset=0;fragment=b'';deadline=time.monotonic()+a.timeout
log=a.launch/'native-xr/openxr.jsonl'
while time.monotonic()<deadline:
 if (a.launch/'process-exit.json').exists():
  raise RuntimeError('Game exited before requested evidence; retain the failed launch logs')
 if not log.exists():time.sleep(.2);continue
 with log.open('rb') as f:
  f.seek(offset);data=fragment+f.read();offset=f.tell()
 lines=data.split(b'\n');fragment=lines.pop();frame=0;closed=False
 for line in lines:
  try:e=json.loads(line)
  except ValueError:continue
  frame=max(frame,e.get('frame',0));closed|=e.get('event')=='native_xr_closed'
 if pending and frame>=pending[0] and not (a.launch/'xr-capture.request').exists():
  requested=pending.pop(0);(a.launch/'xr-capture.request').touch();events.append(dict(kind='single',requested=requested,observed=frame));print(events[-1],flush=True)
 if not burst and frame>=a.burst and not (a.launch/'xr-burst.request').exists():
  (a.launch/'xr-burst.request').write_text('32');burst=True;events.append(dict(kind='burst',count=32,requested=a.burst,observed=frame));print(events[-1],flush=True)
 if closed or (not pending and burst):break
 time.sleep(.1)
(a.launch/'replay-image-requests.json').write_text(json.dumps(dict(requests=events,missed=pending,burst_requested=burst),indent=2)+'\n',encoding='utf-8')
