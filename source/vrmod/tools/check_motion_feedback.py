"""Bounded native-action recovery -> render-thread OpenXR vibration -> mock runtime evidence.
Checks API delivery, not physical tactile quality or hardware scheduling.
"""
import argparse,copy,json
from pathlib import Path
from check_action_sequences import decode

def rows(p):return [json.loads(x) for x in p.read_text(encoding='utf-8-sig').splitlines()]
def check(native,xr,mock,expected_attacks=1,two_hand=False):
 if expected_attacks not in (1,2):raise ValueError("bounded expected attacks")
 n=[x for x in native if x['event']=='motion_feedback_requested'];x=[x for x in xr if x['event']=='native_motion_haptic'];positive=[e for e in x if e['amplitude'][1]>0]
 calls=[e for e in mock if e['event']=='motion_haptic'];stops=[e for e in mock if e['event']=='motion_haptic_stop'];done=[e for e in native if e['event']=='motion_attack_sample' and e['phase']==5]
 by_frame={e['frame']:e for e in n};begins={e['frame']:e for e in xr if e['event']=='native_frame_begin'};ratios=[]
 for e in native:
  if e['event']=='action_sequence_snapshot':
   d=decode(bytes.fromhex(e['base_actor_bytes']))
   if d['state']&1 and d['sequenceLength']>0:ratios.append((e['us'],.6*(d['sequenceLength']-d['currentTime'])/d['sequenceLength']))
 c=dict(
  render_thread_request_matches_every_delivery=bool(x) and len(n)==len(x)==len(by_frame) and all(e['frame'] in by_frame and abs(e['amplitude'][1]-by_frame[e['frame']]['amplitude'])<1e-6 for e in x),
  same_XR_frame_and_generation=bool(x) and all(e['frame'] in begins and e['generation']==begins[e['frame']]['session_generation'] and 0<=e['frame']-by_frame[e['frame']]['source_frame']<=2 for e in positive),
  bounded_right_hand_only=bool(positive) and all(e['amplitude'][0]==0 and 0<=e['amplitude'][1]<=.6 and e['duration']==100000000 and all(e['delivered']) for e in x),
  observed_native_recovery_ratio=bool(positive) and all(any(0<=e['us']-us<=300000 and abs(amplitude-e['amplitude'][1])<2e-6 for us,amplitude in ratios) for e in positive),
  runtime_receives_same_haptic=bool(calls) and len(calls)==len(positive) and all(any(c['frame']==e['frame'] and c['hand']==1 and abs(c['amplitude']-e['amplitude'][1])<1e-6 and c['duration']==100000000 for c in calls) for e in positive),
  pulse_strength_declines=bool(positive) and positive[0]['amplitude'][1]>positive[-1]['amplitude'][1] and all(b['amplitude'][1]<=a['amplitude'][1]+1e-6 for a,b in zip(positive,positive[1:])),
  stops_after_attack=bool(done) and bool(positive) and max(e['us'] for e in positive)<done[0]['us']+200000 and all(e['amplitude']==[0,0] for e in x if e['us']>done[0]['us']+200000) and all(any(s['hand']==h and s['us']>positive[-1]['us'] for s in stops) for h in (0,1)),
  session_actions_closed=any(e['event']=='native_motion_closed' and e['success'] for e in xr),
  no_feedback_failure=not any('haptic_failed' in e['event'] for e in xr+native))
 if two_hand:
  c.pop('bounded_right_hand_only')
  c['bounded_both_hands']=bool(positive) and all(e['amplitude'][0]==e['amplitude'][1] and 0<=e['amplitude'][1]<=.6 and e['duration']==100000000 and all(e['delivered']) for e in x)
  c['runtime_receives_same_haptic']=bool(calls) and len(calls)==2*len(positive) and all(any(c['frame']==e['frame'] and c['hand']==h and abs(c['amplitude']-e['amplitude'][h])<1e-6 and c['duration']==100000000 for c in calls) for e in positive for h in (0,1))
 if expected_attacks==2:
  tickets=[e for e in native if e['event']=='native_attack_ticket'];groups=[[p for p in positive if t['us']<=p['us']<(tickets[i+1]['us'] if i+1<len(tickets) else float('inf'))] for i,t in enumerate(tickets)]
  c.pop('pulse_strength_declines')
  c['two_native_recovery_envelopes']=len(groups)==2 and all(g and g[0]['amplitude'][1]>g[-1]['amplitude'][1] and all(b['amplitude'][1]<=a['amplitude'][1]+1e-6 for a,b in zip(g,g[1:])) for g in groups)
 return c,dict(render_frames=len(x),positive_pulses=len(positive),first=positive[0]['amplitude'][1] if positive else None,last=positive[-1]['amplitude'][1] if positive else None)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--two-hand',action='store_true');p.add_argument('--expected-attacks',type=int,choices=(1,2),default=1);a=p.parse_args();native=rows(a.probe/'view-resource-probe.jsonl');launch=a.probe.parent;xr=rows(launch/'native-xr/openxr.jsonl');mock=rows(launch/'mock.jsonl')
 checks,metrics=check(native,xr,mock,expected_attacks=a.expected_attacks,two_hand=a.two_hand)
 checks['Core_Sync_clean']=(launch/'validation.log').stat().st_size==0
 checks['normal_exit']=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 if a.negative_tests:
  for name,event,key,value in [('unbounded_pulse','native_motion_haptic','duration',100000001),('old_session','native_motion_haptic','generation',0),('wrong_strength','native_motion_haptic','amplitude',[0,1]),('undelivered','native_motion_haptic','delivered',[False,False])]:
   bad=copy.deepcopy(xr)
   for e in bad:
    if e['event']==event:e[key]=value
   checks['reject_'+name]=not all(check(native,bad,mock,expected_attacks=a.expected_attacks,two_hand=a.two_hand)[0].values())
  bad=copy.deepcopy(mock)
  for e in bad:
   if e['event']=='motion_haptic':e['hand']=0
  checks['reject_wrong_runtime_hand']=not all(check(native,xr,bad,expected_attacks=a.expected_attacks,two_hand=a.two_hand)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},metrics=metrics,limitation=__doc__);(a.probe/'motion-feedback-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(metrics);print('PASS',sum(checks.values()),'FAIL',sum(not x for x in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
