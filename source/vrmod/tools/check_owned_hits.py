"""Audit local Impact VM scope ownership. Zero hits is a valid empty-swing result,
not damage success. Positive hit payloads require an actual target fixture.
"""
import argparse,copy,json,pathlib,struct

def audit(rows,expected_impacts=1):
 if expected_impacts not in (1,2):raise ValueError("bounded expected Impact count")
 ev=lambda n:[r for r in rows if r['event']==n]
 begin=ev('motion_hit_observer_enabled');end=ev('motion_hit_observer_removed');vm=ev('motion_owned_vm');hits=ev('motion_owned_hit_iteration');damage=ev('motion_owned_damage_callback');owned=[r for r in ev('impact_hierarchy_sample') if r['sequence']]
 c={'explicit_readonly_abi':len(begin)==1 and begin[0]['read_only'] and begin[0]['hit_bytes']==232 and begin[0]['caller']=='0x36b616','local_vm_observed':len(vm)>=3,'scopes_stable_and_readonly':bool(vm) and all(r['owner_stable'] and r['read_only'] and r['caller']=='0x36b616' and r['world']!='0x0' for r in vm),'single_native_event':bool(vm) and len({(r['world'],r['entity'],r['event_bytes']) for r in vm})==expected_impacts,'owned_vm_matches_ECS':bool(vm) and all(any(h['entity']==r['entity'] and h['root_entity']==r['root'] and h['sequence_event_bytes']==r['event_bytes'] and h['impact_bytes']==r['impact_bytes'] and h['latest_actor_item']==r['item'] for h in owned) for r in vm),'instance_id_matches_component':bool(vm) and all(r['instance_id']==struct.unpack_from('<I',bytes.fromhex(r['impact_bytes']),16)[0] for r in vm),'queue_count_layout_exact':bool(vm) and all(len(bytes.fromhex(r['queue_'+side]))==48 and r['count_'+side]==struct.unpack_from('<I',bytes.fromhex(r['queue_'+side]),40)[0] and r['count_'+side]<=256 for r in vm for side in ['before','after']),'observer_restored_and_drained':len(end)==1 and end[0]['restored'] and end[0]['callbacks']==end[0]['failures']==0 and end[0]['owned_vm_calls']==len(vm) and end[0]['hit_iterations']==len(hits) and end[0]['damage_callbacks']==len(damage),'no_observation_failure':not any('failure' in r['event'] for r in rows)}
 c['hit_iterator_matches_native_semantics']=all(r['scope_matches'] and r['read_only'] and r['return_matches_output'] and ((r['index_before']<r['queue_count'] and r['index_after']==r['index_before']+1 and r['result_word']==256 and len(bytes.fromhex(r['hit_bytes']))==232) or(r['index_before']>=r['queue_count'] and r['index_after']==r['index_before'] and r['result_word']==0 and not r['hit_bytes'])) for r in hits)
 c['damage_callback_scope_verified']=all(r['scope_matches'] and r['read_only'] and r['return_matches_output'] and r['damage_amount_unverified'] for r in damage)
 m=dict(owned_vm_calls=len(vm),hit_iterations=len(hits),damage_callbacks=len(damage),max_queued_hits=max([r['count_before'] for r in vm]+[r['count_after'] for r in vm],default=0),live_target_damage_proven=False)
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,m=audit(rows)
 if a.negative_tests:
  for name,event,key,value in [('wrong_owner','motion_owned_vm','entity',0),('wrong_scope','motion_owned_vm','owner_stable',False),('wrong_instance','motion_owned_vm','instance_id',0),('wrong_count','motion_owned_vm','count_before',999),('no_restore','motion_hit_observer_removed','restored',False)]:
   bad=copy.deepcopy(rows);r=next((r for r in bad if r['event']==event),None)
   if r:r[key]=value
   c['reject_'+name]=r is not None and not all(audit(bad)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'owned-hit-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
