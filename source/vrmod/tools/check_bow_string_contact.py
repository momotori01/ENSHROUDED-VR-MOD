"""Measure private string endpoint against the recorded right grip using native FK."""
import argparse,csv,json,math
from pathlib import Path
from check_capture_cameras import qmul,rotate
from emulate_native_grip import Engine

def audit(probe):
 events=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 poses={e['frame']:e for e in events if e['event']=='motion_bow_pose'}
 visuals={e['frame']:e for e in events if e['event']=='motion_visual_pose' and e['item']==327157552}
 controls={e['frame']:e for e in events if e['event']=='motion_bow_physical_pose' and e['held']}
 hands={int(x[0]):list(map(float,x)) for x in csv.reader(x for x in (probe.parent/'mock-hands.csv').read_text().splitlines() if x and not x.startswith('#'))}
 engine=Engine();rows=[]
 for frame in [540,570,600,620,650,680,700]:
  if frame not in controls:continue
  h=hands[frame];m=visuals[frame];p=poses[frame];bones=engine.bones(bytes.fromhex(p['joints_bytes']),[65535,0,1,1,1,1]);n=bones[5][1]
  yaw,pitch,roll=[math.radians(x)/2 for x in h[4:7]]
  q=qmul(qmul([0,math.sin(yaw),0,math.cos(yaw)],[math.sin(pitch),0,0,math.cos(pitch)]),[0,0,math.sin(roll),math.cos(roll)])
  local=[n[0]*m['scale'][0],n[1]*m['scale'][1],-n[2]*m['scale'][2]]
  point=[a+b for a,b in zip(h[1:4],rotate(q,local))];target=h[14:17]
  rows.append(dict(frame=frame,endpoint_tracking=point,right_grip=target,distance_m=math.dist(point,target)))
 checks=dict(native_fk_without_substitution=not engine.calls,enough_samples=len(rows)>=6,string_contact=bool(rows) and max(r['distance_m'] for r in rows)<2e-5)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,max_distance_m=max((r['distance_m'] for r in rows),default=None),samples=rows,limitation='Known bow at left-grip mount in this fixture. Right controller grip is a provisional pinch point; hardware fingertip calibration and arrow mesh orientation remain unverified.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'bow-string-contact-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
