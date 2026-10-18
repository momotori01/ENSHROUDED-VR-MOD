"""Check both grip contacts, measured sliding and wheel isolation in the explicit live display trial."""
import argparse,json,math
from pathlib import Path
from check_recorded_xr_native import rotate

def audit(probe,profiles_file,extended=False):
 profiles={x['item']:x for x in json.loads(profiles_file.read_text())['profiles']}
 e=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 poses=[x for x in e if x['event']=='motion_visual_pose'];active=[x for x in poses if x.get('wield_active')]
 hands={x['serial']:x for x in e if x['event']=='motion_hand_pose'};anchors={x['recorded_xr_frame']:x for x in e if x['event']=='recorded_xr_anchor'}
 pad=[x for x in e if x['event']=='controller_live_sample'];claimed=[x for x in pad if x.get('wield_grip')]
 c=dict(explicit_policy=any(x['event']=='live_two_hand_wield_enabled' and x['display_only'] and not x['combat_authorized'] for x in e),bounded_active=len(active)>=120 and all(500<=x['frame']<=680 for x in active),both_contacts=bool(active),source_unchanged=bool(active),same_frame=bool(active),complete_evidence=bool(active))
 errors=dict(right_m=0.,left_m=0.,distance_m=0.);ys=[];lefts=[];offs=[]
 for x in active:
  h=hands.get(x['serial']);a=anchors.get(x['frame']);p=profiles.get(x['item'])
  if not h or not a or not p:c['complete_evidence']=False;continue
  c['same_frame'] &= h['frame']==x['frame'] and h['time']==x['time']
  c['source_unchanged'] &= x['source_unchanged'] and h['source_unchanged'] and x['native_mask']==0 and x['sdf_handle']==0
  c['both_contacts'] &= x['mount_kind']=='native_melee_right_grip' and x['grip_hand']==1
  scale=x['scale'];pivot=p['right_in_model_XR'];right=[pivot[0],x['wield_main_y'],pivot[2]];left=[0,x['wield_off_y'],0]
  for hand,model,label in [(0,left,'left_m'),(1,right,'right_m')]:
   v=rotate(x['orientation'],[m*s*(1 if i!=2 else -1) for i,(m,s) in enumerate(zip(model,scale))])
   actual=[(u-v0)/2**32+d for u,v0,d in zip(x['q32'],a['camera_q32'],v)]
   target=rotate(a['orientation'],[u+v for u,v in zip(h['grips'][hand][:3],a['camera_offset'])])
   errors[label]=max(errors[label],math.dist(actual,target))
  errors['distance_m']=max(errors['distance_m'],abs(x['wield_distance']-math.dist(h['grips'][0][:3],h['grips'][1][:3])))
  ys.append(x['wield_main_y']);offs.append(x['wield_off_y']);lefts.append(h['grips'][0][:3])
 episodes=[]
 for x in active:
  if not episodes or episodes[-1][-1]['frame']+1!=x['frame'] or episodes[-1][-1]['item']!=x['item']:episodes.append([])
  episodes[-1].append(x)
 c['both_contacts'] &= max(errors.values())<1e-5
 span=max(ys)-min(ys) if ys else 0
 c['eighteen_cm_slide']=.175<span<.185
 c['support_grip_fixed']=bool(offs) and max(offs)-min(offs)<1e-6 and max(math.dist(p,lefts[0]) for p in lefts)<1e-6
 c['released_to_single']=any(x['frame']==681 and not x.get('wield_active') for x in poses)
 c['no_wheel_during_wield']=len(claimed)>=3 and all(x['raw_grips']==[1,1] and not x['buttons']&0x200 and x['auxiliary']==0 for x in claimed)
 c['wheel_after_release']=sum(bool(x['buttons']&0x200) and not x.get('wield_grip') and x['frame']>=750 for x in pad)>=5
 if extended:
  wanted=set(range(500,640))|set(range(675,681))|set(range(2140,2231))
  c['bounded_active']={x['frame'] for x in active}==wanted and len(active)==len(wanted)
  c['two_melee_assets']={x['item'] for x in active}==set(profiles)
  # Reacquisition intentionally chooses a new support point. Never mix that
  # offset (or different assets) into the 18 cm within-grasp sliding span.
  full=[ep for ep in episodes if (ep[0]['frame'],ep[-1]['frame']) in {(500,639),(2140,2230)}]
  c['eighteen_cm_slide']=len(full)==2 and all(.175<max(x['wield_main_y'] for x in ep)-min(x['wield_main_y'] for x in ep)<.185 for ep in full)
  c['support_grip_fixed']=len(episodes)==3 and all(max(x['wield_off_y'] for x in ep)-min(x['wield_off_y'] for x in ep)<1e-6 and max(math.dist(hands[x['serial']]['grips'][0][:3],hands[ep[0]['serial']]['grips'][0][:3]) for x in ep)<1e-6 for ep in episodes)
  c['release_without_wheel']=all(not x.get('wield_active') for x in poses if 640<=x['frame']<=674) and any(x['frame']==640 for x in poses) and all(not x['buttons']&0x200 for x in pad if 640<=x['frame']<750)
  c['right_repress_reacquires']=any(x['frame']==675 for x in active)
  c['axe_released']=any(x['frame']==2231 and not x.get('wield_active') for x in poses)
 c['no_attack_input']=bool(pad) and all(x['triggers']==[0,0] and not x['buttons']&0x4000 for x in pad)
 c['completed']=any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in e)
 c['no_failure']=not any('failure' in x['event'] for x in e)
 return dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,active_poses=len(active),grip_claim_samples=len(claimed),slide_m=span,episodes=[dict(item=ep[0]['item'],first=ep[0]['frame'],last=ep[-1]['frame'],slide_m=max(x['wield_main_y'] for x in ep)-min(x['wield_main_y'] for x in ep),support_y=ep[0]['wield_off_y']) for ep in episodes],max_errors=errors,limitation='Explicit measured-asset display fixture only. No combat, reversed hand order, all weapon classes or real-controller comfort proof.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('profiles',type=Path);p.add_argument('--extended',action='store_true');a=p.parse_args();r=audit(a.probe,a.profiles,a.extended);(a.probe/'live-wield-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
