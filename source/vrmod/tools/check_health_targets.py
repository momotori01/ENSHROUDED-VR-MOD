"""Independent audit of read-only target discovery; not contact/damage evidence.
Native bounded scan and external inventory are sequential snapshots. Their
moving positions are not required to match; each is recomputed from its raw Q32.
Faction is a native label, not an enemy/attackability or line-of-sight decision.
"""
import argparse,copy,json,math,pathlib,struct

def audit(rows, external, attributes=None):
 ev=lambda n:[r for r in rows if r['event']==n]
 starts=ev('motion_target_enabled');ends=ev('motion_target_removed');regs=ev('motion_target_registry');scans=ev('motion_target_scan_complete');candidates=ev('motion_target_candidate')
 c={}
 c['one_readonly_probe']=len(starts)==len(ends)==len(regs)==len(scans)==1 and starts[0]['read_only'] and starts[0]['no_input_fixture']
 if not c['one_readonly_probe']:return c
 reg=regs[0];world=reg['world'];done=ends[0];schema_required=bool(starts[0].get('health_schema_required'))
 c['restored_and_drained']=done['restored'] and done['callbacks']==done['failures']==0 and done['samples']==len(candidates)
 c['no_failure_or_input_fixture']=not any('failure' in r['event'] or r['event'].startswith('controller_fixture') or r['event']=='motion_target_scan_invalidated' for r in rows)
 c['native_complete']=ev('input_observation_complete')==[next((r for r in rows if r['event']=='input_observation_complete' and r['result']==0),None)]
 c['whole_map_scanned']=scans[0]['buckets']==reg['capacity'] and scans[0]['read_only'] and scans[0]['world']==world
 c['bounded_nearest_output']=scans[0]['nearest_limit']==32 and scans[0]['candidates']==len(candidates) and 0<len(candidates)<=32 and len({r['entity'] for r in candidates})==len(candidates)
 c['native_distance_order']=all(a['distance_m']<=b['distance_m'] for a,b in zip(candidates,candidates[1:]))
 def valid_geometry(r,root):
  b=bytes.fromhex(r['transform_bytes'])
  if len(b)!=56:return False
  q=struct.unpack_from('<3q',b);d=[(q[i]-root[i])/2**32 for i in range(3)]
  h=bytes.fromhex(r['health_bytes'])
  if len(h)!=68:return False
  offset,count=struct.unpack_from('<II',h,8)
  return all(math.isfinite(v) for v in d) and all(abs(a-b)<2e-5 for a,b in zip(d,r['relative_m'])) and abs(math.dist(d,[0,0,0])-r['distance_m'])<2e-5 and 0<=r['distance_m']<=250 and offset==36 and 0<count<=8 and bool(h[16]&1) and r['stable_read']
 c['native_raw_geometry_and_health']=all(valid_geometry(r,r['root_q32']) and r['world']==world and r['local_entity']==reg['local_entity'] and r['health_type']==reg['health_type'] and r['current_type']==reg['current_type'] and r['read_only'] and r['health_index_unresolved']==(not schema_required) and r['hostility_unclassified'] for r in candidates)
 if schema_required:
  schema=ev('motion_target_health_schema');valid=len(schema)==1 and attributes is not None
  c['runtime_Health_schema_present']=valid
  if valid:
   s=schema[0];b=bytes.fromhex(s['attribute_metadata']);t=bytes.fromhex(s['component_type_bytes']);index=struct.unpack_from('<H',b,12)[0] if len(b)==32 else -1
   c['native_attribute_matches_external']=s['world']==world and s['registry']==attributes['registry'] and s['attribute_name']==attributes['selected']['name']=='Health' and s['attribute_metadata']==attributes['selected']['bytes'] and attributes['table_stable'] and attributes['read_only'] and attributes['health_key']==0x8eb84995 and len(b)==32 and struct.unpack_from('<II',b)==(0x8eb84995,0x8eb84995)
   c['native_component_matches_external']=s['component_id']==reg['health_type'] and s['component_name']=='keen::ecs::Health' and s['component_bytes']==68 and len(t)==64 and struct.unpack_from('<H',t,56)[0]==68 and struct.unpack_from('<Q',t,24)[0]==17 and s['read_only']
   c['native_HP_values_from_raw']=0<=index<8 and all(r['health_schema_verified'] and r['health_index']==index and r['health_value']==struct.unpack_from('<i',bytes.fromhex(r['health_bytes']),36+4*index)[0] for r in candidates)
 c['native_observation_times']=all(0<reg.get('started_ms',0)<=r['observed_ms']<=scans[0].get('finished_ms',0) for r in candidates)
 c['external_same_world_and_types']=external['world']==world and external['local_entity']==reg['local_entity'] and external['health_type']==reg['health_type'] and external['current_type']==reg['current_type'] and external['capacity']==reg['capacity']
 c['external_stable_storage']=external['read_only'] and external['root_position_unchanged'] and external['map_storage_unchanged'] and external['hostility_unclassified']
 allrows=external['all_candidates'];types=external['component_registry']['records']
 c['external_count_and_order']=external['candidate_count']==len(allrows) and len({r['entity'] for r in allrows})==len(allrows) and all(a['distance_m']<=b['distance_m'] for a,b in zip(allrows,allrows[1:]))
 c['external_raw_geometry_and_health']=all(valid_geometry(r,external['root_q32']) for r in allrows)
 c['registry_capacity_and_identity']=external['component_registry']['stable_read'] and external['component_registry']['count']==len(types)<=external['component_registry']['capacity']==640 and all(r['id']==i for i,r in enumerate(types)) and len({r['qualified_name'] for r in types})==len(types)
 c['query_names_from_live_registry']=all(types[idx]['qualified_name']=='keen::ecs::'+name and types[idx]['size']==size for idx,name,size in [(reg['health_type'],'Health',68),(reg['current_type'],'CurrentTransform',56),(external['actor_type'],'Actor',3600),(external['faction_type'],'Faction',1)])
 c['all_observed_runtime_strides_match']=all(0<=i<len(types) and types[i]['size']==size for r in allrows for i,size in r['components'])
 c['local_actor_present']=any(r['entity']==external['local_entity'] and r['has_actor'] and r['faction_name']=='Player' and r['distance_m']<1e-7 for r in allrows)
 c['actor_filter']=external['nearest_other_actors']==[r for r in allrows if r['has_actor'] and r['entity']!=external['local_entity']][:16]
 c['external_nearest']=external['nearest']==allrows[:32]
 c['faction_values_bounded']=all((r['faction_value'] is None and r['faction_name'] is None) or (type(r['faction_value'])==int and 0<=r['faction_value']<16 and bool(r['faction_name'])) for r in allrows)
 return c

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];external=json.loads((a.probe/'independent-health-targets.json').read_text());attributes=json.loads((a.probe/'attribute-registry.json').read_text(encoding='utf-8-sig')) if (a.probe/'attribute-registry.json').exists() else None;checks=audit(rows,external,attributes)
 if a.negative_tests:
  cases=[('World','motion_target_registry','world','0x0'),('restore','motion_target_removed','restored',False),('distance','motion_target_candidate','distance_m',249.0),('count','motion_target_scan_complete','candidates',0),('read_only','motion_target_candidate','read_only',False),('time','motion_target_candidate','observed_ms',0)]
  if any(r['event']=='motion_target_health_schema' for r in rows):cases += [('Health_name','motion_target_health_schema','attribute_name','Stamina'),('Health_metadata','motion_target_health_schema','attribute_metadata','00'*32),('Health_value','motion_target_candidate','health_value',-123456),('Health_index','motion_target_candidate','health_index',7)]
  for name,event,key,value in cases:
   bad=copy.deepcopy(rows);next(r for r in bad if r['event']==event)[key]=value;checks['synthetic_reject_'+name]=not all(audit(bad,external,attributes).values())
  for name in ['registry','map','faction','component_bound']:
   bad=copy.deepcopy(external)
   if name=='registry':bad['component_registry']['records'][bad['health_type']]['qualified_name']='keen::ecs::Mana'
   if name=='map':bad['map_storage_unchanged']=False
   if name=='faction':bad['all_candidates'][0]['faction_value']=255
   if name=='component_bound':next(r for r in bad['all_candidates'] if r['has_actor'])['components'].append([640,1])
   checks['synthetic_reject_'+name]=not all(audit(rows,bad,attributes).values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},passed=sum(bool(v) for v in checks.values()),failed=sum(not v for v in checks.values()),live_target_damage_proven=False,limitation=__doc__)
 (a.probe/'health-target-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed']);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
