"""Calibrated displayed pose at native collision boundary, read-only proposals only."""
import argparse,copy,json,math
from pathlib import Path
from check_motion_collider_shadow import audit as shadow_audit
from check_motion_two_hand import rows,sequence
from grip_preview_math import calibrated
from check_recorded_xr_native import rotate


def audit(native,seq,profile,tolerance):
 c,m=shadow_audit(native);proposals=[r for r in native if r['event']=='motion_collider_shadow'];eligible=[r for r in proposals if r['eligible']];hands={r['frame']:r for r in native if r['event']=='motion_hand_pose'};calibrated_poses={r['frame']:r for r in native if r['event']=='grip_preview_pose'}
 valid=bool(eligible);maxgap=0.
 for r in eligible:
  f=r['hand_frame'];row=seq[f-1];p,q,both=calibrated(row,profile);grip=[a+b for a,b in zip(p,rotate(q,profile['off_hand_in_scaled_model_XR']))];gap=math.dist(grip,row[1:4]);maxgap=max(maxgap,gap)
  valid &= r.get('two_hand_required') is True and r.get('two_hand_support_valid') is True and both and gap<=tolerance and f in hands and r['frame'] in calibrated_poses and calibrated_poses[r['frame']]['resource_verified']
 c['both_grips_valid_at_physics_callback']=valid
 c['calibrated_model_used']=any(r['event']=='motion_visual_created' and r.get('weapon_calibration')=='measured_local_grip' for r in native)
 c['no_actual_tracked_collision_change']=not any(r['event'] in ('motion_collider_apply_enabled','motion_collider_apply_begin','motion_event_retime') for r in native)
 m['max_support_gap_m']=maxgap;return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 native=rows(probe/'view-resource-probe.jsonl');seq=sequence(probe.parent/'mock-hands.csv');profile=json.loads((probe/'grip-preview.json').read_text());tolerance=float((probe/'world-two-hand-attack.txt').read_text().split()[1]);c,m=audit(native,seq,profile,tolerance)
 for name in ('motion-two-hand-checks.json','motion-hand-checks.json','motion-visual-checks.json'):
  checks=json.loads((probe/name).read_text())['checks'];c[name+'_PASS']=bool(checks) and all(v=='PASS' for v in checks.values())
 if a.negative_tests:
  for key,value in [('two_hand_required',False),('two_hand_support_valid',False),('hand_frame',390),('proposed_transform','00'*56)]:
   bad=copy.deepcopy(native)
   for r in bad:
    if r['event']=='motion_collider_shadow' and r['eligible']:r[key]=value
   c['reject_'+key]=not all(audit(bad,seq,profile,tolerance)[0].values())
  for name in ('motion_collider_apply_enabled','motion_event_retime'):
   c['reject_'+name]=not all(audit(native+[dict(event=name)],seq,profile,tolerance)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,limitation=__doc__);(probe/'calibrated-collider-shadow-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
