"""Same-frame head gesture/native attack interlock and merged XR haptics. No menu-transition or target-damage claim."""
import argparse,copy,json,math
from pathlib import Path
from check_motion_attack import check as attack_check
from generate_head_combat_fixture import verify

def rows(p):return [json.loads(l) for l in p.read_text(encoding='utf-8-sig').splitlines()]
def audit(native,xr,mock,seq):
 ev=lambda data,name:[e for e in data if e['event']==name]
 c,m=attack_check(native,seq);samples=ev(native,'motion_attack_sample');head=ev(native,'head_gesture_sample');heads={e['frame']:e for e in head};hs=ev(xr,'native_gesture_haptic');byh={e['frame']:e for e in hs}
 entry=[e for e in head if e['entered']];exit=[e for e in head if e['left']];guards=[e for e in samples if 440<=e['frame']<=481];after=[e for e in samples if 482<=e['frame']<520];enabled=ev(native,'head_combat_fixture_enabled')
 c.update(dict(
  explicit_head_combat_policy=len(enabled)==1 and enabled[0]['same_frame_policy'] and not enabled[0]['menu_commands'] and enabled[0]['native_action_unchanged'],
  same_frame_immutable_policy=bool(samples) and all(e.get('gesture_guard') and e.get('gesture_frame')==e['frame'] and e.get('gesture_time')==e['time'] and e['frame'] in heads and e.get('gesture_active')==heads[e['frame']]['active'] and e.get('gesture_reset')==heads[e['frame']]['reset_swing'] for e in samples),
  entry_and_exit_match_tracked_head=[e['frame'] for e in entry]==[448] and [e['frame'] for e in exit]==[481],
  head_mapping_geometry=bool(head) and all(e['valid'] and max(abs(a-b) for a,b in zip(e['hand_in_head'],[seq[e['frame']-1][14],seq[e['frame']-1][15]-1.65,seq[e['frame']-1][16]]))<2e-6 and e['active']==(448<=e['frame']<481) and e['buttons']==e['mapped_buttons']==0 for e in head),
  reserved_region_blocks_fast_controller_motion=len(guards)>=35 and {440,481}<={e['frame'] for e in guards} and all(e['blocked']==3 and not e['request'] and not e['gesture_request'] and not e['buttons'] and not e['active'] for e in guards),
  no_queued_exit_swing=len(after)>=30 and all(not e['request'] and not e['buttons'] for e in after),
  only_fresh_post_neutral_swing=[e['frame'] for e in samples if e['gesture_request']]==[520],
  one_haptic_call_per_frame=len(hs)==len(byh)==len(head) and byh.keys()==heads.keys() and not ev(xr,'native_motion_haptic') and all(all(e['delivered']) and e['generation']==heads[e['frame']]['session'] for e in hs),
 ))
 # Verify raw speed at entry/within gesture/exit, not the intentionally zeroed
 # blocked Intent speed. Both arms translate equally in this fixture.
 c['suppressed_paths_really_fast']=all(math.dist(seq[f-1][14:17],seq[f-2][14:17])*90>=2 for f in [440,*range(456,476),481,*range(482,490)])
 entrytime=entry[0]['time'] if len(entry)==1 else -1
 pulse=[];cooldown=[];valid=bool(hs)
 for h in hs:
  f=h['frame'];t=heads.get(f,{}).get('time',0);delta=t-entrytime;amp=h['amplitude'];duration=h['duration']
  if f<520:
   expected=.15 if 0<=delta<30000000 else 0
   valid &= amp[0]==0 and abs(amp[1]-expected)<1e-6 and duration==[100000000,30000000-delta if expected else 100000000]
   if expected:pulse.append(f)
  elif max(amp)>0:
   valid &= 0<amp[0]==amp[1]<=.6 and duration==[100000000,100000000] and any(e['active'] and e['total']>0 and 0<=h['us']-e['us']<=300000 and abs(amp[1]-.6*e['remaining']/e['total'])<2e-6 for e in samples)
   cooldown.append(h)
 c['pulse_and_native_recovery_both_preserved']=valid and pulse==[448,449,450] and len(cooldown)>3 and cooldown[0]['amplitude'][1]>cooldown[-1]['amplitude'][1] and all(b['amplitude'][1]<=a['amplitude'][1]+1e-6 for a,b in zip(cooldown,cooldown[1:]))
 expected=[(h['frame'],i,h['amplitude'][i],h['duration'][i]) for h in hs for i in (0,1) if h['amplitude'][i]>0];actual=[(e['frame'],e['hand'],e['amplitude'],e['duration']) for e in ev(mock,'motion_haptic')]
 c['runtime_receives_exact_merged_haptics']=len(expected)==len(actual) and all(a[:2]==b[:2] and abs(a[2]-b[2])<1e-6 and a[3]==b[3] for a,b in zip(expected,actual))
 c['haptics_stop_and_session_closes']=bool(cooldown) and all(max(h['amplitude'])==0 for h in hs[-10:]) and any(e['event']=='native_motion_closed' and e['success'] for e in xr)
 c['same_controller_translation_policy']=bool(samples) and len({e.get('speed_basis') for e in samples})==1 and all(e.get('speed_basis') in ('controller_translation_v1','controller_translation_v2_right_two_hand') for e in samples)
 return c,dict(attack=m,guarded_samples=len(guards),post_exit_samples=len(after),gesture_pulse_frames=pulse,recovery_pulses=len(cooldown))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe;launch=probe.parent
 native=rows(probe/'view-resource-probe.jsonl');xr=rows(launch/'native-xr/openxr.jsonl');mock=rows(launch/'mock.jsonl');profile=json.loads((probe/'grip-preview.json').read_text());fixture=verify(launch/'mock-hands.csv',profile)
 seq=[[float(v) for v in l.split(',')] for l in (launch/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')];c,m=audit(native,xr,mock,seq)
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS'
 c['normal_exit']=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['Core_Sync_clean']=(launch/'validation.log').stat().st_size==0
 if a.negative_tests:
  for key,value in [('gesture_frame',0),('gesture_time',0),('gesture_guard',False),('gesture_reset',False),('blocked',0),('buttons',16384),('gesture_request',1)]:
   bad=copy.deepcopy(native)
   for e in bad:
    if e['event']=='motion_attack_sample' and 440<=e['frame']<=481:e[key]=value
   c['reject_'+key]=not all(audit(bad,xr,mock,seq)[0].values())
  for frame,amp in [(448,[0,0]),(530,[0,1])]:
   bad=copy.deepcopy(xr)
   for e in bad:
    if e['event']=='native_gesture_haptic' and e['frame']==frame:e['amplitude']=amp
   c['reject_haptic_'+str(frame)]=not all(audit(native,bad,mock,seq)[0].values())
 (probe/'head-combat-checks.json').write_text(json.dumps(dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,fixture=fixture,limits=__doc__),indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);print('PASS',sum(bool(v) for v in c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
