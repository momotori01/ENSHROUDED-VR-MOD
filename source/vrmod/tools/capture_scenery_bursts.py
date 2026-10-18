"""Request and verify up to four bounded consecutive XR captures. Inputs are unchanged."""
import argparse,json,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--frames',default='850,1150,1450,1750');p.add_argument('--timeout',type=int,default=300);a=p.parse_args()
pending=sorted(set(map(int,a.frames.split(','))));wanted=len(pending);assert 1<=wanted<=4 and min(pending)>0
log=a.launch/'native-xr/openxr.jsonl';request=a.launch/'xr-burst.request';events=[];receipts=[];completed=[];offset=0;fragment=b'';deadline=time.monotonic()+a.timeout
while time.monotonic()<deadline:
 if not log.exists():time.sleep(.2);continue
 with log.open('rb') as f:f.seek(offset);data=fragment+f.read();offset=f.tell()
 lines=data.split(b'\n');fragment=lines.pop();frame=0;closed=False
 for line in lines:
  try:e=json.loads(line)
  except ValueError:continue
  frame=max(frame,e.get('frame',0));closed|=e.get('event')=='native_xr_closed'
  if e.get('event')=='manual_xr_burst':receipts.append(e)
  if e.get('event')=='native_burst_capture_complete' and e.get('index')==32 and e.get('total')==32:completed.append(e)
 if closed or any(e.get('accepted') is not True for e in receipts) or len(completed)==wanted:break
 if pending and frame>=pending[0] and not request.exists() and len(completed)==len(events):
  target=pending.pop(0);request.write_text(f'32 {wanted}',encoding='ascii');events.append(dict(requested=target,observed=frame,count=32,session_limit=wanted));print(events[-1],flush=True)
 time.sleep(.1)
checks=dict(all_requested=len(events)==wanted,all_accepted=len(receipts)==wanted and all(e.get('accepted') is True for e in receipts),all_completed=len(completed)==wanted)
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,requests=events,receipts=receipts,completed=completed,missed=pending)
(a.launch/'scenery-burst-requests.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(r['status'],checks,flush=True)
raise SystemExit(int(r['status']!='PASS'))
