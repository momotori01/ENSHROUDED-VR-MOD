"""Genuine offhand grab loss: native collider cancellation without hiding tracked hands."""
import argparse,copy,hashlib,json
from pathlib import Path
from check_motion_tracking_loss import audit as lifecycle_audit
from check_calibrated_collider_shadow import audit as calibrated_audit
from check_motion_two_hand import rows,sequence

def audit(native,seq,profile,tolerance):
 c,m=lifecycle_audit(native,seq,offhand_release=True)
 proposals=[r for r in native if r['event']=='motion_collider_shadow'];applied=[r for r in native if r['event']=='motion_collider_apply_begin'];start=m['loss'][0]
 c['calibrated_weapon_enabled']=any(r['event']=='two_hand_attack_enabled' and r['item']==profile['sample']['item'] and r['type']==7 and r['tracked_hit_geometry_enabled'] for r in native)
 c['support_rejected_at_consumer']=all(r['two_hand_required'] and not r['two_hand_support_valid'] and not r['eligible'] for r in proposals if start<=r['hand_frame']<=500)
 c['no_apply_while_support_released']=all(not start<=r['frame']<=500 for r in applied)
 if applied:
  cc,cm=calibrated_audit(native,seq,profile,tolerance);c['calibrated_pose_while_gripped']=cc['both_grips_valid_at_physics_callback'];m['max_support_gap_m']=cm['max_support_gap_m']
 else:c['early_block_without_physics']=m['blocked_events']==1
 supports=[r for r in native if r['event']=='two_hand_attack_support'];c['prior_invalid_swings_rejected']=all(any(r['frame']==f and not r['request'] and r['blocked']==6 for r in supports) for f in [390,415])
 starts=[r for r in native if r['event']=='motion_attack_sample' and r['gesture_request']];poses={r['frame'] for r in native if r['event']=='grip_preview_pose' and r['resource_verified']}
 c['qualified_same_frame_start']=len(starts)==1 and starts[0]['frame']==440 and 440 in poses
 return c,m

def haptic_checks(native,xr,mock,start):
 delivery=[r for r in xr if r['event']=='native_motion_haptic'];requests={r['frame']:r for r in native if r['event']=='motion_feedback_requested'};calls=[r for r in mock if r['event']=='motion_haptic'];stops=[r for r in mock if r['event']=='motion_haptic_stop'];positive=[r for r in delivery if any(r['amplitude'])]
 return dict(
  feedback_deliveries_match=bool(delivery) and len(delivery)==len(requests) and all(r['frame'] in requests and abs(r['amplitude'][1]-requests[r['frame']]['amplitude'])<1e-6 and abs(r['amplitude'][0]-(0 if start<=r['frame']<=500 else requests[r['frame']]['amplitude']))<1e-6 and all(r['delivered']) for r in delivery),
  no_feedback_after_release=any(r['frame']==start+1 for r in delivery) and all(r['amplitude']==[0,0] for r in delivery if r['frame']>start),
  runtime_pulses_bounded=len(calls)==sum(a>0 for r in positive for a in r['amplitude']) and all(0<=c['amplitude']<=.6 and c['duration']==100000000 and c['frame']<=start and any(e['frame']==c['frame'] and abs(e['amplitude'][c['hand']]-c['amplitude'])<1e-6 for e in positive) for c in calls),
  runtime_stop_both_hands=all(any(s['hand']==h and start<=s['frame']<=start+1 for s in stops) for h in (0,1)),
  no_haptic_failures=not any('haptic_failed' in r['event'] for r in xr),
 )

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 native=rows(probe/'view-resource-probe.jsonl');seq=sequence(probe.parent/'mock-hands.csv');profile=json.loads((probe/'grip-preview.json').read_text());tolerance=float((probe/'world-two-hand-attack.txt').read_text().split()[1]);c,m=audit(native,seq,profile,tolerance)
 base=sequence(Path(__file__).resolve().parents[1]/'test_env/two-hand-attack-hands.csv')
 c['only_external_grab_changed']=len(seq)==len(base) and all(a[:11]==b[:11] and a[12:]==b[12:] for a,b in zip(seq,base)) and m['loss'][0] in (441,445) and m['loss'][1]==500 and all(bool(r[11])==(r[0]>=415 and not m['loss'][0]<=r[0]<=500) for r in seq)
 xr=rows(probe.parent/'native-xr/openxr.jsonl');mock=rows(probe.parent/'mock.jsonl');c.update(haptic_checks(native,xr,mock,m['loss'][0]))
 c['profile_files_unchanged']=all(hashlib.sha256((probe/dst).read_bytes()).hexdigest()==profile['files'][src] for src,dst in [('profile.txt','world-grip-preview.txt'),('hand-pose.bin','grip-hand-pose.bin')])
 if a.negative_tests:
  variants=[('no_abort_release','motion_offhand_release_abort','pulse_released',False),('false_tracking_loss','motion_offhand_release_abort','right_tracked',False),('no_ticket_revoke','motion_offhand_release_abort','ticket_revoked',False),('no_regrip','motion_support_sample','left_grab',False),('source_changed','motion_visual_pose','source_unchanged',False),('wrong_grip_policy','motion_offhand_release_expected','runtime_grab_action',False)]
  variants += [('unowned_block','motion_event_blocked','ticket_identified',False)] if m['blocked_events'] else [('resurrected','motion_collider_cancel_end','collider_after',999),('forced','motion_collider_cancel_begin','forced_fixture',True),('wrong_owner','motion_collider_cancel_begin','world','0x0')]
  for name,event,key,value in variants:
   bad=copy.deepcopy(native);hits=[r for r in bad if r['event']==event]
   if hits:hits[-1][key]=value
   c['reject_'+name]=bool(hits) and not all(audit(bad,seq,profile,tolerance)[0].values())
 if a.negative_tests:
  bad=copy.deepcopy(xr)
  for r in bad:
   if r['event']=='native_motion_haptic' and r['frame']==501:r['amplitude']=[.5,.5]
  c['reject_haptic_revival']=not all(haptic_checks(native,bad,mock,m['loss'][0]).values())
  bad=copy.deepcopy(xr)
  for r in bad:
   if r['event']=='native_motion_haptic' and r['frame']==m['loss'][0]:r['amplitude'][0]=.4
  c['reject_released_hand_vibration']=not all(haptic_checks(native,bad,mock,m['loss'][0]).values())
 for name in ('motion-hand-checks.json','motion-visual-checks.json'):
  checks=json.loads((probe/name).read_text())['checks'];c[name+'_PASS']=bool(checks) and all(v=='PASS' for v in checks.values())
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS';c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0;c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,limitation=__doc__);(probe/'offhand-release-checks.json').write_text(json.dumps(out,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(c.values()),'FAIL',sum(not v for v in c.values()),m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
