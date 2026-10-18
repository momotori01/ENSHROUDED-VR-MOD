"""Measured repeated coordinator + authored Impact + exact native tracked physics.
Enemy HP/contact is reported separately, never inferred from empty-swing success.
"""
import argparse,copy,json
from pathlib import Path
from check_melee_repeat import audit as session_audit
from check_motion_repeat import audit as repeat_audit
from check_calibrated_collider_shadow import audit as geometry_audit
from generate_melee_repeat_fixture import verify

def audit(native,sequence,xr,mock,profile,tolerance):
 c,m=repeat_audit(native,sequence,xr,mock,recovery_swing=True,melee_session=True)
 c.update({'session_'+k:v for k,v in session_audit(native,contact=True).items()})
 geometry,gm=geometry_audit(native,sequence,profile,tolerance)
 for key in ('query_and_ecs_not_written','native_transform_stays_original','no_actual_tracked_collision_change'):geometry.pop(key)
 c.update({'measured_'+k:v for k,v in geometry.items()});m['geometry']=gm
 ev=lambda n:[r for r in native if r['event']==n]
 starts=[r for r in ev('melee_session_command') if r['issued']];ends=[r for r in ev('melee_session_command') if r['retired']]
 proposals=[r for r in ev('motion_collider_shadow') if r['eligible']]
 c['each_physics_sample_has_live_session_request']=bool(proposals) and all(sum(a['us']<=r['us']<b['us'] and a['issued']==b['retired'] for a,b in zip(starts,ends))==1 for r in proposals)
 c['each_request_has_owned_physics']=len(starts)==len(ends)==2 and all(sum(a['us']<=r['us']<b['us'] for r in proposals)>=3 for a,b in zip(starts,ends))
 qualifications=ev('motion_event_retime_qualification')
 c['each_ticket_qualifies_original_Impact_once']=len(qualifications)==2 and [r['ticket_serial'] for r in qualifications]==[1,2] and all(r['motion'] and r['origin_valid'] and r['ticket_matches'] and r['qualified'] and 0<=r['ticket_age_ms']<=1000 and r['native_trigger']!=r['previous_trigger'] for r in qualifications)
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 if (probe/'world-melee-session-contact.txt').read_text().strip()!='EVR_MELEE_SESSION_CONTACT_2':raise ValueError('explicit measured contact mode required')
 read=lambda path:[json.loads(l) for l in path.read_text(encoding='utf-8-sig').splitlines()]
 native=read(probe/'view-resource-probe.jsonl');xr=read(probe.parent/'native-xr/openxr.jsonl');mock=read(probe.parent/'mock.jsonl');profile=json.loads((probe/'grip-preview.json').read_text());verify(probe.parent/'mock-hands.csv',profile)
 sequence=[[float(v) for v in l.split(',')] for l in (probe.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')];tolerance=float((probe/'world-two-hand-attack.txt').read_text().split()[1]);c,m=audit(native,sequence,xr,mock,profile,tolerance)
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS';c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0;c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 if a.negative_tests:
  for name,event,key,value in [('physics','sequence_collider_sample','physics_current_bytes','00'*32),('source','motion_collider_apply_end','original_components_unchanged',False),('duplicate_Impact','motion_event_retime_removed','appended',3),('support','motion_collider_shadow','two_hand_support_valid',False),('authority','motion_collider_shadow','owned_intent',False),('retire','melee_ticket_retired','valid',True),('ticket','motion_event_retime_qualification','ticket_serial',1),('original_asset','motion_event_retime','asset_written',True),('raw_owner','motion_owned_vm','entity',0),('input_restore','controller_fixture_restored','pointers_restored',False)]:
   bad=copy.deepcopy(native)
   for r in bad:
    if r['event']==event:r[key]=value
   c['reject_'+name]=not all(audit(bad,sequence,xr,mock,profile,tolerance)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,limitation=__doc__);(probe/'melee-contact-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()),'enemy_HP_proven',m.get('live_target_damage_proven'));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
