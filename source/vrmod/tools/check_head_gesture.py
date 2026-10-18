"""Bounded native temple gesture proof; does not certify physical reach or unlimited input."""
import argparse,copy,json
from pathlib import Path

def rows(p):return [json.loads(s) for s in p.read_text(encoding='utf-8-sig').splitlines() if s.strip()]
def events(data,name):return [x for x in data if x.get('event')==name]
# hand=1: right temple (EVR_HEAD_GESTURE_MENU_1); hand=0: left temple (EVR_HEAD_GESTURE_MENU_LEFT_1,
# 2026-10-05 18:50 user spec). The menu frames, buttons and native expectations are identical.
POS={1:(14,15,16,'hand_in_head'),0:(1,2,3,'left_hand_in_head')}
def verify(native,xr,mock,sequence,hand=1):
 ev=lambda n:events(native,n)
 sample=ev('head_gesture_sample');front=ev('head_gesture_frontend');polled=ev('controller_fixture_packet_polled');ui=ev('input_ui_native_sample')
 haptics=events(xr,'native_gesture_haptic');hm={x['frame']:x for x in haptics};sm={x['frame']:x for x in sample}
 entries=[x for x in sample if x['entered']];leaves=[x for x in sample if x['left']]
 expectedButtons=lambda f:16 if f in (450,580) else 8 if 500<=f<=502 else 0
 valid=bool(sample);positionError=0
 for s in sample:
  f=s['frame'];r=sequence[min(f-1,len(sequence)-1)];px,py,pz,key=POS[hand];target=[r[px],r[py]-1.65,r[pz]]
  positionError=max(positionError,max(abs(a-b) for a,b in zip(s.get(key,[9,9,9]),target)))
  if hand==0 or 'active_hands' in s:valid &= s.get('active_hands')==((1<<hand) if s['active'] else 0) and (not s['entered'] or s.get('entered_hand')==hand)
  valid &= s['valid'] and s['active']==(438<=f<620) and s['entered']==(f==438) and s['left']==(f==620)
  valid &= s['buttons']==s['mapped_buttons']==expectedButtons(f) and s['raw_buttons']==(128 if expectedButtons(f)==16 else 0) and s['raw_right']==r[32:34]
  valid &= abs(s['pulse']-(.15 if f==438 else 0))<1e-6 and s['pulse_duration']==(30000000 if f==438 else 0)
 entryTime=entries[0]['time'] if len(entries)==1 else -1
 pulseOK=bool(haptics) and len(hm)==len(haptics)==len(sample) and hm.keys()==sm.keys()
 for f,h in hm.items():
  s=sm.get(f,{});dt=s.get('time',0)-entryTime;remaining=30000000-dt
  expected=.15 if 0<=dt<30000000 else 0
  pulseOK &= h['delivered']==[True,True] and h['amplitude'][1-hand]==0 and abs(h['amplitude'][hand]-expected)<1e-6
  pulseOK &= h['duration'][hand]==(remaining if expected else 100000000)
 actualVib=events(mock,'motion_haptic');wanted=[(x['frame'],hand,x['amplitude'][hand],x['duration'][hand]) for x in haptics if x['amplitude'][hand]]
 actual=[(x['frame'],x['hand'],x['amplitude'],x['duration']) for x in actualVib]
 mockOK=len(actual)==len(wanted) and all(a[:2]==b[:2] and abs(a[2]-b[2])<1e-6 and a[3]==b[3] for a,b in zip(actual,wanted))
 captures=events(xr,'native_fixture_capture_complete');requests=ev('head_gesture_capture_requested');restored=ev('controller_fixture_restored');connections=ev('controller_fixture_native_connection');pairs=ev('live_native_pair_complete')
 checks=dict(
  explicit_menu_only_fixture=len(ev('head_gesture_fixture_enabled'))==1 and ev('head_gesture_fixture_enabled')[0]['automatic_confirmation'] is False,
  deterministic_CSV_gesture_and_mapping=bool(valid) and positionError<2e-6,
  one_entry_exit=[x['frame'] for x in entries]==[438] and [x['frame'] for x in leaves]==[620],
  same_epoch_and_increasing_samples=len(sm)==len(sample) and len({(x['session'],x['origin']) for x in sample})==1 and all(b['frame']==a['frame']+1 and b['time']>a['time'] for a,b in zip(sample,sample[1:])),
  frontend_uses_fresh_exact_sample=bool(front) and all(x['frame'] in sm and x['buttons']==sm[x['frame']]['buttons'] and 0<=x['age_ms']<=300 and x['screen'] in (1,4) for x in front),
  consumed_native_edges=[x['buttons'] for x in polled]==[0,16,0,8,0,16,0],
  release_before_disconnect=bool(polled) and bool(ev('controller_fixture_disconnected')) and polled[-1]['buttons']==0 and polled[-1]['us']<ev('controller_fixture_disconnected')[0]['us'],
  short_bounded_haptic_matches_frame_time=bool(pulseOK),
  runtime_received_exact_haptics=mockOK,
  native_controller_menu=any(x['hud_screen']==4 and x.get('frontend_input_mode')==1 and x.get('ui_gamepad_mode')==1 for x in ui),
  native_controller_HUD_return=bool(front) and front[-1]['frame']>=650 and front[-1]['screen']==1 and any(x['hud_screen']==1 and x.get('local_input_mode')==1 and x['us']>front[-1]['us'] for x in ui),
  native_connection_lifecycle=[x['connected'] for x in connections]==[True,False] and all(x['rescan_pending']==0 and x['state_calls']>0 for x in connections),
  backend_table_restored=len(restored)==1 and restored[0]['pointers_restored'] and restored[0]['native_connection_seen'] and restored[0]['native_disconnection_seen'] and restored[0]['rescan_pending']==0 and restored[0]['native_connected']==0,
  capture_sequence=[x['tag'] for x in requests]==[1,3,2] and [x['tag'] for x in captures]==[1,3,2] and all(x['ui_submitted'] for x in captures),
  both_eyes_continue=len(pairs)==600,
  no_failures=not any('failure' in x['event'] or 'failed' in x['event'] for x in native),
 )
 actors=ev('input_actor_final_sample');checks['no_gameplay_or_player_movement']=bool(actors) and all(x['desired_local']==[0,0,0] and x['world_move']==[0,0,0] and x['client_state_mask']==0 for x in actors)
 for n in ['input_ui_observer_removed','input_direction_observer_removed','input_sender_observer_removed','input_pipeline_removed','input_actor_final_removed','input_ranged_removed','input_picking_removed']:
  e=ev(n);checks[n]=len(e)==1 and e[0]['restored'] and e[0].get('failures',0)==0 and e[0].get('callbacks',e[0].get('remaining_callbacks',0))==0
 head=ev('input_head_walking_sample');during=[]
 for h in head:
  seen=[x for x in ui if x['us']<=h['us']]
  if seen and max(seen,key=lambda x:x['us'])['hud_screen']==4:during.append(h)
 checks['head_input_native_during_menu']=bool(during) and all(not x['applied'] and x['native_preserved'] and not x['normal_ui_bound'] for x in during)
 closed=[x for x in requests if x['tag']==2]
 checks['head_input_resumes_after_HUD']=len(closed)==1 and any(x['us']>closed[0]['us'] and x['applied'] and x['normal_ui_bound'] for x in head)
 for tag,screen in ((1,4),(3,4),(2,1)):
  r=[x for x in requests if x['tag']==tag];c=[x for x in captures if x['tag']==tag]
  checks[f'capture_{tag}_native_screen']=len(r)==len(c)==1 and r[0]['screen']==screen and r[0]['us']<c[0]['us']
 return checks,dict(head_relative_position_error=positionError,samples=len(sample),runtime_pulses=len(actualVib))

