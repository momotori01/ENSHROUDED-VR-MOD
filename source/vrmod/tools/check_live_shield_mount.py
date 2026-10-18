"""Audit the measured private shield mount, two-eye payload and Scene lifetime."""
import argparse,collections,json,math
from pathlib import Path
from check_recorded_xr_native import normalize,multiply,rotate

def audit(probe,profile_file):
 rows=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 ev=lambda name:[r for r in rows if r['event']==name]
 profile=json.loads(profile_file.read_text());local=profile['game_hand_to_model']
 hands={r['serial']:r for r in ev('motion_hand_pose')};anchors={r['recorded_xr_frame']:r for r in ev('recorded_xr_anchor')}
 selection={r['serial']:r for r in ev('motion_visual_selection')};weapons={r['serial']:r for r in ev('motion_visual_pose')}
 poses=ev('motion_shield_pose');by_serial={r['serial']:r for r in poses}
 created=ev('motion_shield_created');retired=ev('motion_shield_disposed');eyes=ev('motion_shield_eye_payload')
 gpu=[json.loads(s) for s in (probe.parent/'vulkan.jsonl').read_text().splitlines()]
 alloc={int(r['address'],16):r for r in gpu if r['event']=='body_payload_created'}
 c=dict(explicit_trial=any(r['event']=='motion_shield_enabled' for r in rows),known_shield=len(created)==1 and created[0]['item']==profile['item'],reference_acquired=bool(created) and all(r['references_after']==r['references_before']+1 for r in created),private_instance=bool(created) and all(r['handle']!=r['source_handle'] and r['mask']==r['sdf']==0 and not r['combat_authorized'] for r in created),enough_unique_poses=len(poses)>100 and len(poses)==len(by_serial),same_frame=bool(poses),source_unchanged=bool(poses),measured_left_socket=bool(poses),both_eyes=bool(eyes),owned_payload=bool(eyes),other_models_retained=bool(eyes),source_hidden=bool(eyes),retired=len(retired)==len(created)==1,completed=any(r['result']==0 for r in ev('view_resource_hook_complete')),no_failure=not any('failure' in r['event'] for r in rows))
 errors=dict(position_m=0.,rotation=0.);missing=0
 for r in poses:
  h=hands.get(r['serial']);a=anchors.get(r['frame'])
  if not h or not a:missing+=1;continue
  c['same_frame'] &= h['frame']==r['frame'] and h['time']==r['time']
  c['source_unchanged'] &= r['source_unchanged'] and h['source_unchanged'] and r['mask']==r['sdf']==0
  c['measured_left_socket'] &= r['grip_hand']==0 and r['native_slot']==12 and r['item']==profile['item']
  hp=h['grips'][0][:3];hq=h['grips'][0][3:]
  delta=rotate(a['orientation'],[u+v+w for u,v,w in zip(hp,rotate(hq,local['p']),a['camera_offset'])])
  errors['position_m']=max(errors['position_m'],max(abs((u-v)/2**32-w) for u,v,w in zip(r['q32'],a['camera_q32'],delta)))
  expected=normalize(multiply(a['orientation'],multiply(hq,local['q'])))
  errors['rotation']=max(errors['rotation'],min(max(abs(u-v) for u,v in zip(expected,r['orientation'])),max(abs(u+v) for u,v in zip(expected,r['orientation']))))
 c['all_pose_evidence']=missing==0
 c['measured_left_socket'] &= max(errors.values())<1e-5
 c['both_eyes'] &= collections.Counter((r['serial'],r['eye']) for r in eyes)==collections.Counter((s,eye) for s in by_serial for eye in (0,1))
 for r in eyes:
  pose=by_serial.get(r['serial'],{});address=r['payload'][0]|r['payload'][1]<<32;payload=alloc.get(address,{})
  c['owned_payload'] &= pose.get('frame')==r['frame'] and r['shield_instance']==(pose.get('handle',0)&0xffffff) and r['payload'][2]==0x45564850 and payload.get('tag')==0x45564850 and payload.get('shield_plus_one')==r['shield_instance']+1 and payload.get('immutable',False) and payload.get('us',10**20)<r['us'] and payload.get('models')==len(set(r['indices']))
  ids=[r['hand_instance'],r['extra_instance'],r['shield_instance']]
  sel=selection.get(r['serial'],{});hand=hands.get(r['serial'],{});weapon=weapons.get(r['serial'],{})
  wanted=(weapon.get('handle',0)&0xffffff) if sel.get('weapon_ready') else 0xffffffff
  c['other_models_retained'] &= bool(sel) and sel.get('frame')==r['frame'] and sel.get('source_unchanged') and len(set(ids))==3 and ids[1]==(hand.get('handle',0)&0xffffff) and ids[0]==wanted and not set(ids)&set(r['indices']) and payload.get('hand_plus_one')==(0 if ids[0]==0xffffffff else ids[0]+1) and payload.get('extra_plus_one')==ids[1]+1
  if sel.get('weapon_ready'):c['other_models_retained'] &= weapon.get('frame')==r['frame'] and weapon.get('source_unchanged') and (weapon.get('source_handle',0)&0xffffff) in r['indices']
  c['source_hidden'] &= (pose.get('source_handle',0)&0xffffff) in r['indices'] and r['only_private_stack_padding_changed'] and r['handles_revalidated']
 for r in retired:
  c['retired'] &= r['handle']==created[0]['handle'] and r['updates']==len(poses) and r['generation_invalidated'] and r['reference_released'] and r['scene_disposal'] and r['us']>max((p['us'] for p in poses),default=0)
 return dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,pose_count=len(poses),eye_count=len(eyes),max_errors=errors,missing=missing,created=created,retired=retired,limitation='One measured rigid shield, display only. Image/comfort, other shield assets and posture guard require separate verification.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('profile',type=Path);a=p.parse_args();r=audit(a.probe,a.profile);(a.probe/'live-shield-mount-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
