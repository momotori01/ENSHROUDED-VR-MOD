"""Audit read-only native conclusion and target Health changes.
Empty swings verify hook lifecycle only. Actual damage requires an owned native
VM, matching conclusion, reflected target component and negative observed delta.
Synthetic checker tests are explicitly separate from live damage evidence.
"""
import argparse,copy,json,pathlib,struct
from check_owned_hits import audit as hit_audit
KEY=0x8eb84995
CALLERS={'0x36a295','0x36a5db','0x36a7aa','0x36a92c'}

def audit(rows,expected_impacts=1):
 ev=lambda n:[r for r in rows if r['event']==n]
 start=ev('motion_health_observer_enabled');end=ev('motion_health_observer_removed');drain=ev('motion_hit_observer_removed');commits=ev('motion_owned_hit_commit');done=ev('motion_owned_hit_commit_end');delta=ev('motion_owned_health_delta');vm=ev('motion_owned_vm')
 c={'explicit_readonly_boundaries':len(start)==1 and start[0]['read_only'] and start[0]['health_key']==KEY and start[0]['commit_rva']=='0x3650b0' and start[0]['delta_rva']=='0x364730' and start[0]['lookup_rva']=='0xfe5fe0','hook_restore_and_drain':len(drain)==1 and drain[0]['restored'] and drain[0]['callbacks']==drain[0]['failures']==0,'no_observer_failure':not any('failure' in r['event'] for r in rows),'one_health_drain':len(end)==1 and end[0]['commits']==len(commits) and end[0]['deltas']==len(delta) and end[0]['unqualified']==0,'owned_VM_available':len(vm)>=3,'unique_commit_ids':len({r['commit'] for r in commits})==len(commits),'commit_end_exact':sorted(r['commit'] for r in done)==sorted(r['commit'] for r in commits) and all(r['read_only'] and r['candidate_unchanged'] for r in done)}
 c.update({'hit_scope_'+k:v for k,v in hit_audit(rows,expected_impacts)[0].items()})
 if start and 'witness_capacity' in start[0]:
  cleared=ev('motion_health_witness_removed')
  c['witness_registry_drained']=start[0]['witness_capacity']==16 and start[0]['witness_max_age_ms']==500 and len(cleared)==1 and cleared[0]['cleared'] and cleared[0]['records']==len(vm)

 schema_required=bool(start and start[0].get('health_schema_required'))
 def valid_schema(r):
  try:
   meta=bytes.fromhex(r.get('attribute_metadata',''));h=bytes.fromhex(r.get('header_bytes',''));t=bytes.fromhex(r.get('component_type_bytes',''))
   return bool(r.get('schema_validated')) and len(meta)==32 and len(h)==20 and len(t)==64 and r.get('attribute_name')=='Health' and r.get('component_name')=='keen::ecs::Health' and r.get('component_bytes')==68 and struct.unpack_from('<H',t,56)[0]==68 and struct.unpack_from('<Q',t,24)[0]==17 and struct.unpack_from('<II',meta)==(KEY,KEY) and struct.unpack_from('<Q',meta,24)[0]==6 and struct.unpack_from('<H',meta,12)[0]==r['index'] and struct.unpack_from('<I',h)[0]==KEY and struct.unpack_from('<II',h,8)==(36,r['count']) and h[16]&1
  except (ValueError,KeyError,struct.error):return False
 if schema_required:c['runtime_Health_schema_for_resolved_values']=all(valid_schema(r) for r in delta if r['resolved'])
 owners=['world','entity','root','item','event_bytes','impact_bytes']
 def owner_match(a,b):return all(a.get(k)==b.get(k) for k in owners)
 def valid_commit(r):
  h=bytes.fromhex(r['candidate_bytes'])
  return len(h)==320 and r['read_only'] and r['world_matches'] and r['world']!='0x0' and r['caller'] in CALLERS and 0<=r['owner_age_ms']<=500 and r['target']>0 and struct.unpack_from('<I',h,0x90)[0]==r['root'] and struct.unpack_from('<I',h,0x94)[0]==r['target'] and struct.unpack_from('<I',h,0xd4)[0]==r['instance_id'] and any(owner_match(r,v) and r['instance_id']==v['instance_id'] and v['us']<=r['us'] for v in vm)
 c['commit_matches_native_local_Impact']=all(valid_commit(r) for r in commits)
 byid={r['commit']:r for r in commits}
 def valid_delta(r):
  q=byid.get(r['commit']);resolved=r['resolved']
  joined=q and owner_match(q,r) and q['target']==r['target'] and q['instance_id']==r['instance_id'] and q['us']<=r['us'] and any(e['commit']==r['commit'] and r['us']<=e['us'] for e in done)
  if not(joined and r['read_only'] and r['caller']=='0x365962' and r['key']==KEY):return False
  if not resolved:return not r['native_delta_exact']
  if schema_required and not valid_schema(r):return False
  return r['header_stable'] and 0<=r['component_id']<640 and 0<=r['index']<r['count']<=8 and int(r['container'],16)>0 and int(r['slot'],16)==int(r['container'],16)+36+4*r['index'] and r['native_delta_exact']==((r['after']&0xffffffff)==((r['before']+r['requested_delta'])&0xffffffff))
 c['delta_joined_and_bounded']=all(valid_delta(r) for r in delta)
 # A wrapped integer or unresolved slot cannot constitute physical damage proof.
 proven=[r for r in delta if valid_delta(r) and r['resolved'] and r['native_delta_exact'] and r['requested_delta']<0 and r['before']>0 and r['after']<r['before'] and r['after']-r['before']==r['requested_delta'] and r['target']!=r['root']]
 return c,dict(native_commits=len(commits),health_delta_calls=len(delta),verified_target_decreases=len(proven),live_target_damage_proven=all(c.values()) and bool(proven))

