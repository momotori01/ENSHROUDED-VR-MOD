"""Validate actual Mock launch displacement and native first-step-only correction."""
import argparse,json,math
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--max-error-deg',type=float,default=5);a=p.parse_args()
play=json.loads((a.launch/'play-session-check.json').read_text(encoding='utf-8-sig'));log=a.launch/play['probe']/'view-resource-probe.jsonl'
rows=[]
for line in log.open(encoding='utf-8-sig'):
 try:rows.append(json.loads(line))
 except ValueError:pass
basis=[e for e in rows if e.get('event')=='glide_native_basis'];applied=[e for e in basis if e.get('applied')]
roots=[e for e in rows if e.get('event')=='root_camera_sample'];removed=[e for e in rows if e.get('event')=='glide_launch_removed']
def yaw(q):
 x,y,z,w=q;return math.degrees(math.atan2(2*(x*z+w*y),1-2*(x*x+y*y)))
def error(a,b):return abs((a-b+180)%360-180)
measurements=[]
# One pair for each first-step episode observed on the prediction side. The
# server's optional query tick is absent; zero is not used as same-tick proof.
seen=set()
for e in applied:
 if e.get('caller')!='0x22aaf2' or not e.get('tick'):continue
 key=(e['entity'],e.get('native_trigger_time'))
 if key in seen:continue
 seen.add(key)
 start=min(roots,key=lambda r:abs(r['us']-e['us']));end=min(roots,key=lambda r:abs(r['us']-e['us']-250000))
 delta=[(y-x)/2**32 for x,y in zip(start['stable_q32'],end['stable_q32'])];horizontal=math.hypot(delta[0],delta[2]);heading=math.degrees(math.atan2(delta[0],delta[2]));head=yaw(e['camera_orientation'])
 measurements.append(dict(start_frame=start['frame'],end_frame=end['frame'],elapsed_s=(end['us']-start['us'])/1e6,delta_m=delta,horizontal_m=horizontal,head_deg=head,travel_deg=heading,error_deg=error(heading,head)))
checks=dict(play=play['status']=='PASS',native_bases_observed=bool(basis),launch_applied=bool(applied),prediction_and_authority=any(e['tick'] for e in applied) and any(not e['tick'] for e in applied),
 only_first_step=bool(applied) and all(e['native_played_time']==0 and e['valid'] and e['native_state']&(1<<37) for e in applied),
 same_input_tick_when_available=all(e['tick']==e['input_query_tick'] for e in applied if e['tick']),
 head_direction_at_launch=bool(applied) and all(error(yaw(e['camera_orientation']),yaw(e['chosen_orientation']))<.05 for e in applied),
 later_flight_keeps_native=any(e.get('native_played_time',0)>0 for e in basis) and all(not e['applied'] for e in basis if e.get('native_played_time',0)>0),
 actual_root_motion=bool(measurements) and all(m['horizontal_m']>.1 and .15<m['elapsed_s']<.35 and m['error_deg']<=a.max_error_deg for m in measurements),
 hook_restored=bool(removed) and all(e.get('restored') and not e.get('callbacks') and not e.get('failures') for e in removed),
 no_glide_failure=not any(e.get('event')=='glide_launch_failure' for e in rows))
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,applied_basis_calls=len(applied),basis_calls=len(basis),measurements=measurements,limit='Short Mock ground launch, not sustained flight, multiplayer or real-HMD validation. Tick zero only identifies the optional-table-absent branch.')
(a.launch/'glide-launch-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
