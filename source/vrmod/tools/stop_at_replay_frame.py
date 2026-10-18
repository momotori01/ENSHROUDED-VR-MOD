"""Request managed normal shutdown at an observed Mock frame, preserving evidence."""
import argparse,json,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--frame',type=int,required=True);p.add_argument('--timeout',type=int,default=300);a=p.parse_args()
assert 800<=a.frame<=20000
log=a.launch/'native-xr/openxr.jsonl';offset=0;fragment=b'';deadline=time.monotonic()+a.timeout
while time.monotonic()<deadline:
 if (a.launch/'process-exit.json').exists():
  raise RuntimeError('Game exited before requested evidence; retain the failed launch logs')
 if not log.exists():time.sleep(.1);continue
 with log.open('rb') as f:f.seek(offset);data=fragment+f.read();offset=f.tell()
 lines=data.split(b'\n');fragment=lines.pop();frame=0;closed=False
 for line in lines:
  try:e=json.loads(line)
  except ValueError:continue
  frame=max(frame,e.get('frame',0));closed|=e.get('event')=='native_xr_closed'
 if closed:raise RuntimeError('XR already closed before requested frame')
 if frame>=a.frame:
  probes=sorted(a.launch.glob('view-resource-probe-*/world-session.txt'));assert len(probes)==1
  dest=probes[0].parent/'stop.request';dest.touch(exist_ok=False)
  r=dict(requested=a.frame,observed=frame,stop_file=str(dest),normal_shutdown_requested=True)
  (a.launch/'frame-stop-request.json').write_text(json.dumps(r,indent=2)+'\n');print(r);break
 time.sleep(.1)
else:raise RuntimeError('Frame stop timed out; runner retains its normal time limit')
