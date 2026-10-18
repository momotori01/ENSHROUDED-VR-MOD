"""Verify physical bow private bones against native sampled curve and both XR eyes."""
import argparse,collections,csv,json,math,struct
from pathlib import Path
from check_capture_cameras import qmul,rotate
from emulate_native_grip import Engine,compare
from bow_eye_coverage import audit as eye_coverage

def audit(probe):
 e=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()];ev=lambda n:[x for x in e if x['event']==n]
 poses={x['serial']:x for x in ev('motion_bow_pose')};p={x['serial']:x for x in ev('motion_bow_physical_pose')};d={x['serial']:x for x in ev('motion_visual_pose') if x['item']==327157552}
 c=ev('motion_bow_created');r=ev('motion_bow_pose_disposed');models={x['handle']:x for x in ev('motion_visual_disposed')};controls={x['frame']:x for x in ev('bow_physical_trial_sample')}
 checks=dict(enabled=bool(ev('motion_bow_physical_enabled')),all_frames=bool(p) and len(p)==len(ev('motion_bow_physical_pose')) and set(p)==set(poses)==set(d),
 private_ownership=bool(c) and all(x['private_pose'] and x['pose_handle']!=x['source_pose'] and x['references_after']==x['references_before']+1 for x in c),
 source_isolation=True,curve_match=True,physical_distance=True,active_control=True,rest_after_cancel=True,
 lifetime=bool(r) and len(r)==len(c),native_fk=True,source_snapshots=True,
 completed=any(x['result']==0 for x in ev('view_resource_hook_complete')),no_failure=not any('failure' in x['event'] for x in e))
 keys=[]
 for x in json.loads(Path('docs/analysis/weapon_mount_20261010/bow-native-pull-curve.json').read_text())['samples']:
  if keys and x['native_pull_m']<=keys[-1][0]+1e-6:continue
  keys.append((x['native_pull_m'],list(struct.unpack('<72f',bytes.fromhex(x['joints_bytes'])))))
 def curve(pull):
  if pull<=0:return keys[0][1].copy()
  if pull>=keys[-1][0]:return keys[-1][1].copy()
  hi=next(i for i in range(1,len(keys)) if keys[i][0]>=pull);a,b=keys[hi-1],keys[hi];t=(pull-a[0])/(b[0]-a[0]);out=a[1].copy()
  for j in range(2,6):
   k=j*12;q=b[1][k:k+4];u=a[1][k:k+4]
   if sum(v*w for v,w in zip(q,u))<0:q=[-v for v in q]
   q=[v+(w-v)*t for v,w in zip(u,q)];norm=math.sqrt(sum(v*v for v in q));out[k:k+4]=[v/norm for v in q]
   out[k+4:k+12]=[v+(w-v)*t for v,w in zip(a[1][k+4:k+12],b[1][k+4:k+12])]
  return out
 hands={int(x[0]):list(map(float,x)) for x in csv.reader(x for x in (probe.parent/'mock-hands.csv').read_text().splitlines() if x and not x.startswith('#'))}
 max_curve=max_distance=fkerror=0;fkchecks=0;engine=Engine();active=[]
 for serial,x in p.items():
  v=poses.get(serial);m=d.get(serial)
  if not v or not m:checks['all_frames']=False;continue
  raw=bytes.fromhex(v['joints_bytes']);vals=struct.unpack('<72f',raw);want=curve(x['model_pull'])
  checks['source_isolation'] &= x['source_unchanged'] and v['source_unchanged'] and x['private_pose_only'] and v['mask']==v['sdf']==0 and v['palette_count']==4 and v['frame']==x['frame']==m['frame']
  checks['source_isolation'] &= v['owned_equals_source']==(raw==bytes.fromhex(x['source_joints_bytes']))
  if x['held']:
   active.append(x);ctrl=controls.get(x['control_frame'],{});checks['active_control'] &= ctrl.get('phase')==2 and ctrl.get('binding')==x['binding'] and 0<=x['frame']-x['control_frame']<=8
   h=hands[x['frame']];yaw,pitch,roll=[math.radians(t)/2 for t in h[4:7]];q=qmul(qmul([0,math.sin(yaw),0,math.cos(yaw)],[math.sin(pitch),0,0,math.cos(pitch)]),[0,0,math.sin(roll),math.cos(roll)])
   local=rotate([-q[0],-q[1],-q[2],q[3]],[a-b for a,b in zip(h[14:17],h[1:4])]);dist=max(0,min(keys[-1][0],local[2]/m['scale'][2]-.24005109));max_distance=max(max_distance,abs(dist-x['model_pull']))
   if x.get('tracked_string_contact'):
    assert all(abs(want[j*12+k])<1e-8 for j in [0,1] for k in range(3)) and all(abs(want[j*12+k]-1)<1e-7 for j in [0,1] for k in [8,9,10])
    target=[local[i]/m['scale'][i]*(1 if i<2 else -1)-want[4+i]-want[16+i] for i in range(3)]
    delta=[target[i]-want[64+i] for i in range(3)]
    for i in range(3):want[64+i]=target[i];want[52+i]+=delta[i]

  else:checks['rest_after_cancel'] &= x['model_pull']==0 and raw==bytes.fromhex(json.loads(Path('docs/analysis/weapon_mount_20261010/bow-native-pull-curve.json').read_text())['samples'][0]['joints_bytes'])
  max_curve=max(max_curve,max(abs(a-b) for a,b in zip(vals,want)))
  if x['frame'] in [600,620,680,730]:
   fkchecks+=1;actual=engine.bones(raw,[65535,0,1,1,1,1]);fkerror=max(fkerror,*compare(actual,raw,[65535,0,1,1,1,1]))
 checks['curve_match'] &= max_curve<2e-5;checks['physical_distance'] &= bool(active) and max_distance<2e-5 and max(x['model_pull'] for x in active)>.34
 checks['native_fk'] &= fkchecks>=3 and fkerror<1e-5 and not engine.calls
 matches=0
 for x in ev('native_grip_sample'):
  if x['weapon_type']!=4:continue
  v=p.get(x['serial']);ok=bool(v) and bytes.fromhex(v['source_joints_bytes'])==(probe/x['folder']/'weapon-current-joints.bin').read_bytes();checks['source_snapshots'] &= ok;matches+=int(ok)
 checks['source_snapshots'] &= matches>=4
 for x in r:
  checks['lifetime'] &= x['handle'] in models and models[x['handle']]['us']<x['us'] and x['generation_invalidated'] and x['reference_released'] and x['updates']==sum(y['handle']==x['handle'] for y in poses.values())
 eyes=ev('motion_bow_eye_payload');counts=collections.Counter((x['serial'],x['eye']) for x in eyes)
 coverage=eye_coverage(e,poses);checks['both_eyes']=coverage['status']=='PASS'
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,poses=len(p),eyes=len(eyes),eye_coverage=coverage,max_curve_error=max_curve,max_model_pull_error_m=max_distance,max_native_fk_error=fkerror,native_fk_checks=fkchecks,source_snapshots=matches,limitation='Single known bow Mock physical deformation; shooting, native aim/power and hardware require separate audits.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'physical-bow-pose-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
