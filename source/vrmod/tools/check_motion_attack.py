"""One mock-XR wrist swing -> native PresetA X -> action acknowledgement and completion.
This is not controller-directed collision, repeated combat, real haptics or damage proof.
"""
import argparse,copy,json,math,struct
from pathlib import Path
from check_action_sequences import decode

def rows(p):return [json.loads(x) for x in p.read_text(encoding='utf-8-sig').splitlines()]
def native_look_error(snapshot):
 # The tracking origin may have a nonzero world yaw. Derive game +Z forward
 # from the camera quaternion already matched to the HMD input producer;
 # never assume the world axis is the current HMD direction.
 q=snapshot.get('camera_orientation',[]);look=snapshot.get('world_look',[])
 if len(q)!=4 or len(look)!=3 or not all(isinstance(v,(int,float)) and math.isfinite(v) for v in [*q,*look]):return math.inf
 n=sum(v*v for v in q)
 if abs(n-1)>2e-4:return math.inf
 x,y,z,w=(v/math.sqrt(n) for v in q)
 expected=[2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)]
 return max(abs(a-b) for a,b in zip(look,expected))

def native_look_branch(snapshot):
 c=snapshot.get('native_look_context')
 if not isinstance(c,dict) or not c.get('read_only') or c.get('query_slots')!=[1,7,15]:return 'unproven',math.inf
 try:
  body=bytes.fromhex(c['body_transform_bytes']);mode=c['mode'];settings=c['settings'];post=c['post_look'];move=c['world_move']
  if len(body)!=40 or post==1 or len(move)!=3 or not all(math.isfinite(v) for v in move):return 'unproven',math.inf
  d=decode(bytes.fromhex(snapshot['base_actor_bytes']));state=d['effectiveState']
  # InRangeCombat can auto-aim. Unknown modes / post-look paths require
  # their own native proof instead of reusing the direct-camera branch.
  if state&(1<<25) or mode not in (1,3,4):return 'unproven',math.inf
  if mode in (3,4) or state&(1<<53) or (mode==1 and settings==0 and state&(1<<2)):
   return 'camera_289172',native_look_error(snapshot)
  if any(move):return 'unproven',math.inf
  q=list(struct.unpack_from('<4f',body,24))
  return 'body_337490',native_look_error(dict(camera_orientation=q,world_look=snapshot['world_look']))
 except (ValueError,KeyError,TypeError,struct.error):return 'unproven',math.inf

