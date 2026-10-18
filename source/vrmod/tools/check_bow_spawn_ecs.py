"""Audit the native projectile snapshot reached through a live ranged query."""
import argparse,json,math,struct
from pathlib import Path
from check_capture_cameras import rotate
from check_bow_spawn_entity import audit as entity_audit

def evidence(events):
 ev=lambda n:[x for x in events if x['event']==n]
 ecs=ev('bow_spawn_ecs_observed');entities=ev('bow_spawn_entity_observed');shots=ev('bow_spawn_sample')
 checks=dict(single=len(ecs)==len(entities)==len(shots)==1,live_owner=True,component_inventory=True,projectile=True,initial_transform=True,velocity_direction=True,items_and_owner=True)
 details=[]
 for x,o,s in zip(ecs,entities,shots):
  checks['live_owner'] &= x['sample']==o['sample']==s['sample'] and x['world']==o['native_world'] and x['owner']==o['native_entity'] and x['actor']==o['actor'] and x['spawned_entity']==o['spawned_entity'] and x['actor_type']=='keen::ecs::Actor' and x['live_query_owner_match'] and x['read_only'] and x['stable'] and o['us']<=x['us'] and 0<=x['age_ms']<=500
  rows=x['components'];cs={v['name']:v for v in rows}
  checks['component_inventory'] &= len(rows)==x['component_count']==len(cs)==len({v['id'] for v in rows}) and 0<len(rows)<=96 and all(0<=v['id']<640 and int(v['address'],16)>0 and (len(v['bytes'])==2*v['size'] if v['size']<=256 else not v['bytes']) for v in rows) and sum(len(v['bytes'])//2 for v in rows)<=4096
  expected={'Projectile':1,'CurrentTransform':56,'LastTransform':72,'Velocity':12,'UsedItem':4,'ImpactItemConfig':44,'ImpactHierarchy':16,'WeaponReference':8}
  complete=all('keen::ecs::'+name in cs and cs['keen::ecs::'+name]['size']==size and len(cs['keen::ecs::'+name]['bytes'])==2*size for name,size in expected.items())
  checks['projectile'] &= complete
  if not complete:
   for name in ['initial_transform','velocity_direction','items_and_owner']:checks[name]=False
   continue
  b={name:bytes.fromhex(cs['keen::ecs::'+name]['bytes']) for name in expected}
  transform=bytes.fromhex(s['transform_bytes']);forward=rotate(struct.unpack_from('<4f',transform,24),[0,0,1])
  checks['initial_transform'] &= b['CurrentTransform'][:52]==b['LastTransform'][:52]==transform[:52]
  velocity=struct.unpack('<3f',b['Velocity']);speed=math.sqrt(sum(v*v for v in velocity))
  direction_error=max(abs(velocity[i]/speed-forward[i]) for i in range(3)) if math.isfinite(speed) and speed>0 else math.inf
  checks['velocity_direction'] &= math.isfinite(speed) and 1<speed<1000 and direction_error<2e-6
  item=struct.unpack('<I',b['UsedItem'])[0];impact_item=struct.unpack_from('<I',b['ImpactItemConfig'],8)[0];ammo=struct.unpack_from('<I',b['ImpactItemConfig'],28)[0]
  hierarchy=struct.unpack('<4I',b['ImpactHierarchy']);weapon,owner=struct.unpack('<2I',b['WeaponReference'])
  checks['items_and_owner'] &= item==impact_item==327157552 and ammo==1454438654 and hierarchy[0]==hierarchy[1]==hierarchy[3]==owner==x['owner'] and weapon!=0 and hierarchy[2]!=0
  details.append(dict(entity=x['spawned_entity'],age_ms=x['age_ms'],speed_m_s=speed,direction_error=direction_error,item=item,ammo=ammo,weapon_entity=weapon,weapon_pide=hierarchy[2],components=len(rows)))
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,projectiles=details)

def audit(p):
 events=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text().splitlines()]
 r=evidence(events);r['parent']=entity_audit(p);r['checks']['tracked_entity_chain']=r['parent']['status']=='PASS'
 r['status']='PASS' if all(r['checks'].values()) else 'FAIL'
 r['limitation']='One known Mock projectile exists in a live query with initial tracked transform, matching velocity and native bow/ammo ownership. Snapshot precedes integration: flight/collision, physical power, repeat nocking and hardware remain unverified.'
 return r
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe)
 (v.probe/'bow-spawn-ecs-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
