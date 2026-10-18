"""Correlate one tracked shot with the native Actor lastSpawnedEntityId write.
This observes the native result only; it does not prove projectile lifetime/collision.
"""
import argparse,json,struct
from pathlib import Path
from check_bow_tracked_aim import audit as aim_audit

def evidence(events):
 ev=lambda n:[x for x in events if x['event']==n]
 observed=ev('bow_spawn_entity_observed');shots=ev('bow_spawn_sample')
 dispatch=ev('bow_spawn_dispatch_result');submits=ev('bow_spawn_submit_result')
 checks=dict(single=len(observed)==len(shots)==len(dispatch)==len(submits)==1,
             parent_chain=True,actor_identity=True,native_id=True,read_only=True)
 details=[]
 for o,x,d,s in zip(observed,shots,dispatch,submits):
  params=bytes.fromhex(d['params_bytes']);context=struct.unpack_from('<Q',params)[0]
  checks['parent_chain'] &= o['sample']==x['sample']==d['sample']==s['sample'] and o['params']==d['params']==s['params'] and int(o['event_context'],16)==context and x['us']<=d['us']<=o['us']<=s['us'] and s['result']
  inputs=[v for v in ev('input_bow_sample') if v['input']==o['input'] and v['weapon']==o['weapon'] and v.get('actor')==o['actor'] and v['world']==o['native_world'] and v['entity']==o['native_entity'] and v['us']<=x['us'] and x['us']-v['us']<=1000000]
  checks['actor_identity'] &= bool(inputs) and o['actor_matched'] and o['actor_stable'] and 0<=o['identity_age_ms']<=100 and int(o['actor'],16)!=0 and o['actor']==o['observed_actor']==x['native_actor'] and o['native_world']==x['native_world'] and o['native_entity']==x['native_entity']==x['local_entity']
  checks['native_id'] &= o['field_offset']==0xd18 and o['native_result'] and o['spawned_entity'] not in [0,o['previous_spawned_entity'],o['native_entity']]
  checks['read_only'] &= o['read_only'] and o['source_unchanged'] and s['read_only'] and s['source_unchanged']
  details.append({k:o[k] for k in ['sample','native_world','native_entity','actor','previous_spawned_entity','spawned_entity','identity_age_ms']})
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,entities=details)

def audit(p):
 events=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text().splitlines()]
 r=evidence(events);tracked=aim_audit(p);r['checks']['tracked_aim']=tracked['status']=='PASS'
 r['status']='PASS' if all(r['checks'].values()) else 'FAIL'
 r['tracked_aim']=tracked
 r['limitation']='Known one-shot Mock: native Actor lastSpawnedEntityId changed in the matched submission call. No ECS lookup, projectile component lifetime, flight/collision, repeat nocking or hardware proof.'
 return r
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args()
 r=audit(v.probe);(v.probe/'bow-spawn-entity-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
 print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
