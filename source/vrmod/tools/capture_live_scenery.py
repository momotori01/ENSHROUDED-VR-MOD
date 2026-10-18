"""Capture occasional submitted XR pairs during a supervised scenery test; leave burst to user timing."""
import argparse,json,time
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('launch',type=Path);p.add_argument('--timeout',type=int,default=850);a=p.parse_args()
end=time.monotonic()+a.timeout;next_at=None;events=[];closed=False
while time.monotonic()<end:
 log=a.launch/'native-xr/openxr.jsonl'
 if (a.launch/'process-exit.json').exists():break
 if log.exists():
  with log.open('rb') as f:f.seek(max(0,log.stat().st_size-131072));tail=f.read()
  closed=b'"event":"native_xr_closed"' in tail
  if closed:break
  if next_at is None and b'"event":"native_game_pair_submitted"' in tail:next_at=time.monotonic()+30
  if next_at is not None and time.monotonic()>=next_at and len(events)<13:
   req=a.launch/'xr-capture.request'
   if not req.exists():req.touch();events.append(dict(sequence=len(events)+1,time_unix=time.time()));print(events[-1],flush=True)
   next_at=time.monotonic()+45
 time.sleep(1)
(a.launch/'scenery-image-requests.json').write_text(json.dumps(dict(requests=events,closed=closed),indent=2)+'\n',encoding='utf-8')
