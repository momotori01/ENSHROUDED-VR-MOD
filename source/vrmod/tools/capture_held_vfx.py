"""Request three XR images after validated torch mounting, without input or injection."""
import argparse,time,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('launch',type=Path);p.add_argument('--timeout',type=float,default=240);a=p.parse_args()
deadline=time.monotonic()+a.timeout;after=1000;captures=[]
while time.monotonic()<deadline and len(captures)<3:
 probes=sorted(a.launch.glob('view-resource-probe-*'))
 if len(probes)<2:time.sleep(.5);continue
 fpath=probes[-1]/'view-resource-probe.jsonl'
 if not fpath.exists():time.sleep(.5);continue
 with fpath.open('rb') as f:
  f.seek(max(0,fpath.stat().st_size-524288));lines=f.read().splitlines()
 events=[]
 for l in lines:
  if b'"event":"held_vfx_mounted"' in l:
   try:events.append(json.loads(l))
   except ValueError:pass
 if events:
  frame=events[-1]['frame']
  if frame>=after and not (a.launch/'xr-capture.request').exists():
   (a.launch/'xr-capture.request').touch();captures.append(frame);after=frame+60;print('capture',len(captures),'mount frame',frame,flush=True)
 time.sleep(.2)
(a.launch/'held-vfx-image-requests.json').write_text(json.dumps(dict(requested_frames=captures),indent=2)+'\n',encoding='utf-8');print('capture watcher finished',len(captures),flush=True)