def check(native,sequence):
 ev=lambda name:[x for x in native if x['event']==name]
 samples=ev('motion_attack_sample');starts=[x for x in samples if x['gesture_request']];ends=[x for x in samples if x['phase']==5]
 active=[x for x in samples if x['active']];restore=ev('controller_fixture_restored');packets=ev('controller_attack_packet_polled')
 item=starts[0]['item'] if len(starts)==1 else 0
 decoded=[]
 for x in ev('action_sequence_snapshot'):
  d=decode(bytes.fromhex(x['base_actor_bytes']));d.update(us=x['us'],input_digital=x['input_digital'],entity=x['entity']);decoded.append(d)
 playing=[d for d in decoded if d['state']&1]
 speeds=[]
 for x in starts:
  i=x['frame']-1
  if 0<i<len(sequence):speeds.append(math.dist(sequence[i][14:17],sequence[i-1][14:17])/((x['time']-next((p['time'] for p in reversed(samples) if p['frame']==x['frame']-1),x['time']))*1e-9))
 c=dict(
  explicit_bounded_motion_policy=len(ev('motion_attack_fixture_enabled'))==1 and ev('motion_attack_fixture_enabled')[0]['one_swing_only'] and not ev('motion_attack_fixture_enabled')[0]['direct_damage_writes'],
  one_gesture_request=len(starts)==1 and starts[0]['request']==starts[0]['gesture_request']==1 and all(x['request'] in (0,1) for x in samples),
  independent_CSV_swing_speed=len(speeds)==1 and abs(speeds[0]-starts[0]['speed'])<2e-4 and 2<=speeds[0]<=12,
  no_attack_before_swing=bool(starts) and all(x['buttons']==0 and not x['accepted'] for x in samples if x['us']<starts[0]['us']),
  one_native_X_press_and_release=[x['buttons'] for x in packets]==[0,16384,0] and all(x['right_trigger']==0 for x in packets),
  native_input_consumed=bool(decoded) and any(x['input_digital']&1 for x in decoded) and any(x['input_digital']==10 for x in decoded) and all(x['input_digital'] in (0,1,10) for x in decoded),
  native_actor_acknowledges_item=bool(playing) and all(x['usedItem']==item and x['trigger_actorEntityId']==x['entity'] for x in playing),
  one_native_action=len({(x['trigger_sequenceId'],x['trigger_triggerTime']) for x in playing})==1,
  native_action_follows_gesture=bool(starts) and bool(playing) and min(x['us'] for x in playing)>starts[0]['us'],
  raw_recovery_ratio=bool(active) and all(x['total']==x['length'] and x['remaining']==x['length']-x['current_time'] and 0<=x['remaining']<=x['total'] for x in active),
  declining_haptic_calculation=any(x['haptic_diagnostic']>0 for x in active) and all(x['haptic_diagnostic']==0 or abs(x['haptic_diagnostic']-.6*x['remaining']/x['total'])<2e-6 for x in active),
  completes_after_native_idle=len(ends)==1 and ends[0]['request']==1 and ends[0]['accepted'] and not ends[0]['active'] and ends[0]['buttons']==0,
  valid_native_and_ui_samples=bool(samples) and all(x['observation_valid'] and x['normal_ui'] and x['phase']!=6 for x in samples),
  original_input_table_restored=len(restore)==1 and restore[0]['pointers_restored'] and restore[0]['native_connection_seen'] and restore[0]['native_disconnection_seen'] and not restore[0]['native_connected'] and not restore[0]['rescan_pending'],
  action_reader_clean=bool(ev('action_sequence_removed')) and ev('action_sequence_removed')[0]['restored'] and ev('action_sequence_removed')[0]['failures']==0 and ev('action_sequence_removed')[0]['callbacks']==0,
  no_failure_events=not any('failure' in x['event'] or 'failed' in x['event'] for x in native))
 if ev('melee_head_policy_enabled'):
  head=[x for x in ev('input_head_walking_sample') if x['applied'] and x.get('melee_head_evidence')]
  snapshots=ev('action_sequence_snapshot');activeSnapshots=[x for x in snapshots if decode(bytes.fromhex(x['base_actor_bytes']))['state']&1]
  matches=[];consumerDelays=[]
  for x in activeSnapshots:
   match=[h for h in head if h['us']<=x['us'] and x.get('camera_q32')==h['camera_q32'] and x.get('camera_orientation')==h['camera_orientation']]
   matches.append(bool(match))
   if match:consumerDelays.append(x['us']-max(h['us'] for h in match))
  # The producer's200ms freshness gate is checked in check_idle_head. Native
  # prediction/history may consume that exact camera later; report its delay
  # separately instead of treating a simulation tick as an input timestamp.
  c['melee_HMD_camera_during_actual_action']=bool(matches) and all(matches)
  if any('native_look_context' in x for x in activeSnapshots):
   c['melee_native_look_follows_proven_branch']=bool(activeSnapshots) and all(native_look_branch(x)[1]<2e-6 for x in activeSnapshots)
  else:
   # Retain the historical requirement/result; no missing branch evidence
   # is accepted retroactively merely because body-look is a plausible cause.
   c['melee_native_look_matches_HMD']=bool(activeSnapshots) and all(native_look_error(x)<2e-6 for x in activeSnapshots)
  c['melee_button_and_release_keep_head_aim']=any(h['native_digital']==1 for h in head) and any(h['native_digital']==10 for h in head)
  c['melee_original_control_bytes_preserved']=bool(head) and all(h['only_camera_bytes_changed'] and h['native_digital']&~15==0 and h['normal_ui_bound'] for h in head)
 return c,dict(samples=len(samples),native_active_samples=len(playing),request_frame=starts[0]['frame'] if starts else None,independent_speed=speeds,phases=sorted({x['phase'] for x in samples}),native_camera_consumer_delay_us=max(consumerDelays,default=None) if ev('melee_head_policy_enabled') else None,native_look_error=max((native_look_error(x) for x in activeSnapshots),default=None) if ev('melee_head_policy_enabled') else None)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();native=rows(a.probe/'view-resource-probe.jsonl');launch=a.probe.parent
 sequence=[[float(v) for v in line.split(',')] for line in (launch/'mock-hands.csv').read_text().splitlines() if line and not line.startswith('#')]
 checks,metrics=check(native,sequence)
 checks['exact_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/single-swing-hands.csv').read_bytes()
 checks['normal_exit']=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 checks['runner_PASS']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS'
 checks['clean_Core_Sync']=(launch/'validation.log').stat().st_size==0
 if a.negative_tests:
  for name,event,key,value in [('false_speed','motion_attack_sample','speed',0),('wrong_item','motion_attack_sample','item',0),('repeated_request','motion_attack_sample','request',2),('invalid_native','motion_attack_sample','observation_valid',False),('held_native_button','controller_attack_packet_polled','buttons',16384),('bad_haptic_ratio','motion_attack_sample','haptic_diagnostic',1),('unrestored_table','controller_fixture_restored','pointers_restored',False)]:
   bad=copy.deepcopy(native)
   for x in bad:
    if x['event']==event:x[key]=value
   checks['reject_'+name]=not all(check(bad,sequence)[0].values())
 if a.negative_tests and any(x['event']=='melee_head_policy_enabled' for x in native):
  bad=copy.deepcopy(native)
  for x in bad:
   if x['event']=='action_sequence_snapshot':x['camera_orientation']=[0,.5,0,.8660254]
  checks['reject_wrong_melee_camera']=not check(bad,sequence)[0]['melee_HMD_camera_during_actual_action']
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},metrics=metrics,limitation=__doc__);(a.probe/'motion-attack-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(metrics);print('PASS',sum(checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