def synthetic_tests():
 owner=dict(world='0x10000',entity=9,root=3,item=5,event_bytes='ab'*24,impact_bytes='00'*16+'07000000')
 candidate=bytearray(320)
 for at,v in [(0x90,3),(0x94,11),(0xd4,7)]:struct.pack_into('<I',candidate,at,v)
 rows=[dict(event='motion_health_observer_enabled',read_only=True,health_key=KEY,commit_rva='0x3650b0',delta_rva='0x364730',lookup_rva='0xfe5fe0')]
 rows +=[dict(event='motion_hit_observer_enabled',read_only=True,hit_bytes=232,caller='0x36b616'),dict(event='impact_hierarchy_sample',sequence=2,entity=9,root_entity=3,sequence_event_bytes=owner['event_bytes'],impact_bytes=owner['impact_bytes'],latest_actor_item=5)]
 rows +=[dict(event='motion_owned_vm',**owner,instance_id=7,us=i,read_only=True,owner_stable=True,caller='0x36b616',queue_before='00'*48,queue_after='00'*48,count_before=0,count_after=0) for i in [1,2,3]]
 rows +=[dict(event='motion_owned_hit_commit',**owner,commit=1,target=11,instance_id=7,caller='0x36a295',owner_age_ms=1,world_matches=True,candidate_bytes=candidate.hex(),read_only=True,us=4),dict(event='motion_owned_health_delta',**owner,commit=1,target=11,instance_id=7,component_id=22,container='0x1000',slot='0x102c',key=KEY,index=2,count=8,before=100,after=83,requested_delta=-17,resolved=True,header_stable=True,native_delta_exact=True,caller='0x365962',read_only=True,us=5),dict(event='motion_owned_hit_commit_end',commit=1,read_only=True,candidate_unchanged=True,us=6),dict(event='motion_health_observer_removed',commits=1,deltas=1,unqualified=0),dict(event='motion_hit_observer_removed',restored=True,callbacks=0,failures=0,owned_vm_calls=3,hit_iterations=0,damage_callbacks=0)]
 checks={};c,m=audit(rows);checks['synthetic_positive_join']=all(c.values()) and m['live_target_damage_proven']
 for name,event,key,value in [('World','motion_owned_hit_commit','world','0x20000'),('root','motion_owned_hit_commit','root',4),('instance','motion_owned_hit_commit','instance_id',8),('expired','motion_owned_hit_commit','owner_age_ms',501),('target','motion_owned_health_delta','target',12),('key','motion_owned_health_delta','key',1),('index','motion_owned_health_delta','index',8),('slot','motion_owned_health_delta','slot','0x1030'),('restore','motion_hit_observer_removed','restored',False),('time','motion_owned_health_delta','us',7),('unknown_world','motion_health_observer_removed','unqualified',1),('claimed_arithmetic','motion_owned_health_delta','after',82)]:
  bad=copy.deepcopy(rows);next(r for r in bad if r['event']==event)[key]=value;c,m=audit(bad);checks['synthetic_reject_'+name]=not all(c.values()) and not m['live_target_damage_proven']
 for name,changes in [('unresolved',dict(resolved=False,native_delta_exact=False)),('zero',dict(requested_delta=0,after=100)),('healing',dict(requested_delta=17,after=117))]:
  bad=copy.deepcopy(rows);next(r for r in bad if r['event']=='motion_owned_health_delta').update(changes);c,m=audit(bad);checks['synthetic_not_damage_'+name]=all(c.values()) and not m['live_target_damage_proven']
 strong=copy.deepcopy(rows);strong[0]['health_schema_required']=True
 meta=bytearray(32);struct.pack_into('<II',meta,0,KEY,KEY);struct.pack_into('<H',meta,12,2);struct.pack_into('<Q',meta,24,6)
 h=bytearray(20);struct.pack_into('<IIII',h,0,KEY,0,36,8);h[16]=1;t=bytearray(64);struct.pack_into('<Q',t,24,17);struct.pack_into('<H',t,56,68)
 next(r for r in strong if r['event']=='motion_owned_health_delta').update(schema_validated=True,attribute_metadata=meta.hex(),header_bytes=h.hex(),component_type_bytes=t.hex(),attribute_name='Health',component_name='keen::ecs::Health',component_bytes=68)
 c,m=audit(strong);checks['synthetic_schema_positive']=all(c.values()) and m['live_target_damage_proven']
 for key,value in [('schema_validated',False),('attribute_name','Health_Max'),('component_name','keen::ecs::Stamina'),('component_bytes',20),('attribute_metadata','00'*32),('header_bytes','00'*20),('component_type_bytes','00'*64)]:
  bad=copy.deepcopy(strong);next(r for r in bad if r['event']=='motion_owned_health_delta')[key]=value;c,m=audit(bad);checks['synthetic_schema_reject_'+key]=not all(c.values()) and not m['live_target_damage_proven']
 return checks

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,m=audit(rows)
 if a.negative_tests:c.update(synthetic_tests())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(bool(v) for v in c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'owned-health-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
