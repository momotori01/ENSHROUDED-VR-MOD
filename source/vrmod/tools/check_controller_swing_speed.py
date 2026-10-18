"""Independent controller-translation speed and twist rejection proof; not contact/damage proof."""
import argparse,copy,json,math,struct
from pathlib import Path
from generate_controller_speed_fixture import verify

F=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
def audit(native,seq,profile,expected_frames=(520,)):
 samples=[e for e in native if e['event']=='motion_attack_sample'];frames={e['frame']:e for e in samples}
 twist=[e for e in samples if 430<=e['frame']<=490];support={e['frame']:e for e in native if e['event']=='two_hand_attack_support'}
 velocity=[e for e in samples if e.get('velocity_previous_frame')];maximum=maximum_hand=0.;valid=bool(velocity)
 for e in velocity:
  prev=frames.get(e['velocity_previous_frame']);f=e['frame'];before=e['velocity_previous_frame']
  if not prev or not 0<before<f<=len(seq) or prev['time']>=e['time']:valid=False;continue
  dt=(e['time']-prev['time'])*1e-9
  v=[[(F(seq[f-1][i+k])-F(seq[before-1][i+k]))/dt for k in range(3)] for i in (1,14)]
  norms=[math.sqrt(sum(x*x for x in a)) for a in v];total=[a+b for a,b in zip(*v)];n=math.sqrt(sum(x*x for x in total))
  speed=max(0.,*[sum(a*b/n for a,b in zip(hand,total)) for hand in v]) if n>1e-6 else 0.
  if e.get('speed_basis')=='controller_translation_v2_right_two_hand':speed=norms[1]
  hs=e.get('controller_speed',[])
  if len(hs)!=2 or any(not math.isfinite(x) for x in hs) or not math.isfinite(e['speed']):valid=False;continue
  maximum=max(maximum,abs(speed-e['speed']));maximum_hand=max(maximum_hand,max(abs(a-b) for a,b in zip(norms,hs)))
 required=[e for e in samples if e['blocked'] in (0,7,8,9,10)]
 checks=dict(
  controller_speed_basis_explicit=bool(samples) and len({e.get('speed_basis') for e in samples})==1 and all(e.get('speed_basis') in ('controller_translation_v1','controller_translation_v2_right_two_hand') for e in samples),
  velocity_previous_sample_provenance=bool(required) and all(e.get('velocity_previous_frame',0)>0 for e in required) and valid,
  independent_controller_speed=len(velocity)>=60 and valid and maximum<3e-4 and maximum_hand<3e-4,
  both_held_twists_observed=len(twist)>=50 and all(support.get(e['frame'],{}).get('left_grab') and support[e['frame']]['left_tracked'] for e in twist),
  twist_zero_speed_no_attack=bool(twist) and all(e['speed']==0 and e['buttons']==e['gesture_request']==e['request']==0 and not e['active'] and not e['accepted'] and e['blocked']==0 and e.get('controller_speed')==[0,0] for e in twist),
  controller_translation_one_native_request=[e['frame'] for e in samples if e['gesture_request']]==list(expected_frames),
  no_early_native_ticket=[e['frame'] for e in native if e['event']=='native_attack_ticket']==list(expected_frames),
 )
 return checks,dict(velocity_samples=len(velocity),twist_samples=len(twist),speed_basis=sorted({e.get('speed_basis','') for e in samples}),max_speed_error=maximum,max_hand_speed_error=maximum_hand)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();probe=a.probe
 native=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()];profile=json.loads((probe/'grip-preview.json').read_text());right_only=(probe/'right-hand-speed-policy.txt').exists()
 repeat=(probe/'world-melee-session.txt').exists() and (probe/'world-melee-session.txt').read_text().strip()=='EVR_MELEE_SESSION_REPEAT_2'
 expected_frames=(520,600) if repeat else (520,)
 if repeat:
  from generate_melee_repeat_fixture import verify as verify_repeat
  fixture=verify_repeat(probe.parent/'mock-hands.csv',profile)
 elif right_only:
  from generate_right_hand_speed_fixture import verify as verify_right
  fixture=verify_right(probe.parent/'mock-hands.csv',profile)
 else:fixture=verify(probe.parent/'mock-hands.csv',profile)
 seq=[[float(x) for x in l.split(',')] for l in (probe.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')];c,m=audit(native,seq,profile,expected_frames)
 if right_only:
  left=[e for e in native if e['event']=='motion_attack_sample' and 500<=e['frame']<=512];start=[e for e in native if e['event']=='motion_attack_sample' and e.get('gesture_request')]
  c['right_hand_policy_v2']=all(e.get('speed_basis')=='controller_translation_v2_right_two_hand' for e in native if e['event']=='motion_attack_sample')
  c['left_only_fast_rejected']=len(left)==13 and all(e['speed']==0 and e['controller_speed'][1]==0 and not e['request'] and not e['gesture_request'] and not e['buttons'] and not e['active'] and e['blocked']==0 and (e['frame']==506 or e['controller_speed'][0]>3) for e in left)
  c['right_only_with_stationary_left_accepted']=[e['frame'] for e in start]==list(expected_frames) and all(e['controller_speed'][0]==0 and 3<e['speed']<4 and abs(e['speed']-e['controller_speed'][1])<1e-6 for e in start)
  m['left_only_samples']=len(left)
 c['runner_PASS']=json.loads((probe/'probe-status.json').read_text())['status']=='PASS'
 c['normal_exit']=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['Core_Sync_clean']=(probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  for key,value in [('speed_basis','weapon_tip'),('velocity_previous_frame',0),('speed',10),('controller_speed',[0,10]),('gesture_request',1)]:
   bad=copy.deepcopy(native)
   for e in bad:
    if e['event']=='motion_attack_sample' and 430<=e['frame']<=490:e[key]=value
   c['reject_'+key]=not all(audit(bad,seq,profile,expected_frames)[0].values())
  for key,value in [('left_grab',False),('left_tracked',False)]:
   bad=copy.deepcopy(native)
   for e in bad:
    if e['event']=='two_hand_attack_support' and 430<=e['frame']<=490:e[key]=value
   c['reject_'+key]=not all(audit(bad,seq,profile,expected_frames)[0].values())
 (probe/'controller-speed-checks.json').write_text(json.dumps(dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,fixture=fixture,limits=__doc__),indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);print('PASS',sum(c.values()),'FAIL',sum(not v for v in c.values()));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
