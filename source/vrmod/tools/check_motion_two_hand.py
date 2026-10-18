"""Bounded calibrated two-hand native-input gate. No tracked-contact/damage claim."""
import argparse,copy,hashlib,json,math
from pathlib import Path
from check_motion_attack import check as attack_check
from check_motion_feedback import check as feedback_check
from check_recorded_xr_native import rotate
from grip_preview_math import calibrated


def rows(p):return [json.loads(x) for x in p.read_text(encoding='utf-8-sig').splitlines()]
def sequence(p):return [[float(v) for v in x.split(',')] for x in p.read_text().splitlines() if x and not x.startswith('#')]
def support_checks(native,seq,profile,tolerance,collider_applied=False,early=False,swing_frame=440):
 ev=lambda name:[e for e in native if e['event']==name]
 samples=ev('motion_attack_sample');support=ev('two_hand_attack_support');poses=ev('grip_preview_pose');starts=[s for s in samples if s['gesture_request']]
 poseframes={p['frame'] for p in poses if p['resource_verified']};hands={e['frame'] for e in ev('motion_hand_pose')};weapon={e['frame'] for e in ev('motion_visual_pose')}
 byframe={e['frame']:e for e in support};windows={name:[e for e in support if lo<=e['frame']<=hi] for name,lo,hi in [('unheld',390,395),('far',415,420)]}
 errors={};valid=True
 for name,lo,hi in [('unheld',390,395),('far',415,420),('qualified',swing_frame,swing_frame+5)]:
  gaps=[];speeds=[]
  for f in range(lo,hi+1):
   row=seq[f-1];p,q,both=calibrated(row,profile);off=[u+v for u,v in zip(p,rotate(q,profile['off_hand_in_scaled_model_XR']))];gaps.append(math.dist(off,row[1:4]));speeds.append(math.dist(row[14:17],seq[f-2][14:17])*90)
   valid &= len(row)==29 and row[0]==f and row[24:29]==[1,1,1,1,0] and row[12:14]==[1,1] and all(row[i]==0 for i in [7,8,9,10,20,21,22,23]) and row[11]==(0 if name=='unheld' else 1)
  errors[name]=dict(min_gap=min(gaps),max_gap=max(gaps),min_speed=min(speeds),max_speed=max(speeds))
 enabled=ev('two_hand_attack_enabled')
 checks=dict(
  explicit_isolated_two_hand_policy=len(enabled)==1 and enabled[0]['item']==profile['sample']['item'] and enabled[0]['type']==7 and abs(enabled[0]['tolerance']-tolerance)<1e-7 and enabled[0]['tracked_hit_geometry_enabled'] is collider_applied,
  CSV_separates_fast_unheld_far_qualified=valid and all(2<=e['min_speed']<=e['max_speed']<=12 for e in errors.values()) and errors['far']['min_gap']>tolerance and errors['qualified']['max_gap']<tolerance,
  unheld_fast_swing_observed_and_rejected=len(windows['unheld'])>=2 and all(not e['left_grab'] and e['left_tracked'] and e['blocked']==6 and e['request']==0 for e in windows['unheld']),
  far_fast_swing_observed_and_rejected=len(windows['far'])>=2 and all(e['left_grab'] and e['left_tracked'] and e['blocked']==6 and e['request']==0 for e in windows['far']),
  only_qualified_frame_requests_attack=len(starts)==1 and starts[0]['frame']==swing_frame and starts[0]['gesture_request']==1 and byframe.get(swing_frame,{}).get('request')==1 and byframe[swing_frame]['blocked']==0,
  support_flags_match_external_CSV=bool(support) and all(e['left_grab']==bool(seq[e['frame']-1][11]) and e['left_tracked']==bool(seq[e['frame']-1][12] and seq[e['frame']-1][13]) for e in support),
  input_after_same_frame_verified_hand_weapon=bool(starts) and all(s['frame'] in poseframes & hands & weapon and s['item']==profile['sample']['item'] for s in starts),
  calibrated_collision_mode_matches_policy=(bool(ev('motion_collider_apply_enabled')) and len(ev('motion_collider_apply_begin'))>=3 and (([r.get('operation') for r in ev('motion_event_retime')]==['append','suppress']) if early else not ev('motion_event_retime'))) if collider_applied else (not ev('motion_collider_apply_enabled') and not ev('motion_collider_apply_begin') and not ev('motion_event_retime')),
 )
 return checks,errors

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--controller-speed',action='store_true');p.add_argument('--right-speed',action='store_true');p.add_argument('--collider-applied',action='store_true');p.add_argument('--early-impact',action='store_true');a=p.parse_args();probe=a.probe;launch=probe.parent
 if a.right_speed and not a.controller_speed:p.error('--right-speed requires --controller-speed')
 native=rows(probe/'view-resource-probe.jsonl');xr=rows(launch/'native-xr/openxr.jsonl');mock=rows(launch/'mock.jsonl');seq=sequence(launch/'mock-hands.csv');profile=json.loads((probe/'grip-preview.json').read_text())
 magic,tol=(probe/'world-two-hand-attack.txt').read_text().split();tolerance=float(tol)
 checks,metrics=attack_check(native,seq);two,errors=support_checks(native,seq,profile,tolerance,a.collider_applied,a.early_impact,520 if a.controller_speed else 440);checks.update(two);fb,fm=feedback_check(native,xr,mock,two_hand=True);checks.update({'feedback_'+k:v for k,v in fb.items()})
 checks['early_requires_physics']=not a.early_impact or a.collider_applied
 checks['exact_explicit_fixture']=magic=='EVR_TWO_HAND_ATTACK_1' and tolerance==.035 and (launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env'/('right-hand-speed-hands.csv' if a.right_speed else 'controller-speed-late-hands.csv' if a.controller_speed else 'two-hand-attack-hands.csv')).read_bytes()
 if a.controller_speed:
  from check_controller_swing_speed import audit as speed_audit
  sc,sm=speed_audit(native,seq,profile);checks.update(sc);errors['controller_speed']=sm
 checks['profile_files_unchanged']=all(hashlib.sha256((probe/dst).read_bytes()).hexdigest()==profile['files'][src] for src,dst in [('profile.txt','world-grip-preview.txt'),('hand-pose.bin','grip-hand-pose.bin')])
 checks['normal_exit']=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 checks['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS'
 checks['Core_Sync_clean']=(launch/'validation.log').stat().st_size==0
 if a.negative_tests:
  for name,event,key,value in [('support_false','two_hand_attack_support','left_tracked',False),('grip_bypass','two_hand_attack_support','blocked',0),('early_request','motion_attack_sample','frame',390),('wrong_item','motion_attack_sample','item',0),('hidden_asset','grip_preview_pose','resource_verified',False),('duplicate_attack','motion_attack_sample','gesture_request',2)]:
   bad=copy.deepcopy(native)
   for e in bad:
    if e['event']==event:e[key]=value
   checks['reject_'+name]=not all(support_checks(bad,seq,profile,tolerance,a.collider_applied,a.early_impact,520 if a.controller_speed else 440)[0].values())
  for forbidden in (() if a.early_impact else ('motion_event_retime',) if a.collider_applied else ('motion_collider_apply_enabled','motion_collider_apply_begin','motion_event_retime')):
   checks['reject_'+forbidden]=not support_checks(native+[dict(event=forbidden)],seq,profile,tolerance,a.collider_applied,a.early_impact,520 if a.controller_speed else 440)[0]['calibrated_collision_mode_matches_policy']
  bad=copy.deepcopy(seq)
  for f in range(415,421):
   pose,q,_=calibrated(seq[f-1],profile);bad[f-1][1:4]=[u+v for u,v in zip(pose,rotate(q,profile['off_hand_in_scaled_model_XR']))]
  checks['reject_wrong_support_CSV']=not all(support_checks(native,bad,profile,tolerance,a.collider_applied,a.early_impact,520 if a.controller_speed else 440)[0].values())
  bad=copy.deepcopy(xr)
  for e in bad:
   if e['event']=='native_motion_haptic':e['amplitude'][0]=0
  checks['reject_missing_offhand_feedback']=not all(feedback_check(native,bad,mock,two_hand=True)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},attack=metrics,support=errors,feedback=fm,limitation=__doc__)
 (probe/'motion-two-hand-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(json.dumps(dict(attack=metrics,support=errors,feedback=fm)));print('PASS',sum(bool(v) for v in checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
