"""Calibrated two-hand display -> native physics inputs; actual target damage is separate."""
import argparse,copy,json
from pathlib import Path
from check_motion_collider_apply import audit as applied_audit
from check_calibrated_collider_shadow import audit as calibrated_audit
from check_motion_two_hand import rows,sequence

def audit(native,seq,profile,tolerance,early=False):
 c,m=applied_audit(native);cc,cm=calibrated_audit(native,seq,profile,tolerance)
 for key in ('query_and_ecs_not_written','native_transform_stays_original','no_actual_tracked_collision_change'):cc.pop(key)
 c.update(cc);m['max_support_gap_m']=cm['max_support_gap_m']
 if early:
  from check_event_retime import audit as retime_audit
  ec,em=retime_audit(native,seq);c.update({'early_'+k:v for k,v in ec.items()});m['early']=em
 else:c['no_early_Impact_in_this_stage']=not any(r['event']=='motion_event_retime' for r in native)
 enabled=[r for r in native if r['event']=='two_hand_attack_enabled'];c['two_hand_geometry_explicit']=len(enabled)==1 and enabled[0]['tracked_hit_geometry_enabled'] is True
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--early-impact',action='store_true');a=p.parse_args();probe=a.probe;native=rows(probe/'view-resource-probe.jsonl');seq=sequence(probe.parent/'mock-hands.csv');profile=json.loads((probe/'grip-preview.json').read_text());tolerance=float((probe/'world-two-hand-attack.txt').read_text().split()[1]);c,m=audit(native,seq,profile,tolerance,a.early_impact)
 for name in ('motion-two-hand-checks.json','motion-hand-checks.json','motion-visual-checks.json'):
  checks=json.loads((probe/name).read_text())['checks'];c[name+'_PASS']=bool(checks) and all(v=='PASS' for v in checks.values())
 if a.negative_tests:
  for name,event,key,value in [('offhand_lost','motion_collider_shadow','two_hand_support_valid',False),('support_not_required','motion_collider_shadow','two_hand_required',False),('wrong_hand_frame','motion_collider_shadow','hand_frame',390),('wrong_physics_pose','sequence_collider_sample','physics_current_bytes','00'*32),('original_changed','motion_collider_apply_end','original_components_unchanged',False),('false_readonly','two_hand_attack_enabled','tracked_hit_geometry_enabled',False)]:
   bad=copy.deepcopy(native)
   for e in bad:
    if e['event']==event:e[key]=value
   c['reject_'+name]=not all(audit(bad,seq,profile,tolerance,a.early_impact)[0].values())
 c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS'
 c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,limitation=__doc__);(probe/'calibrated-collider-apply-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
