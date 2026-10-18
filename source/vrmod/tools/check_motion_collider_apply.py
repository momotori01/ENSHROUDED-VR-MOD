"""One-swing native physics transform substitution proof; not a target damage test."""
import argparse,copy,json,pathlib,struct
from check_motion_collider_shadow import audit as shadow_audit
from check_impact_ownership import audit as ownership_audit

def physics_transform(raw):
 b=bytes.fromhex(raw);f32=lambda x:struct.unpack('<f',struct.pack('<f',x))[0]
 return struct.pack('<8f',*[f32(f32(v)*2**-32) for v in struct.unpack_from('<3q',b)],struct.unpack_from('<f',b,40)[0],*struct.unpack_from('<4f',b,24))

def audit(rows):
 c,m=shadow_audit(rows)
 c.pop('query_and_ecs_not_written');c.pop('native_transform_stays_original')
 owner,owner_details=ownership_audit(rows,True,True);c.update({'ownership_'+k:v for k,v in owner.items()})
 ev=lambda n:[r for r in rows if r['event']==n]
 starts=ev('motion_collider_apply_begin');ends=ev('motion_collider_apply_end');native={r.get('ready_sample'):r for r in ev('sequence_collider_sample')};proposals={r['ready_sample']:r for r in ev('motion_collider_shadow')};finish={r['ready_sample']:r for r in ends}
 scopes=bytes_ok=physics_ok=initial=True
 ids=[]
 for r in starts:
  key=r['ready_sample'];s=native.get(key,{});p=proposals.get(key,{});end=finish.get(key,{})
  scopes &= r['us']<s.get('us',0)<end.get('us',0) and s.get('entity')==r['entity'] and p.get('eligible',False) and p.get('frame')==r['frame']
  bytes_ok &= r['applied_current']==p.get('proposed_transform')==s.get('transform_bytes') and r['original_current']==p.get('original_transform') and r['applied_current'][80:]==r['original_current'][80:]
  physics_ok &= physics_transform(r['applied_current'])==bytes.fromhex(s.get('physics_current_bytes',''))
  initial &= r['initial_sweep_reset']==(r['collider_before']==0)
  if r['collider_before']==0:
   if r['previous_present']:
    initial &= r['applied_previous'][:104]==r['applied_current'][:104] and r['applied_previous'][104:]==r['original_previous'][104:]
   initial &= bytes.fromhex(s.get('physics_previous_bytes',''))==physics_transform(r['applied_current'])
  else:initial &= r['applied_previous']==r['original_previous']
  ids.append((r['collider_before'],end.get('collider_after')))
 c['explicit_one_swing_apply_mode']=len(ev('motion_collider_apply_enabled'))==1 and ev('motion_collider_apply_enabled')[0]['native_hit_window_retained']==(not bool(ev('motion_event_retime')))
 c['all_eligible_samples_applied']=len(starts)==len(ends)==len(proposals)>=3
 c['scopes_surround_native_consumer']=bool(starts) and scopes
 c['exact_rigid_display_pose_native_scale']=bool(starts) and bytes_ok
 c['native_physics_input_bit_exact']=bool(starts) and physics_ok
 c['initial_sweep_current_equals_previous']=bool(starts) and initial
 c['one_local_collider_lifetime']=bool(ids) and len({r['entity'] for r in starts})==1 and ids[0][0]==0 and all(a[1]==b[0] and a[1] for a,b in zip(ids,ids[1:]))
 c['original_ecs_unchanged_query_restored']=bool(ends) and all(r['query_restored'] and r['original_components_unchanged'] and not r['ecs_written_by_mod'] for r in ends)
 d=ev('motion_collider_apply_drained');unhook=ev('input_actor_final_removed')
 c['drained_before_unhook']=len(d)==1 and len(unhook)==1 and d[0]['us']<unhook[0]['us'] and unhook[0]['restored'] and d[0]['outstanding']==d[0]['failures']==d[0]['fallbacks']==0 and d[0]['opened']==d[0]['closed']==len(starts)
 c['no_rejected_fallback']=not ev('motion_collider_apply_fallback') and m['rejected']==0
 m['native_scopes']=len(starts);m['native_collider_ids']=ids
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,m=audit(rows)
 if a.negative_tests:
  for name,event,key,val in [('wrong_applied_pose','motion_collider_apply_begin','applied_current','00'*56),('physics_mismatch','sequence_collider_sample','physics_current_bytes','00'*32),('phantom_initial_sweep','sequence_collider_sample','physics_previous_bytes','00'*32),('source_written','motion_collider_apply_end','original_components_unchanged',False),('not_restored','motion_collider_apply_end','query_restored',False),('undrained','motion_collider_apply_drained','outstanding',1),('bad_frame','motion_collider_apply_begin','frame',0)]:
   bad=copy.deepcopy(rows);first=next((x['ready_sample'] for x in bad if x['event']=='motion_collider_apply_begin'),None)
   r=next((r for r in bad if r['event']==event and (event!='sequence_collider_sample' or r.get('ready_sample')==first)),None)
   if r:r[key]=val
   c['reject_'+name]=r is not None and not all(audit(bad)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'motion-collider-apply-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
