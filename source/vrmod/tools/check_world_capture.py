"""Verify the requested bounded RenderDoc capture, separately from play stability."""
import argparse,json
from pathlib import Path

def audit(launch,pair=None,manual=False):
 events=[]
 for f in launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
  for line in f.open(encoding='utf-8'):
   e=json.loads(line)
   if e.get('event') in ('world_final_capture_started','world_final_capture_ended'):
    events.append(dict(probe=str(f.parent),**e))
 starts=[e for e in events if e['event']=='world_final_capture_started']
 ends=[e for e in events if e['event']=='world_final_capture_ended']
 files=[dict(path=str(f),bytes=f.stat().st_size) for f in launch.glob('view-resource-probe-*/world_final*.rdc')]
 start_pair=starts[0].get('paired_frames') if len(starts)==1 else None
 manual_policy=(Path(starts[0]['probe'])/'world-capture-on-request.txt') if len(starts)==1 else None
 manual_armed=bool(manual_policy and manual_policy.is_file() and manual_policy.read_text(encoding='utf-8').strip()=='EVR_WORLD_CAPTURE_REQUEST_1')
 checks=dict(single_start=len(starts)==1,single_end=len(ends)==1,single_file=len(files)==1 and files[0]['bytes']>0,
  requested_mode=len(starts)==1 and bool(starts[0].get('manual_request',False))==manual,
  expected_start=(manual_armed and isinstance(start_pair,int) and start_pair>30 and start_pair%15==0) if manual else pair is not None and start_pair==pair-1,
  expected_end=len(ends)==1 and ends[0].get('paired_frames')==(start_pair+1 if manual and isinstance(start_pair,int) else pair),
  same_probe=len(starts)==len(ends)==1 and starts[0]['probe']==ends[0]['probe'],
  ordered=len(starts)==len(ends)==1 and ends[0]['us']>starts[0]['us'])
 return dict(status='PASS' if all(checks.values()) else 'FAIL',mode='manual' if manual else 'fixed_pair',requested_pair=pair,checks=checks,events=events,files=files,
  limitation='Capture completion only; replay and image inspection are still required.')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);mode=p.add_mutually_exclusive_group(required=True);mode.add_argument('--pair',type=int);mode.add_argument('--manual',action='store_true');a=p.parse_args()
 r=audit(a.launch,a.pair,a.manual);(a.launch/'world-capture-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
 print(json.dumps(r,indent=2));return int(r['status']!='PASS')
if __name__=='__main__':raise SystemExit(main())
