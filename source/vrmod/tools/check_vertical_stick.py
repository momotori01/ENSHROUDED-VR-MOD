"""Finite native L3/B routing from recorded OpenXR axes, not a full control scheme."""
import argparse,copy,hashlib,json
from pathlib import Path

def audit(rows):
 ev=lambda e:[r for r in rows if r.get('event')==e]
 front=ev('vertical_stick_frontend');polled=ev('controller_fixture_packet_polled');enabled=ev('vertical_stick_fixture_enabled');restored=ev('controller_fixture_restored')
 expected=lambda f:64 if 450<=f<=458 or 550<=f<=558 else 8192 if 620<=f<=628 else 0
 axis=lambda f:1 if 450<=f<=458 or 550<=f<=558 else -1 if 620<=f<=628 else 0
 valid=[r for r in front if r.get('actor_valid') and r['frame']>=440]
 checks=dict(explicit_finite_scope=len(enabled)==1 and enabled[0]['buttons_only']==[64,8192] and enabled[0]['native_state_writes'] is False and enabled[0]['end_frame']==750,
     exact_frame_axis_mapping=bool(front) and all(r['buttons']==r['mapped']==expected(r['frame']) and r['right_y']==axis(r['frame']) for r in front),
     one_current_owner_and_fresh_XR=bool(front) and len({r['owner'] for r in front})==1 and all(r['owner']>0 and 0<=r['age_ms']<=200 and r['screen']==1 for r in front),
     unique_increasing_samples=bool(front) and all(b['frame']>a['frame'] and b['time']>a['time'] for a,b in zip(front,front[1:])),
     native_consumed_edges=[r['buttons'] for r in polled]==[0,64,0,64,0,8192,0],
     actual_actor_values=bool(valid) and all(r['entity']==r['actor_entity'] and r['entity']>0 and r['actor_age_ms']<=200 for r in valid),
     unavailable_native_context_neutral=bool(front) and all(r['buttons']==0 and r.get('mapping_valid') is False and r['mode']==0 for r in front if not r.get('actor_valid')),
     fresh_context_required_for_commands=all(r['actor_valid'] and r.get('mapping_valid') is True and r.get('root_age_ms',10**9)<=200 and r['actor_age_ms']<=200 for r in front if r['buttons']),
     native_sneak_input=any(r['digital']&(1<<52) for r in valid),
     native_sneak_entered=any(450<=r['frame']<550 and r['actor_state']&(1<<21) for r in valid),
     native_sneak_left=any(580<=r['frame']<620 and not(r['actor_state']&(1<<21)) for r in valid),
     native_evade_input=any(r['digital']&(1<<29) for r in valid),
     native_dodge_state=any(620<=r['frame']<=700 and r['actor_state']&(1<<3) for r in valid),
     no_mainhand_attack_input=bool(valid) and all(not(r['digital']&15) for r in valid),
     final_release=bool(front) and front[-1]['frame']>=750 and front[-1]['buttons']==0,
     actual_input_restore=len(restored)==1 and restored[0].get('pointers_restored') and restored[0].get('native_connection_seen') and restored[0].get('native_disconnection_seen') and restored[0].get('native_connected')==0,
     no_fixture_failure=not any(r.get('event') in ('controller_fixture_failure','controller_fixture_failed','live_native_pair_failure','view_resource_probe_failure') for r in rows))
 return {k:bool(v) for k,v in checks.items()}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 source=a.probe/'view-resource-probe.jsonl';raw=source.read_bytes();rows=[json.loads(l) for l in raw.splitlines() if l.strip()];checks=audit(rows)
 if a.negative_tests:
  for label,event,change in [('wrong_axis','vertical_stick_frontend',{'right_y':1}),('stale_pose','vertical_stick_frontend',{'age_ms':201}),('wrong_actor','vertical_stick_frontend',{'entity':0,'actor_valid':True,'frame':450}),('attack_packet','controller_fixture_packet_polled',{'buttons':16384}),('not_restored','controller_fixture_restored',{'pointers_restored':False})]:
   bad=copy.deepcopy(rows);target=next((r for r in bad if r.get('event')==event),None)
   if target is None:checks['negative_'+label]=False;continue
   target.update(change);checks['negative_'+label]=not all(audit(bad).values())
 report=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,raw_sha256=hashlib.sha256(raw).hexdigest(),scope=__doc__)
 (a.probe/'vertical-stick-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
