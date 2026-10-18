"""Check all captured world eye cameras against a fixed replay reference, independent of session recenter."""
import argparse,json,math
from pathlib import Path
from replay_real_head_pose import quat
from check_capture_cameras import qmul,rotate

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('reference',type=Path);p.add_argument('csv',type=Path);a=p.parse_args()
 ref=a.reference.read_text().split();assert ref[0]=='EVR_SCENERY_REPLAY_1' and len(ref)==8
 base=list(map(float,ref[4:]));norm=math.sqrt(sum(x*x for x in base));base=[x/norm for x in base]
 origin=[int(x)/2**32 for x in ref[1:4]]
 rows=[list(map(float,l.split(','))) for l in a.csv.read_text().splitlines() if l and not l.startswith('#')]
 checks=[]
 for f in a.launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
  for line in f.open():
   e=json.loads(line)
   if e.get('event')!='captured_game_camera':continue
   r=rows[(e['frame']-1)%len(rows)];eye=e['eye'];q=quat(*[x*math.pi/180 for x in r[4:7]])
   delta=rotate(q,[(1 if eye else -1)*r[7]/2,0,0]);pos=[r[1+i]+delta[i] for i in range(3)];pos[2]*=-1
   expected=qmul(base,[-q[0],-q[1],q[2],q[3]]);qerror=min(max(abs(x-y) for x,y in zip(expected,e['orientation'])),max(abs(x+y) for x,y in zip(expected,e['orientation'])))
   off=rotate(base,pos);perror=max(abs(e['camera_q32'][i]/2**32-origin[i]-off[i]) for i in range(3))
   checks.append(dict(frame=e['frame'],eye=eye,position_error_m=perror,quaternion_error=qerror,ok=perror<1e-5 and qerror<2e-6))
 report=dict(status='PASS' if checks and all(c['ok'] for c in checks) else 'FAIL',cameras=len(checks),max_position_error_m=max((c['position_error_m'] for c in checks),default=None),max_quaternion_error=max((c['quaternion_error'] for c in checks),default=None),checks=checks,limitation='Geometry evidence only; world lighting/time/streaming and image correctness are not established.')
 (a.launch/'scenery-replay-check.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='checks'},indent=2));return int(report['status']!='PASS')
if __name__=='__main__':raise SystemExit(main())
