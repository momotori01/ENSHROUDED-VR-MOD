"""Check submitted bow poses against same-frame left socket and source attachment evidence."""
import argparse,json,math,struct
from pathlib import Path
from check_recorded_xr_native import normalize,multiply,rotate

def audit(probe,native_only=False,animated=False):
 events=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 hands={x['serial']:x for x in events if x['event']=='motion_hand_pose'}
 anchors={x['recorded_xr_frame']:x for x in events if x['event']=='recorded_xr_anchor'}
 observed={x['item']:x for x in events if x['event']=='live_weapon_attachment_observation' and x['weapon_type']==4}
 poses=[x for x in events if x['event']=='motion_visual_pose' and x['item'] in observed]
 checks=dict(bow_present=len(poses)>100,left_selected=bool(poses),same_frame=bool(poses),source_unchanged=bool(poses),source_attachment=bool(observed),opposite_hand_separate=bool(poses))
 errors=dict(position_m=0.,rotation=0.);missing=0
 for x in poses:
  checks['left_selected'] &= x.get('grip_hand')==0 and x.get('mount_kind')==('native_left_bow_animated' if animated else 'native_left_bow' if native_only else 'native_left_bow_string_back')
  checks['source_unchanged'] &= x['source_unchanged'] and x['native_mask']==0 and x['sdf_handle']==0
  h=hands.get(x['serial']);a=anchors.get(x['frame']);o=observed[x['item']]
  if h is None or a is None:missing+=1;continue
  checks['same_frame'] &= h['frame']==x['frame'] and h['time']==x['time'] and h['source_unchanged']
  checks['opposite_hand_separate'] &= math.dist(h['grips'][0][:3],h['grips'][1][:3])>.1
  raw=bytes.fromhex(o['attachment_bytes']);local=[v/2**32 for v in struct.unpack_from('<3q',raw)];q=list(struct.unpack_from('<4f',raw,24))
  checks['source_attachment'] &= len(raw)==48 and raw[40]==0 and raw[41]==9 and struct.unpack_from('<I',raw,44)[0]==x['entity'] and o['slot']==9 and o['item_slot']==5 and o['read_only']
  hp=h['grips'][0][:3];hq=h['grips'][0][3:];local=[u+v+w for u,v,w in zip(hp,rotate(hq,local),a['camera_offset'])]
  delta=rotate(a['orientation'],local)
  errors['position_m']=max(errors['position_m'],max(abs((u-v)/2**32-w) for u,v,w in zip(x['q32'],a['camera_q32'],delta)))
  expected=normalize(multiply(a['orientation'],multiply(hq,q if (native_only or animated) else multiply(q,[0,1,0,0]))))
  errors['rotation']=max(errors['rotation'],min(max(abs(u-v) for u,v in zip(expected,x['orientation'])),max(abs(u+v) for u,v in zip(expected,x['orientation']))))
 checks['complete_evidence']=missing==0
 checks['left_native_pose_matches']=errors['position_m']<1e-5 and errors['rotation']<1e-5
 checks['completed']=any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in events)
 checks['no_failure']=not any('failure' in x['event'] for x in events)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,pose_count=len(poses),max_errors=errors,missing=missing,limitation='Display mount only. Not proof of physical draw, projectile aim, string animation, other bow assets or hardware comfort.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--native-only',action='store_true');p.add_argument('--animated',action='store_true');a=p.parse_args();r=audit(a.probe,a.native_only,a.animated);(a.probe/'live-bow-mount-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
