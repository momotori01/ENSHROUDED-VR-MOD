"""Identity-only native collider query borrowing and restoration validation."""
import argparse,copy,json,pathlib
from check_impact_ownership import audit

def check(rows):
 c,e=audit(rows,True,True);ev=lambda n:[r for r in rows if r['event']==n]
 b=ev('sequence_collider_identity_begin');ends=ev('sequence_collider_identity_end');d=ev('sequence_collider_identity_drained');samples=ev('sequence_collider_sample')
 c['identity_scope_count_matches_physics']=len(b)==len(ends)==len(e['matches'])>=3
 c['identity_scopes_only_owned_entity']=bool(b) and all(r['entity'] in e['local_entities'] and r['root']!=r['entity'] and r['item'] for r in b)
 c['identity_original_ecs_unchanged']=bool(ends) and all(r['query_restored'] and r['original_components_unchanged'] and not r['ecs_written'] for r in ends)
 c['identity_scopes_ordered_and_exact_bytes']=bool(b) and len(b)==len(ends) and all(x['us']<y['us'] and x['entity']==y['entity'] and not x['ecs_written'] and any(s['entity']==x['entity'] and x['us']<s['us']<y['us'] and s['transform_bytes']==x['current_bytes'] for s in samples) for x,y in zip(b,ends))
 c['identity_fully_returned_before_unhook']=len(d)==1 and d[0]['outstanding']==0 and d[0]['opened']==len(b)==d[0]['closed'] and d[0]['failures']==0 and bool(ev('input_actor_final_removed')) and d[0]['us']<ev('input_actor_final_removed')[0]['us']
 return c,e

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,e=check(rows)
 if a.negative_tests:
  for name,event,field,value in [('source_modified','sequence_collider_identity_end','original_components_unchanged',False),('unreturned_scope','sequence_collider_identity_drained','outstanding',1),('wrong_source_bytes','sequence_collider_identity_begin','current_bytes','00'*56)]:
   bad=copy.deepcopy(rows);r=next((r for r in bad if r['event']==event),None)
   if r:r[field]=value
   c['reject_'+name]=r is not None and not all(check(bad)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),evidence=e,limitation=__doc__)
 (a.probe/'collider-identity-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed']);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
