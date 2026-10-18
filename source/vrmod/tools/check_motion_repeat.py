"""Two-swing integration audit. Exactly two independent native actions are required.
The legacy single-swing checkers keep their original defaults. No observed hit
or HP change is invented; empty swings establish lifecycle/geometry only.
"""
import argparse,copy,json,math,pathlib,struct
from check_action_sequences import decode
from check_owned_health import audit as health_audit
from check_motion_collider_apply import audit as collider_audit
from check_motion_feedback import check as feedback_audit
from check_motion_attack import check as single_attack_audit
from emulate_event_selection import select_reference,IMPACT

def audit(rows,sequence,xr,mock,recovery_swing=False,melee_session=False):
 expected_frames=[520,600] if melee_session else [440,550]
 recovery_range=(532,537) if melee_session else (450,461)
 rest_begin=551 if melee_session else 480
 ev=lambda n:[r for r in rows if r['event']==n]
 samples=ev('motion_attack_sample');starts=[r for r in samples if r['gesture_request']];tickets=ev('native_attack_ticket');enabled=ev('motion_attack_fixture_enabled')
 c={'explicit_two_attack_policy':len(enabled)==1 and not enabled[0]['one_swing_only'] and enabled[0]['expected_attacks']==2 and not enabled[0]['direct_damage_writes'], 'two_requests':len(starts)==2 and [r['request'] for r in starts]==[1,2] and [r['frame'] for r in starts]==expected_frames}
 if not c['two_requests']:return c,dict(request_frames=[r['frame'] for r in starts])
 c['separate_tickets']=len(tickets)==2 and [r['request'] for r in tickets]==[1,2] and tickets[0]['serial']!=tickets[1]['serial'] and all(r['motion'] and r['session'] and r['origin'] for r in tickets)
 speeds=[]
 for r in starts:
  previous=next((p for p in reversed(samples) if p['frame']==r['frame']-1),None);frame=r['frame']
  speed=math.dist(sequence[frame-1][14:17],sequence[frame-2][14:17])/((r['time']-previous['time'])*1e-9) if previous else -1;speeds.append(speed)
 c['independent_two_CSV_speeds']=all(2<=v<=12 and abs(v-r['speed'])<2e-4 for v,r in zip(speeds,starts))
 c['separate_X_pulses']=[r['buttons'] for r in ev('controller_attack_packet_polled')]==[0,16384,0,16384,0] and all(r['right_trigger']==0 for r in ev('controller_attack_packet_polled'))
 active=[]
 for r in ev('action_sequence_snapshot'):
  d=decode(bytes.fromhex(r['base_actor_bytes']));d.update(us=r['us'],entity=r['entity'],digital=r['input_digital'],world=r.get('world','0x0'));
  if d['state']&1:active.append(d)
 # A predicted/history actor copy can have no resolved ECS World and a
 # provisional trigger time. Count actual actions in the independently owned
 # Impact World only; retain and validate all camera/input observations.
 impact_worlds={r['world'] for r in ev('motion_event_retime') if r['operation']=='append'}
 c['one_owned_Impact_World']=len(impact_worlds)==1 and '0x0' not in impact_worlds
 qualified=[r for r in active if r['world'] in impact_worlds and r['world']!='0x0']
 c['qualified_actor_world_present']=bool(qualified) and all(r['world']=='0x0' or r['world'] in impact_worlds for r in active)
 actions=sorted({(r['trigger_sequenceId'],r['trigger_triggerTime']) for r in qualified},key=lambda k:min(r['us'] for r in qualified if (r['trigger_sequenceId'],r['trigger_triggerTime'])==k))
 c['qualified_actions_equal_owned_Impacts']={(r['trigger_sequenceId'],r['trigger_triggerTime']) for r in qualified}=={(r['sequence'],r['trigger']) for r in ev('motion_event_retime') if r['operation']=='append'}
 c['two_native_acks']=len(actions)==2 and all(r['usedItem']==starts[0]['item'] and r['trigger_actorEntityId']==r['entity'] for r in active)
 if len(actions)==2:
  for i,k in enumerate(actions):
   group=[r for r in qualified if (r['trigger_sequenceId'],r['trigger_triggerTime'])==k]
   c['native_action_after_request_'+str(i+1)]=min(r['us'] for r in group)>starts[i]['us'] and (i==1 or max(r['us'] for r in group)<starts[1]['us'])
 c['idle_and_neutral_between_actions']=any(r['completed_attacks']==1 and r['phase']==2 and not r['active'] and r['buttons']==0 and starts[0]['us']<r['us']<starts[1]['us'] for r in samples)
 c['no_request_during_recovery']=all(not r['gesture_request'] for r in samples if r['active'] or r['remaining']>0)
 if recovery_swing:
  attempted=[r for r in samples if recovery_range[0]<=r['frame']<=recovery_range[1]]
  recovering=[r for r in attempted if r['active'] and r['remaining']>0]
  c['separate_fast_swing_during_recovery']=len(recovering)>=6 and all(2<r['speed']<12 and not r['gesture_request'] and r['request']==1 and r['buttons']==0 and r['blocked']==7 for r in recovering)
  c['cooldown_speeds_independently_match']=bool(attempted) and all(abs(math.dist(sequence[r['frame']-1][14:17],sequence[r['frame']-2][14:17])/((r['time']-next(p['time'] for p in reversed(samples) if p['frame']==r['frame']-1))*1e-9)-r['speed'])<2e-4 for r in attempted)
  c['no_queued_swing_after_recovery']=all(not r['gesture_request'] and not r['buttons'] and not r['active'] for r in samples if rest_begin<=r['frame']<expected_frames[1])

 c['native_recovery_ratios']=bool(active) and all(r['remaining']==r['length']-r['current_time'] and r['total']==r['length'] and 0<=r['remaining']<=r['total'] for r in samples if r['active'])
 ends=[r for r in samples if r['phase']==5];c['completed_exactly_two']=len(ends)==1 and ends[0]['completed_attacks']==2 and ends[0]['request']==2 and ends[0]['accepted'] and not ends[0]['active'] and ends[0]['buttons']==0
 c['valid_UI_and_actor']=all(r['observation_valid'] and r['normal_ui'] and r['phase']!=6 for r in samples)
 restored=ev('controller_fixture_restored');c['native_input_restored']=len(restored)==1 and restored[0]['pointers_restored'] and restored[0]['native_connection_seen'] and restored[0]['native_disconnection_seen'] and not restored[0]['native_connected'] and not restored[0]['rescan_pending']
 c['no_failed_events']=not any('failure' in r['event'] or 'failed' in r['event'] for r in rows)
 cc,cm=collider_audit(rows);cc.pop('one_local_collider_lifetime');cc.pop('explicit_one_swing_apply_mode');c.update({'physics_'+k:v for k,v in cc.items()})
 begin=ev('motion_collider_apply_begin');finish={r['ready_sample']:r for r in ev('motion_collider_apply_end')};entities=sorted({r['entity'] for r in begin})
 c['two_collider_lifetimes']=len(entities)==2
 for eid in entities:
  group=[r for r in begin if r['entity']==eid];ids=[(r['collider_before'],finish.get(r['ready_sample'],{}).get('collider_after')) for r in group]
  c['independent_collider_'+str(eid)]=len(ids)>=3 and ids[0][0]==0 and all(a[1]==b[0] and a[1] for a,b in zip(ids,ids[1:]))
 c['collider_handles_not_reused']=len({finish[r['ready_sample']]['collider_after'] for r in begin})==2
 c['every_pose_has_current_authority']=all(r['owned_intent'] for r in ev('motion_collider_shadow') if r['eligible'])
 changes=ev('motion_event_retime');selection=ev('motion_event_selection');key=lambda r:tuple(r[k] for k in ['world','entity','sequence','item','trigger']);keys=list(dict.fromkeys(key(r) for r in changes));c['two_retime_actions']=len(keys)==2
 assets=ev('sequence_asset_snapshot');ready=ev('sequence_collider_ready_sample')
 for index,k in enumerate(keys):
  delta=[r for r in changes if key(r)==k];sel=[r for r in selection if key(r)==k];ok=len(delta)==2 and [r['operation'] for r in delta]==['append','suppress']
  c['append_suppress_pair_'+str(index)]=ok
  if not ok or not sel:continue
  early,late=delta;added=[v for v in early['after'] if v not in early['before']];c['preserve_other_events_'+str(index)]=len(added)==1 and early['after']==early['before']+added and late['before'].count(added[0])==1 and late['after']==[v for v in late['before'] if v!=added[0]]
  c['preserve_sources_'+str(index)]=all(r['actor_and_storage_unchanged'] and not r['asset_written'] and r['applied'] for r in delta)
  asset=next((a for a in assets if (a['sequence'],a['item'],a['entity'])==(k[2],k[3],k[1])),None)
  if not asset:c['native_selection_'+str(index)]=False;continue
  subs=[s for s in asset['subsequences'] if s['index']==early['sub']]
  if len(subs)!=1:c['native_selection_'+str(index)]=False;continue
  events=subs[0]['events'];match=True;emitted=[]
  for j,r in enumerate(sel):
   raw=bytes.fromhex(r.get('state_tags',''));tags=[raw[i:i+16] for i in range(0,len(raw),16)];expected=select_reference(events,*r['range'],tags);indices=[]
   for e in r['events']:
    matches=[(i,a) for i,a in enumerate(events) if a['address']==e['payload']]
    if len(matches)!=1:match=False;continue
    i,a=matches[0];indices.append(i);match &= all(a[t]==e[t] for t in ['type_hash','variant_bytes','base_event']) and (e['type_hash']!=IMPACT or a['spawn_impact']==e['spawn_impact'])
   match &= bool(raw) and expected==indices and r['read_only'] and r['actor_unchanged'] and r['return_unchanged'] and r['caller']=='0x1e88ad'
   following=sel[j+1]['us'] if j+1<len(sel) else float('inf');updates=[d for d in delta if r['us']<d['us']<following]
   emitted+=updates[-1]['after'] if updates else [e['variant'] for e in r['events']]
  c['native_selection_'+str(index)]=match and sel[0]['range'][0]==0 and all(a['range'][1]==b['range'][0] for a,b in zip(sel,sel[1:]))
  c['one_final_Impact_per_action_'+str(index)]=len(added)==1 and emitted.count(added[0])==1
  owned=[r for r in ready if early['us']<r['us']<late['us'] and r['sequence']==k[2] and r['entity'] in entities]
  c['early_new_collider_'+str(index)]=len(owned)>=3 and len({r['entity'] for r in owned})==1 and owned[0]['collider_id_before_native']==0
 drained=ev('motion_event_retime_removed');c['exact_event_drain']=len(drained)==1 and drained[0]['appended']==drained[0]['suppressed']==2 and drained[0]['blocked']==0
 hc,hm=health_audit(rows,2);c.update({'health_'+k:v for k,v in hc.items()})
 fc,fm=feedback_audit(rows,xr,mock,2,two_hand=melee_session);c.update({'feedback_'+k:v for k,v in fc.items()})
 # Reuse only independent camera/control invariants; single count checks retain their contract.
 ac,am=single_attack_audit(rows,sequence)
 c.update({k:v for k,v in ac.items() if k.startswith('melee_')})
 c['action_reader_clean']=ac['action_reader_clean']
 return c,dict(speeds=speeds,actions=actions,unqualified_actor_snapshots=sum(r['world']=='0x0' for r in active),unqualified_trigger_times=sorted({r['trigger_triggerTime'] for r in active if r['world']=='0x0'}),collider_entities=entities,health=hm,feedback=fm,live_target_damage_proven=hm['live_target_damage_proven'])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--recovery-swing',action='store_true');a=p.parse_args();launch=a.probe.parent
 read=lambda p:[json.loads(s) for s in p.read_text(encoding='utf-8-sig').splitlines()]
 native=read(a.probe/'view-resource-probe.jsonl');xr=read(launch/'native-xr/openxr.jsonl');mock=read(launch/'mock.jsonl');sequence=[[float(v) for v in l.split(',')] for l in (launch/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')]
 c,m=audit(native,sequence,xr,mock,a.recovery_swing);c['exact_repeat_fixture']=(launch/'mock-hands.csv').read_bytes()==(pathlib.Path('vrmod/test_env')/('two-swing-cooldown-hands.csv' if a.recovery_swing else 'two-swing-hands.csv')).read_bytes();c['normal_exit']=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0;c['runner_PASS']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS';c['Core_Sync_clean']=(launch/'validation.log').stat().st_size==0
 if a.negative_tests:
  for name,event,field,value in [('speed','motion_attack_sample','speed',0),('owner','motion_owned_vm','entity',0),('physics','sequence_collider_sample','physics_current_bytes','00'*32),('source','motion_collider_apply_end','original_components_unchanged',False),('event','motion_event_retime','asset_written',True),('input','controller_fixture_restored','pointers_restored',False),('authority','motion_collider_shadow','owned_intent',False),('head','action_sequence_snapshot','camera_orientation',[0,.5,0,.8660254]),('world','action_sequence_snapshot','world','0x0'),('look','action_sequence_snapshot','world_look',[.2,.4,.6])]:
   bad=copy.deepcopy(native)
   for r in bad:
    if r['event']==event:r[field]=value
   c['synthetic_reject_'+name]=not all(audit(bad,sequence,xr,mock,a.recovery_swing)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(bool(v) for v in c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__);(a.probe/'motion-repeat-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',report['passed'],'FAIL',report['failed'],m);return bool(report['failed'])
if __name__=='__main__':raise SystemExit(main())
