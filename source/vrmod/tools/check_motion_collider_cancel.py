"""Validate bounded cancellation after tracked Collider adoption, through native removal."""
import argparse,copy,json,pathlib
from check_motion_collider_shadow import audit as shadow_audit
from check_motion_collider_apply import physics_transform

def audit(rows,count):
 c,m=shadow_audit(rows);c.pop('native_transform_stays_original');c.pop('query_and_ecs_not_written')
 ev=lambda n:[r for r in rows if r['event']==n]
 begin=ev('motion_collider_apply_begin');end=ev('motion_collider_apply_end');cancel=ev('motion_collider_cancel_begin');cancel_end=ev('motion_collider_cancel_end');over=ev('motion_collider_lookup_suppressed');native={r.get('ready_sample'):r for r in ev('sequence_collider_sample') if r.get('ready_sample')};proposal={r['ready_sample']:r for r in ev('motion_collider_shadow')}
 c['explicit_cancel_fixture']=len(ev('motion_collider_lookup_enabled'))==1 and ev('motion_collider_lookup_enabled')[0]['cancel_after']==count
 c['exact_initial_tracked_updates']=len(begin)==len(end)==count and bool(begin)
 c['tracked_physics_input_exact']=bool(begin) and all(native[r['ready_sample']]['physics_updated'] and bytes.fromhex(native[r['ready_sample']]['physics_current_bytes'])==physics_transform(r['applied_current']) and native[r['ready_sample']]['transform_bytes']==r['applied_current']==proposal[r['ready_sample']]['proposed_transform'] for r in begin)
 c['native_cancel_scopes_observed']=len(cancel)==len(cancel_end)==len(over)>=3 and len(cancel)+len(begin)==len(proposal)
 c['one_owned_world_entity']=bool(cancel) and len({r['entity'] for r in begin+cancel+over})==1 and all(r['world']==proposal[r['ready_sample']]['impact_world'] for r in cancel+over)
 c['native_lookup_only_at_owned_caller']=bool(over) and all(r['caller']=='0x36bb02' and r['event_matches'] and r['original_resource_present'] and r['returned_null'] for r in over)
 c['first_cancel_removes_live_collider']=bool(cancel) and cancel[0]['collider_before']!=0 and bool(end) and end[-1]['collider_after']==cancel[0]['collider_before']
 c['cancel_remains_latched_with_valid_tracking']=bool(cancel) and all(r['forced_fixture'] and r['pose_eligible'] and r['latched'] for r in cancel) and all(r['collider_before']==0 for r in cancel[1:])
 c['native_handle_cleared_no_physics_update']=bool(cancel_end) and all(r['collider_after']==0 and r['lookup_suppressed'] and native[r['ready_sample']]['collider_id']==0 and not native[r['ready_sample']]['physics_updated'] for r in cancel_end)
 c['cancel_keeps_original_transform']=bool(cancel) and all(native[r['ready_sample']]['transform_bytes']==proposal[r['ready_sample']]['original_transform'] for r in cancel)
 c['scoped_override_order']=bool(cancel) and len(cancel)==len(over)==len(cancel_end) and all(a['ready_sample']==b['ready_sample']==d['ready_sample'] and a['us']<b['us']<native[a['ready_sample']]['us']<d['us'] for a,b,d in zip(cancel,over,cancel_end))
 c['all_original_components_unchanged']=bool(end+cancel_end) and all(r['query_restored'] and r['original_components_unchanged'] and not r['ecs_written_by_mod'] for r in end+cancel_end)
 d=ev('motion_collider_apply_drained');rem=ev('motion_collider_lookup_removed');iterator=ev('input_actor_final_removed')
 c['scopes_and_lookup_fully_drained']=len(d)==len(rem)==len(iterator)==1 and d[0]['opened']==d[0]['closed']==len(begin+cancel) and d[0]['applied']==count and d[0]['cancelled']==len(cancel) and d[0]['outstanding']==d[0]['failures']==0 and rem[0]['restored'] and rem[0]['callbacks']==0 and d[0]['us']<rem[0]['us']<iterator[0]['us'] and iterator[0]['restored']
 m.update(applied=len(begin),cancelled=len(cancel),cancel_after=count)
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--after',type=int,default=5);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,m=audit(rows,a.after)
 if a.negative_tests:
  for name,event,key,value in [('not_removed','motion_collider_cancel_end','collider_after',999),('wrong_world','motion_collider_cancel_begin','world','0x0'),('foreign_lookup','motion_collider_lookup_suppressed','event_matches',False),('wrong_caller','motion_collider_lookup_suppressed','caller','0x0'),('restarted','motion_collider_cancel_begin','latched',False),('source_written','motion_collider_cancel_end','original_components_unchanged',False),('hook_not_restored','motion_collider_lookup_removed','restored',False)]:
   bad=copy.deepcopy(rows);r=next((r for r in bad if r['event']==event),None)
   if r:r[key]=value
   c['reject_'+name]=r is not None and not all(audit(bad,a.after)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'motion-collider-cancel-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
