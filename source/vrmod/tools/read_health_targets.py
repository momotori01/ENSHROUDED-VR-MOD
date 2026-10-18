"""Independent read-only target inventory from a same-process native query proof.
Never writes memory, injects input, calls game functions, or classifies hostility.
The snapshot is sequential and may omit concurrently changing entities.
"""
import argparse,json,pathlib,struct,time,math
from read_game_memory import Process
from inject_camera_observer import creation_filetime
from read_component_registry import read_registry

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args()
 launch=a.probe.parent;game=pathlib.Path('enshrouded.exe').resolve();meta=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
 if time.time()-(launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('managed telemetry stale')
 rows=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()]
 registry=[x for x in rows if x['event']=='motion_target_registry'];rem=[x for x in rows if x['event']=='motion_target_removed']
 if len(registry)!=1 or len(rem)!=1 or not rem[0]['restored'] or rem[0]['callbacks'] or rem[0]['failures']:raise ValueError('completed single-world query proof required')
 reg=registry[0];world=int(reg['world'],16);health=reg['health_type'];current=reg['current_type'];local=reg['local_entity']
 actor=next(x for x in rows if x['event']=='impact_query_world_binding' and x['world']==reg['world'] and x['bytes']==3104 and x['query_pointer_match'])
 proc=Process(meta['pid'],game);skipped=0;candidates=[];cache={}
 def read(addr,size):
  b=proc.read(addr,size)
  if b is None:raise ValueError(f'unreadable target metadata {addr:x}')
  return b
 def value(addr,fmt):return struct.unpack('<'+fmt,read(addr,struct.calcsize('<'+fmt)))[0]
 try:
  if creation_filetime(proc.handle)!=int(meta['creation_filetime']):raise ValueError('process generation changed')
  # Re-read the exact native query's type-ID storage; never reuse a numeric ID
  # from another process or just trust the label in the JSON.
  query=bytes.fromhex(reg['query_bytes']);typeptr=struct.unpack_from('<Q',query,0x58)[0]
  if value(typeptr,'H')!=health:raise ValueError('Health lookup type changed')
  registry=read_registry(proc,world);registry_types=registry['records'];named={r['qualified_name']:r for r in registry_types}
  for idx,name,size in [(health,'Health',68),(current,'CurrentTransform',56),(actor['component_id'],'Actor',actor['component_stride'])]:
   if registry_types[idx]['qualified_name']!='keen::ecs::'+name or registry_types[idx]['size']!=size:raise ValueError('registry/query disagree')
  faction=named['keen::ecs::Faction']
  if faction['runtime_name']!='keen::ecs::Faction' or faction['size']!=1:raise ValueError('unexpected Faction ABI')
  import hashlib
  enums=json.loads(pathlib.Path('docs/analysis/motion_faction_enum.json').read_text())
  if enums['exe_sha256']!=hashlib.sha256(game.read_bytes()).hexdigest():raise ValueError('faction enum belongs to another binary')
  faction_names={r['value']:r['name'] for r in enums['types'][0]['values']}
  bitmap,capacity,keys,values=[value(world+0x148+off,'Q') for off in [8,16,40,64]]
  if capacity!=reg['capacity'] or not 0<capacity<=0x100000 or capacity&(capacity-1):raise ValueError('World map bounds changed')
  bits=read(bitmap,(capacity+7)//8);ids=read(keys,capacity*4);records=read(values,capacity*8)
  def entity(at):
   eid=struct.unpack_from('<I',ids,at*4)[0];record=struct.unpack_from('<Q',records,at*8)[0]
   raw=read(record,56);layout,data,row=struct.unpack_from('<Q',raw,24)[0],struct.unpack_from('<Q',raw,32)[0],struct.unpack_from('<I',raw,48)[0]
   if row>0x1000000:raise ValueError('row outside bounds')
   if layout not in cache:cache[layout]=read(layout,0xf84)
   desc=cache[layout]
   def component(idx,size):
    if not 0<=idx<640:return 0
    if not (struct.unpack_from('<Q',desc,(idx//64)*8)[0]>>(idx%64)&1):return 0
    stride=struct.unpack_from('<H',desc,0xa84+idx*2)[0];offset=struct.unpack_from('<H',desc,0x84+idx*2)[0]
    return data+offset+row*stride if stride==size else 0
   return eid,record,raw,component,desc
  occupied=[i for i in range(capacity) if bits[i//8]>>(i%8)&1]
  root_at=next(i for i in occupied if struct.unpack_from('<I',ids,i*4)[0]==local)
  _,_,_,rootcomp,_=entity(root_at);root_ptr=rootcomp(current,56)
  if not root_ptr:raise ValueError('local CurrentTransform absent')
  root=struct.unpack_from('<3q',read(root_ptr,56))
  for at in occupied:
   try:
    eid,record,raw,component,desc=entity(at);hp=component(health,68);transform=component(current,56)
    if not hp or not transform:continue
    h=read(hp,68);t=read(transform,56);offset,count=struct.unpack_from('<II',h,8)
    if not h[16]&1 or offset!=36 or not 0<count<=8:continue
    q=struct.unpack_from('<3q',t);relative=[(q[i]-root[i])/2**32 for i in range(3)];distance=math.dist(relative,[0,0,0])
    if not math.isfinite(distance) or distance>250:continue
    if raw!=read(record,56) or h!=read(hp,68) or t!=read(transform,56) or value(keys+at*4,'I')!=eid or value(values+at*8,'Q')!=record:skipped+=1;continue
    has_actor=bool(component(actor['component_id'],actor['component_stride']))
    fp=component(faction['id'],1);fv=value(fp,'B') if fp else None
    if fp and fv!=value(fp,'B'):skipped+=1;continue
    types=[(i,struct.unpack_from('<H',desc,0xa84+i*2)[0]) for i in range(640) if struct.unpack_from('<Q',desc,(i//64)*8)[0]>>(i%64)&1] if has_actor else []
    candidates.append(dict(entity=eid,distance_m=distance,relative_m=relative,position_q32=q,health_bytes=h.hex(),transform_bytes=t.hex(),has_actor=has_actor,components=types,faction_value=fv,faction_name=faction_names.get(fv),stable_read=True))
   except ValueError:skipped+=1
  stable_map=[value(world+0x148+off,'Q') for off in [8,16,40,64]]==[bitmap,capacity,keys,values]
  root_same=root==struct.unpack_from('<3q',read(root_ptr,56));candidates.sort(key=lambda r:r['distance_m'])
  out=dict(process=meta['pid'],creation_filetime=meta['creation_filetime'],world=reg['world'],local_entity=local,health_type=health,current_type=current,actor_type=actor['component_id'],faction_type=faction['id'],component_registry=registry,root_q32=root,root_position_unchanged=root_same,map_storage_unchanged=stable_map,capacity=capacity,occupied=len(occupied),layout_count=len(cache),candidate_count=len(candidates),skipped=skipped,all_candidates=candidates,nearest=candidates[:32],nearest_other_actors=[r for r in candidates if r['has_actor'] and r['entity']!=local][:16],hostility_unclassified=True,read_only=True,limitation=__doc__)
  output=a.probe/'independent-health-targets.json';output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
  print(output);print('candidates',len(candidates),'other actors',len(out['nearest_other_actors']),'root stable',root_same,'map stable',stable_map,'skipped',skipped)
  for q in out['nearest_other_actors']:print(q['entity'],round(q['distance_m'],3),q['relative_m'],q['faction_name'])
  return int(not(stable_map and root_same))
 finally:proc.close()
if __name__=='__main__':raise SystemExit(main())
