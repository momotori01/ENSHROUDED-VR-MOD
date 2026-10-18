"""Independent byte decoder for native sequence_event_collider observations.
PASS establishes observation and local-parent sequence correlation only.
"""
import argparse,collections,copy,json,pathlib,struct,math

def audit(rows):
 r=[x for x in rows if x.get('event')=='sequence_collider_sample'];removed=[x for x in rows if x.get('event')=='sequence_collider_removed'];enabled=[x for x in rows if x.get('event')=='sequence_collider_enabled'];checks=[]
 def check(k,v):checks.append(dict(name=k,status='PASS' if v else 'FAIL'))
 check('verified_shared_iterator_site',len(enabled)==1 and enabled[0].get('caller_rva')=='0x36be7e' and enabled[0].get('query_bytes')==80)
 check('bounded_observations',0<len(r)<=1043)
 check('no_read_failures',not any(x.get('event')=='sequence_collider_failure' for x in rows))
 check('stable_read_only',bool(r) and all(x.get('stable_read') and x.get('read_only') for x in r))
 widths={'event_bytes':24,'collider_bytes':24,'transform_bytes':56,'last_transform_bytes':72,'despawn_link_bytes':8}
 width_ok=all(all(len(x.get(k,''))==2*v for k,v in widths.items()) for x in r)
 check('reflected_byte_widths',bool(r) and width_ok)
 local=[x for x in r if x['local_parent']]
 check('local_attack_collider_observed',bool(local))
 check('exact_local_parent_from_bytes',bool(local) and width_ok and all(struct.unpack_from('<I',bytes.fromhex(x['despawn_link_bytes']))[0]==x['parent_entity']==x['latest_actor_entity']!=0 and x['entity']!=x['parent_entity'] for x in local))
 check('sequence_from_raw_matches_native_action',bool(local) and width_ok and all(struct.unpack_from('<I',bytes.fromhex(x['event_bytes']))[0]==x['sequence']==x['latest_actor_sequence']!=0 for x in local))
 check('native_collider_id_from_bytes',bool(local) and width_ok and any(x['collider_id'] for x in local) and all(struct.unpack_from('<Q',bytes.fromhex(x['collider_bytes']),8)[0]==x['collider_id'] for x in local))
 check('sample_during_local_action',bool(local) and any(x['latest_actor_active'] and x['latest_actor_item'] and 0<=x['milliseconds']-x['latest_actor_ms']<=200 for x in local))
 world=[]
 if width_ok:
  for x in local:
   b=bytes.fromhex(x['transform_bytes']);pos=struct.unpack_from('<3q',b);quat=struct.unpack_from('<4f',b,24);scale=struct.unpack_from('<3f',b,40)
   world.append(dict(sample=x['sample'],entity=x['entity'],parent=x['parent_entity'],sequence=x['sequence'],collider=x['collider_id'],position=[v/2**32 for v in pos],orientation=quat,scale=scale))
 check('finite_native_geometry',bool(world) and all(all(math.isfinite(v) for v in x['position']+list(x['orientation'])+list(x['scale'])) and abs(sum(v*v for v in x['orientation'])-1)<1e-3 for x in world))
 check('iterator_restored_and_quiescent',len(removed)==1 and removed[0].get('restored') and removed[0].get('callbacks')==0 and removed[0].get('failures')==0 and removed[0].get('samples')==len(r))
 return checks,dict(total=len(r),local=len(local),foreign=len(r)-len(local),entities=sorted(set(x['entity'] for x in local)),native_positions=len(set(tuple(x['position']) for x in world)),native_orientations=len(set(tuple(x['orientation']) for x in world)),geometry=world,limit='Read-only native collision observation. No controller collision, damage, source ownership beyond direct parent, or multiplayer behavior established.')

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=pathlib.Path);a.add_argument('--negative-tests',action='store_true');a=a.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];checks,evidence=audit(rows)
 if a.negative_tests:
  for name,change in [('parent',lambda r:r.update(parent_entity=r['parent_entity']+1)),('sequence',lambda r:r.update(sequence=r['sequence']+1)),('collider',lambda r:r.update(collider_id=r['collider_id']+1)),('unstable',lambda r:r.update(stable_read=False))]:
   altered=copy.deepcopy(rows);r=next((x for x in altered if x.get('event')=='sequence_collider_sample' and x['local_parent']),None)
   if r:change(r)
   bad,_=audit(altered);checks.append(dict(name='reject_'+name,status='PASS' if r and any(c['status']=='FAIL' for c in bad) else 'FAIL'))
 result=dict(checks=checks,passed=sum(x['status']=='PASS' for x in checks),failed=sum(x['status']=='FAIL' for x in checks),evidence=evidence)
 (a.probe/'sequence-collider-checks.json').write_text(json.dumps(result,indent=2)+'\n')
 for c in checks:print(c['status'],c['name'])
 print({k:v for k,v in evidence.items() if k!='geometry'});print('PASS',result['passed'],'FAIL',result['failed']);return bool(result['failed'])
if __name__=='__main__':raise SystemExit(main())
