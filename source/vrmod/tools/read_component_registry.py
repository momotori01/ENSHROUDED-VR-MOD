"""Read the live ECS registry via a completed native target query proof.
World[0] -> registry: +8 records, +16 count, +24 capacity; record stride256.
Recovered from native 8964e0/8971d0. Read-only; no process calls or writes.
Numeric IDs are accepted only with same-process query witnesses and PE metadata.
"""
import argparse,hashlib,json,pathlib,struct,time
from read_game_memory import Process
from inject_camera_observer import creation_filetime

def read_registry(proc, world):
 def read(a,n):
  b=proc.read(a,n)
  if b is None:raise ValueError('unreadable registry storage')
  return b
 def text(ptr,n):
  if not proc.base<=ptr<proc.base+0x3000000 or not 0<n<=256:raise ValueError('unbounded registry name')
  return read(ptr,n).decode('ascii')
 registry=struct.unpack('<Q',read(world,8))[0]
 header=read(registry,32);array,count,capacity=struct.unpack_from('<QQQ',header,8)
 if not 0<count<=capacity<=640:raise ValueError('registry bounds differ from reflected mask')
 raw=read(array,count*256);rows=[]
 for i in range(count):
  b=raw[i*256:(i+1)*256];np,n,qp,qn=struct.unpack_from('<4Q',b);runtime,author=struct.unpack_from('<QQ',b,0x28);size=struct.unpack_from('<H',b,0x38)[0]
  if runtime and not proc.base<=runtime<proc.base+0x3000000:raise ValueError('runtime metadata outside verified image')
  meta=read(runtime,144) if runtime else None
  if meta and struct.unpack_from('<I',meta,64)[0]!=size:raise ValueError('runtime metadata size mismatch')
  runtime_name=text(*struct.unpack_from('<QQ',meta,32)) if meta else None
  rows.append(dict(id=i,name=text(np,n),qualified_name=text(qp,qn),runtime_name=runtime_name,runtime_rva=hex(runtime-proc.base) if runtime else None,author_rva=hex(author-proc.base) if author else None,size=size,hash=struct.unpack_from('<I',b,32)[0]))
 if struct.unpack('<Q',read(world,8))[0]!=registry or header!=read(registry,32) or raw!=read(array,count*256):raise ValueError('registry changed during read')
 if len({r['qualified_name'] for r in rows})!=count:raise ValueError('duplicate registry names')
 return dict(registry=hex(registry),array=hex(array),count=count,capacity=capacity,record_size=256,records=rows,stable_read=True)

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=pathlib.Path);args=a.parse_args();folder=args.probe;meta=json.loads((folder.parent/'launch.json').read_text(encoding='utf-8-sig'))
 if time.time()-(folder.parent/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('stale managed telemetry')
 logs=[json.loads(s) for s in (folder/'view-resource-probe.jsonl').read_text().splitlines()];regs=[r for r in logs if r['event']=='motion_target_registry'];ends=[r for r in logs if r['event']=='motion_target_removed']
 if len(regs)!=1 or len(ends)!=1 or not ends[0]['restored'] or ends[0]['failures'] or ends[0]['callbacks']:raise ValueError('completed native query proof required')
 reg=regs[0];p=Process(meta['pid'],pathlib.Path('enshrouded.exe').resolve())
 try:
  if creation_filetime(p.handle)!=int(meta['creation_filetime']):raise ValueError('different process generation')
  out=read_registry(p,int(reg['world'],16));types=out['records'];checks=[]
  def witness(i,name,size):
   row=types[i];ok=row['qualified_name']=='keen::ecs::'+name and row['size']==size;checks.append(dict(id=i,name=name,size=size,pass_match=ok))
   if not ok:raise ValueError('native query disagrees with registry: '+name)
  witness(reg['health_type'],'Health',68);witness(reg['current_type'],'CurrentTransform',56)
  for r in logs:
   if r.get('world')!=reg['world']:continue
   if r['event']=='impact_query_world_binding' and r['bytes']==3104:witness(r['component_id'],'Actor' if r['component_stride']==3600 else 'ClientActor',r['component_stride'])
   if r['event']=='impact_component_binding':
    witness(r['hierarchy_component_id'],'ImpactHierarchy',16);witness(r['item_component_id'],'ImpactItemConfig',44)
  out.update(process=meta['pid'],creation_filetime=meta['creation_filetime'],world=reg['world'],exe_sha256=hashlib.sha256(pathlib.Path('enshrouded.exe').read_bytes()).hexdigest(),query_witnesses=checks,read_only=True)
  path=folder/'component-registry.json';path.write_text(json.dumps(out,indent=2)+'\n',encoding='utf8');print(path,'count',out['count'],'capacity',out['capacity'],'witnesses',len(checks))
  for r in types:
   if r['name'] in ['Faction','Actor','Health','CurrentTransform','Aggro','NpcState','Player','TestA','WindMotorComponent']:print(r)
 finally:p.close()
if __name__=='__main__':main()