def ev0(data,name):
 e=events(data,name);return e[0] if e else {}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();root=a.probe.parent
 native=rows(a.probe/'view-resource-probe.jsonl');xr=rows(root/'native-xr/openxr.jsonl');mock=rows(root/'mock.jsonl')
 seq=[[float(v) for v in l.split(',')] for l in (root/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')]
 token=(a.probe/'controller-ui-fixture.txt').read_text().strip();hand=0 if token=='EVR_HEAD_GESTURE_MENU_LEFT_1' else 1
 if token not in ('EVR_HEAD_GESTURE_MENU_1','EVR_HEAD_GESTURE_MENU_LEFT_1'):raise ValueError('not a head gesture menu probe')
 checks,metrics=verify(native,xr,mock,seq,hand)
 checks['approved_hand_fixture']=(root/'mock-hands.csv').read_bytes()==Path('vrmod/test_env/'+('world-head-gesture-left-hands.csv' if hand==0 else 'world-head-gesture-hands.csv')).read_bytes()
 checks['fixture_hand_logged']=hand==1 and 'hand' not in ev0(native,'head_gesture_fixture_enabled') or ev0(native,'head_gesture_fixture_enabled').get('hand')==('left' if hand==0 else 'right')
 checks['normal_exit']=json.loads((root/'process-exit.json').read_text(encoding='utf-8-sig')).get('exit_code')==0
 checks['Core_Sync_clean']=(root/'validation.log').stat().st_size==0
 checks['runner_PASS']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS'
 if a.negative_tests:
  cases=[('head_gesture_sample','buttons',128),('head_gesture_sample','active',False),('head_gesture_sample',POS[hand][3],[0,0,0]),('head_gesture_frontend','age_ms',301),('controller_fixture_packet_polled','buttons',128),('controller_fixture_restored','pointers_restored',False)]
  if hand==0:cases+=[('head_gesture_sample','active_hands',2),('head_gesture_sample','entered_hand',1)]
  for event,key,value in cases:
   bad=copy.deepcopy(native)
   for x in bad:
    if x['event']==event:x[key]=value
   checks['reject_'+event+'_'+key]=not all(verify(bad,xr,mock,seq,hand)[0].values())
  for key,value in [('amplitude',[0,1]),('duration',[100000000,100000000])]:
   bad=copy.deepcopy(xr)
   for x in bad:
    if x['event']=='native_gesture_haptic':x[key]=value
   checks['reject_haptic_'+key]=not all(verify(native,bad,mock,seq,hand)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},metrics=dict(metrics,hand='left' if hand==0 else 'right'),limits='Mock temple Start/DpadRight only; Select, other directions, left-stick movement during the gesture (CPU-tested only), physical reach/haptic feel and combat interlock are separate tests.')
 (a.probe/'head-gesture-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(metrics);print('PASS',sum(checks.values()),'FAIL',sum(not x for x in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
