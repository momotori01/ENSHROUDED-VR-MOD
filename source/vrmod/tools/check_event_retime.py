"""Check one early native Impact, later suppression and native action preservation.
This is a native X-input diagnostic, not proof of physical-hand timing or damage.
"""
import argparse,copy,json,pathlib
from check_event_selection import audit as selection_audit
from check_native_attack import check as attack_audit

def audit(rows,sequence=None):
 c,m=selection_audit(rows,True,True);c.pop('impact_before_native_collider',None)
 ev=lambda n:[r for r in rows if r['event']==n]
 changes=ev('motion_event_retime');done=ev('motion_event_retime_removed');s=ev('motion_event_selection_shadow');samples=ev('motion_event_selection');ready=ev('sequence_collider_ready_sample');enable=ev('motion_event_selection_enabled')
 c['explicit_one_impact_override']=len(enable)==1 and enable[0]['one_impact_retime'] and not enable[0]['read_only']
 c['exact_append_then_suppress']=[r['operation'] for r in changes]==['append','suppress']
 c['owned_action_identity_preserved']=bool(changes) and len({tuple(r[k] for k in ['world','entity','sequence','item','trigger','call','sub']) for r in changes})==1
 c['source_actor_asset_storage_preserved']=bool(changes) and all(r['actor_and_storage_unchanged'] and not r['asset_written'] and r['applied'] for r in changes)
 q=ev('motion_event_retime_qualification');tickets=ev('native_attack_ticket');c['fresh_matching_issued_ticket']=len(q)==len(tickets)==1 and q[0]['qualified'] and q[0]['ticket_matches'] and 0<=q[0]['ticket_age_ms']<=1000 and q[0]['ticket_serial']==tickets[0]['serial'] and q[0]['native_trigger']!=q[0]['previous_trigger']==tickets[0]['previous_trigger']
 c['counts_drained_once']=len(done)==1 and done[0]['appended']==done[0]['suppressed']==1
 if len(changes)==2 and len(s)==1 and len(s[0]['events'])==1:
  early,late=changes;v=s[0]['events'][0]['variant']
  c['append_preserves_every_original_event']=early['after']==early['before']+[v] and v not in early['before']
  c['suppression_preserves_every_other_event']=late['before'].count(v)==1 and late['after']==[p for p in late['before'] if p!=v]
  c['same_candidate_and_owner_world']=early['world']==s[0]['world'] and early['entity']==s[0]['entity']
  guid=bytes.fromhex(s[0]['events'][0]['spawn_impact'])[0x158:0x168];owned=[r for r in ready if bytes.fromhex(r['event_bytes'])[8:24]==guid and r['sequence']==early['sequence']]
  c['native_creation_before_authored_window']=bool(owned) and early['us']<owned[0]['us']<late['us'] and owned[0]['collider_id_before_native']==0
  c['one_collider_no_late_recreation']=len(owned)>2 and len({r['entity'] for r in owned})==1 and all(early['us']<r['us']<late['us'] for r in owned) and all(r['collider_id_before_native']!=0 for r in owned[1:])
  c['original_output_matches_observation']=True
  for change in changes:
   previous=[r for r in samples if r['us']<change['us']][-1]
   c['original_output_matches_observation'] &= change['before']==[e['variant'] for e in previous['events']]
  emitted=[]
  for i,r in enumerate(samples):
   next_us=samples[i+1]['us'] if i+1<len(samples) else float('inf')
   delta=[d for d in changes if r['us']<d['us']<next_us]
   emitted += delta[-1]['after'] if delta else [e['variant'] for e in r['events']]
  c['one_impact_in_final_native_output']=emitted.count(v)==1 and len(emitted)==sum(len(r['events']) for r in samples)
  m.update(early_us=early['us'],authored_us=late['us'],first_collider_us=owned[0]['us'] if owned else None,owned_collider_updates=len(owned),advanced_wall_ms=(late['us']-early['us'])/1000)
 else:c['native_creation_before_authored_window']=c['one_collider_no_late_recreation']=False

 if ev('motion_attack_sample'):
  if sequence is None:raise ValueError('motion timing audit requires the actual hand fixture')
  from check_motion_attack import check
  ac,am=check(rows,sequence)
  c['motion_ticket_origin_verified']=bool(len(q)==len(tickets)==1 and q[0]['motion'] and q[0]['origin_valid'] and tickets[0]['motion'] and tickets[0]['request'] and tickets[0]['session'] and tickets[0]['origin'])
 else:ac,am=attack_audit(rows)
 c['native_attack_acceptance_recovery_preserved']=all(ac.values());m['native_action']=am
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];sequence=None
 if any(r['event']=='motion_attack_sample' for r in rows):sequence=[[float(v) for v in line.split(',')] for line in (a.probe.parent/'mock-hands.csv').read_text().splitlines() if line and not line.startswith('#')]
 c,m=audit(rows,sequence)
 if a.negative_tests:
  for name,event,key,value in [('wrong_world','motion_event_retime','world','0x0'),('source_written','motion_event_retime','actor_and_storage_unchanged',False),('asset_written','motion_event_retime','asset_written',True),('double_emit','motion_event_retime_removed','appended',2),('no_suppress','motion_event_retime_removed','suppressed',0),('not_applied','motion_event_retime','applied',False)]:
   bad=copy.deepcopy(rows);r=next((r for r in bad if r['event']==event),None)
   if r:r[key]=value
   c['reject_'+name]=r is not None and not all(audit(bad,sequence)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'event-retime-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
