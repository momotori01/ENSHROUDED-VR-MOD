"""Correlate native Crosshair projectile transforms with the local bow fire state."""
import argparse,json,struct,math
from pathlib import Path
from check_capture_cameras import rotate,qmul

def audit(p):
 e=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text().splitlines()];ev=lambda n:[x for x in e if x['event']==n]
 sp=ev('bow_spawn_sample');shots=[x for x in sp if x['is_projectile']];native=ev('input_bow_sample');removed=ev('bow_spawn_removed')
 checks=dict(enabled=len(ev('bow_spawn_enabled'))==1,projectile_observed=bool(shots),original_sources_unchanged=bool(sp) and all(x['sources_unchanged'] and x['read_only'] and x['result'] for x in sp),
 native_local_bow_correlation=True,known_event_type=True,finite_unit_transform=True,
 hook_restored=len(removed)==1 and all(x['restored'] and not x['callbacks'] and not x['failures'] and x['samples']==len(sp) for x in removed),
 completed=any(x['result']==0 for x in ev('view_resource_hook_complete')),no_failure=not any('failure' in x['event'] for x in e))
 rows=[]
 for x in shots:
  b=bytes.fromhex(x['event_bytes']);c=bytes.fromhex(x['context_bytes']);o=bytes.fromhex(x['transform_bytes']);camera=bytes.fromhex(x['camera_bytes']);q=struct.unpack_from('<4f',o,24);scale=struct.unpack_from('<3f',o,40)
  candidates=[v for v in native if v.get('input')==x['input']];near=min(candidates,key=lambda v:abs(v['us']-x['us'])) if candidates else None
  item=state=None;dt=None;world=None
  if near:
   w=bytes.fromhex(near['weapon_bytes']);item=struct.unpack_from('<I',w,0x4c)[0];state=w[0];dt=abs(near['us']-x['us']);world=near['world']
  checks['native_local_bow_correlation'] &= bool(near) and item==327157552 and state in [3,4] and dt<250000 and near['entity']==x['local_entity'] and int(near['weapon'],16)==struct.unpack_from('<Q',c,0xb8)[0]
  checks['known_event_type'] &= x.get('typed_dispatch_path') and x.get('parent_caller')=='0x1e951d' and x.get('event_type_hash')==0x51e20bf3 and b[0x148]==1 and b[0x90] in [2,3]
  checks['finite_unit_transform'] &= all(math.isfinite(v) for v in q+scale) and abs(sum(v*v for v in q)-1)<.002 and max(abs(v-1) for v in scale)<1e-5
  pos=struct.unpack_from('<3q',o);campos=struct.unpack_from('<3q',camera);camq=struct.unpack_from('<4f',camera,24);dist=struct.unpack_from('<f',camera,40)[0]
  off=struct.unpack_from('<10f',b,0xc0);radius=struct.unpack_from('<2f',b,0x104);angles=struct.unpack_from('<6f',b,0x10c)
  deterministic=radius==(0.,0.) and all(angles[i]==angles[i+1] for i in [0,2,4])
  pe=qe=None
  if deterministic:
   local=rotate(camq,off[:3]);fw=rotate(camq,[0,0,1]);expected=[campos[i]/2**32+fw[i]*dist+local[i]+off[i+3] for i in range(3)]
   axis=lambda i,a:[math.sin(math.radians(a)/2) if j==i else math.cos(math.radians(a)/2) if j==3 else 0 for j in range(4)]
   spread=qmul(qmul(axis(1,-angles[2]),axis(2,-angles[4])),axis(0,-angles[0]));expectedq=qmul(qmul(camq,off[6:10]),spread)
   pe=max(abs(a/2**32-b) for a,b in zip(pos,expected));qe=min(max(abs(a-sign*b) for a,b in zip(q,expectedq)) for sign in [-1,1])
  rows.append(dict(world=world,last_xr_frame=x['last_xr_frame'],item=item,native_state=state,correlation_delay_us=dt,spawn_kind=b[0x90],camera_distance=dist,spawn_offset=off,random_radius=radius,random_angles=angles,deterministic=deterministic,position_error_m=pe,quaternion_error=qe,template_guid=b[0x80:0x90].hex(),source_event_address=x['event_address']))
 comparable=[r for r in rows if r['deterministic']]
 checks['deterministic_math']=all(r['position_error_m']<3e-6 and r['quaternion_error']<3e-6 for r in comparable)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,samples=len(sp),projectiles=len(shots),deterministic_math_samples=len(comparable),projectile_rows=rows,limitation='Read-only native Crosshair call and local bow state correlation. Does not establish the final spawned ECS entity, collision, controller aiming or physical draw power. Nondegenerate spread is retained but not replayed.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'bow-spawn-observer-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
