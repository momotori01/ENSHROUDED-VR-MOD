"""World-qualified attachment alignment for the captured single melee weapon.
Identity calibration is established only for the observed item/slot/event, not all weapons.
"""
import argparse,copy,json,pathlib,struct

def audit(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 pairs=[r for r in ev('sequence_collider_sample') if 'native_weapon_slot_bytes' in r];bindings=ev('impact_query_world_binding');checks={};metrics=[]
 checks['calibration_samples']=len(pairs)>=3
 checks['no_observer_failure']=not any(r['event'].endswith('_failure') for r in rows)
 stable=width=world=owner=same=neutral=timing=True
 for r in pairs:
  a=bytes.fromhex(r['native_weapon_slot_bytes']);server=r['impact_server_slot'];b=bytes.fromhex(server.get('bytes',''))
  stable &= server.get('present',False) and server.get('stable',False) and r['stable_read']
  width &= len(a)==48 and len(b)==112
  if len(a)!=48 or len(b)!=112:continue
  same &= a==b[:48]
  parent=struct.unpack_from('<I',a,44)[0]
  owner &= parent==r['latest_actor_entity']!=0 and a[40]==0 and a[41]==9 and r['native_weapon_item']==r['latest_actor_item']!=0
  neutral &= struct.unpack_from('<6f',a)==(0.,)*6 and struct.unpack_from('<4f',a,24)==(0.,0.,0.,1.)
  timing &= 0<=r['milliseconds']-r['native_weapon_slot_ms']<=500
  physical=r['owner_lookup']['world'];visual=r['native_weapon_world'];actor=r['latest_actor_world']
  world &= physical==server['world']==actor!='0x0' and visual not in ('0x0',physical)
  world &= any(x['world']==visual and x['bytes']==176 and x.get('component_stride',176)==176 and x['query_pointer_match'] and x['us']<r['us'] for x in bindings)
  world &= any(x['world']==physical and x['bytes']==3104 and x.get('component_stride') in (3104,3128,3600) and x['query_pointer_match'] and x['us']<r['us'] for x in bindings)
  metrics.append(dict(entity=r['entity'],visual_entity=r['native_weapon_entity'],physical_world=physical,visual_world=visual,actor_world=actor,parent=parent,slot=a[41],local_offset=struct.unpack_from('<3f',a),world_offset=struct.unpack_from('<3f',a,12),orientation_offset=struct.unpack_from('<4f',a,24),age_ms=r['milliseconds']-r['native_weapon_slot_ms']))
 for k,v in [('stable_native_slots',stable),('reflected_slot_sizes',width),('exact_local_parent_slot_item',owner),('client_server_offsets_identical',same),('observed_weapon_identity_calibration',neutral),('qualified_camera_actor_worlds',world),('fresh_client_slot_snapshot',timing)]:checks[k]=bool(pairs) and v
 return checks,metrics

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,e=audit(rows)
 if a.negative_tests:
  for name,field,value in [('wrong_actor_world','latest_actor_world','0x0'),('cross_world_alias','native_weapon_world','0x0'),('wrong_item','native_weapon_item',0),('missing_slot','native_weapon_slot_bytes','00'*48)]:
   bad=copy.deepcopy(rows);r=next((r for r in bad if r['event']=='sequence_collider_sample' and 'native_weapon_slot_bytes' in r),None)
   if r:r[field]=value
   c['reject_'+name]=r is not None and not all(audit(bad)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),samples=e,limitation=__doc__)
 (a.probe/'weapon-attachment-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed']);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
