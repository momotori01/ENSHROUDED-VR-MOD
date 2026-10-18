"""Verify native measured grip points coincide with the tracked right hand in world space."""
import argparse,json,math,hashlib
from pathlib import Path
from check_recorded_xr_native import rotate,multiply,normalize

def audit(probe,profile_file):
 data=json.loads(profile_file.read_text());src=Path(data['source'])/'live-native-mounts.json'
 if hashlib.sha256(src.read_bytes()).hexdigest()!=data['source_sha256']:raise ValueError('measurement source changed')
 profiles={p['item']:p for p in data['profiles']}
 events=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 hands={x['serial']:x for x in events if x['event']=='motion_hand_pose'}
 anchors={x['recorded_xr_frame']:x for x in events if x['event']=='recorded_xr_anchor'}
 poses=[x for x in events if x['event']=='motion_visual_pose' and x['item'] in profiles and not x.get('wield_active',False)]
 samples=[x for x in events if x['event']=='native_grip_sample' and x['item'] in profiles]
 enabled=[x for x in events if x['event']=='live_weapon_mount_enabled']
 pitch=float(enabled[0]['pitch_degrees']);angle=-pitch*math.pi/360;trim=[math.sin(angle),0,0,math.cos(angle)]
 c=dict(both_assets=set(x['item'] for x in poses)==set(profiles),source_identity=set(x['item'] for x in samples)==set(profiles),right_selected=bool(poses),same_frame=bool(poses),source_unchanged=bool(poses),scale_evidence=bool(poses),complete_evidence=True)
 for x in samples:
  p=profiles[x['item']];c['source_identity'] &= x['weapon_guid']==p['weapon_guid'] and x['hand_guid']==p['hand_guid'] and x['weapon_type']==p['type'] and x['source_unchanged']
 errors=dict(grip_position_m=0.,orientation=0.)
 for x in poses:
  c['right_selected'] &= x.get('grip_hand')==1 and x.get('mount_kind')=='native_melee_right_grip'
  c['source_unchanged'] &= x['source_unchanged'] and x['native_mask']==0 and x['sdf_handle']==0
  a=anchors.get(x['frame']);h=hands.get(x['serial']);p=profiles[x['item']];scale=x.get('scale')
  if a is None or h is None:c['complete_evidence']=False;continue
  if scale is None:c['scale_evidence']=False;continue
  c['same_frame'] &= x['frame']==h['frame'] and x['time']==h['time'] and h['source_unchanged']
  c['scale_evidence'] &= len(scale)==3 and all(.9<s<1.1 for s in scale)
  native_model_pivot=[v*s*(1 if i!=2 else -1) for i,(v,s) in enumerate(zip(p['right_in_model_XR'],scale))]
  pivot=rotate(x['orientation'],native_model_pivot)
  actual=[(v-r)/2**32+d for v,r,d in zip(x['q32'],a['camera_q32'],pivot)]
  target=rotate(a['orientation'],[u+v for u,v in zip(a['camera_offset'],h['grips'][1][:3])])
  errors['grip_position_m']=max(errors['grip_position_m'],math.dist(actual,target))
  q=p['grip_to_model_XR']['q'];q=[-q[0],-q[1],q[2],q[3]]
  expected=normalize(multiply(a['orientation'],multiply(h['grips'][1][3:],multiply(trim,q))))
  errors['orientation']=max(errors['orientation'],min(max(abs(u-v) for u,v in zip(expected,x['orientation'])),max(abs(u+v) for u,v in zip(expected,x['orientation']))))
 c['measured_pivot_matches']=errors['grip_position_m']<1e-5 and errors['orientation']<1e-5
 c['completed']=any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in events)
 c['no_failure']=not any('failure' in x['event'] for x in events)
 return dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,poses=len(poses),max_errors=errors,limitation='Measured local-asset display only; physical comfort, two-hand slide and combat are not established.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('profiles',type=Path);a=p.parse_args();r=audit(a.probe,a.profiles);(a.probe/'live-melee-mount-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
