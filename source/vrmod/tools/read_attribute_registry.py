"""Read the native attribute table from a completed Health target query.
No game functions, process writes, movement, damage or host setting changes.
Index meaning comes from native fe5fe0/fe5f30 and Health caller365962;
a static table/positive health value is not evidence of an actual hit.
"""
import argparse,hashlib,json,pathlib,struct,time
from read_game_memory import Process
from inject_camera_observer import creation_filetime

KEY=0x8eb84995
SHA='af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781'
def locate(key,capacity,bits,keys):
 if capacity<1 or capacity&(capacity-1) or len(bits)!=(capacity+63)//64*8 or len(keys)!=capacity*4:raise ValueError('table snapshot bounds')
 at=key&(capacity-1)
 for _ in range(capacity):
  if not (bits[at//8]>>(at%8)&1):return None
  if struct.unpack_from('<I',keys,at*4)[0]==key:return at
  at=(at+1)%capacity
 raise ValueError('full table without matching key has no native termination')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();launch=a.probe.parent
 meta=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'));game=pathlib.Path('enshrouded.exe').resolve()
 if hashlib.sha256(game.read_bytes()).hexdigest()!=SHA:raise ValueError('unsupported executable')
 if time.time()-(launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('stale managed telemetry')
 rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
 regs=[r for r in rows if r['event']=='motion_target_registry'];ends=[r for r in rows if r['event']=='motion_target_removed']
 if len(regs)!=1 or len(ends)!=1 or not ends[0]['restored'] or ends[0]['callbacks'] or ends[0]['failures']:raise ValueError('completed target query proof required')
 source=regs[0];query=bytes.fromhex(source['query_bytes']);registry=struct.unpack_from('<Q',query,24)[0]
 proc=Process(meta['pid'],game)
 def read(addr,n):
  b=proc.read(addr,n)
  if b is None:raise ValueError(f'unreadable attribute storage {addr:x}')
  return b
 try:
  if creation_filetime(proc.handle)!=int(meta['creation_filetime']):raise ValueError('process generation differs')
  table=registry+0x68;header=read(table,88)
  bitmap,capacity,keys,values=[struct.unpack_from('<Q',header,o)[0] for o in (8,16,40,64)];count=struct.unpack_from('<I',header,84)[0]
  if not registry or not bitmap or not keys or not values or not 0<count<capacity<=16384 or capacity&(capacity-1):raise ValueError('unexpected attribute table bounds')
  bits=read(bitmap,(capacity+63)//64*8);rawkeys=read(keys,capacity*4);rawvalues=read(values,capacity*32)
  occupied=[i for i in range(capacity) if bits[i//8]>>(i%8)&1]
  if len(occupied)!=count:raise ValueError('attribute count mismatch')
  entries=[]
  for i in occupied:
   key=struct.unpack_from('<I',rawkeys,i*4)[0];b=rawvalues[i*32:(i+1)*32]
   if locate(key,capacity,bits,rawkeys)!=i:raise ValueError('attribute key duplicate/unreachable')
   nameptr,namelen=struct.unpack_from('<QQ',b,16)
   if not nameptr or not 0<namelen<=256:raise ValueError('attribute name bounds')
   namebytes=read(nameptr,namelen);name=namebytes.decode('utf8')
   if namebytes!=read(nameptr,namelen):raise ValueError('attribute name changed')
   entries.append(dict(bucket=i,key=key,key_hex=hex(key),bytes=b.hex(),index=struct.unpack_from('<H',b,12)[0],name=name,name_bytes=namebytes.hex(),root_key=struct.unpack_from('<I',b)[0],metadata_key=struct.unpack_from('<I',b,4)[0]))
  at=locate(KEY,capacity,bits,rawkeys)
  if at is None:raise ValueError('native Health key missing')
  selected=next(r for r in entries if r['bucket']==at)
  if selected['index']>=8:raise ValueError('native Health key outside reflected container size')
  if header!=read(table,88) or bits!=read(bitmap,len(bits)) or rawkeys!=read(keys,len(rawkeys)) or rawvalues!=read(values,len(rawvalues)):raise ValueError('attribute map changed')
  candidates=[r for r in rows if r['event']=='motion_target_candidate'];health=[]
  for r in candidates:
   b=bytes.fromhex(r['health_bytes']);offset,count=struct.unpack_from('<II',b,8)
   if len(b)!=68 or offset!=36 or not b[16]&1 or not selected['index']<count<=8:raise ValueError('Health snapshot incompatible')
   health.append(dict(entity=r['entity'],world=r['world'],health_bytes=r['health_bytes'],index=selected['index'],value=struct.unpack_from('<i',b,offset+4*selected['index'])[0],observed_ms=r['observed_ms'],historical_snapshot=True))
  out=dict(exe_sha256=SHA,process=meta['pid'],creation_filetime=meta['creation_filetime'],world=source['world'],query_bytes=query.hex(),registry=hex(registry),table=hex(table),header=header.hex(),capacity=capacity,count=len(entries),bitmap=bits.hex(),keys=rawkeys.hex(),values=rawvalues.hex(),entries=entries,health_key=KEY,selected=selected,health_snapshots=health,table_stable=True,read_only=True,live_target_damage_proven=False,limitation=__doc__)
  path=a.probe/'attribute-registry.json';path.write_text(json.dumps(out,indent=2)+'\n',encoding='utf8');print(path);print('records',len(entries),'Health index',selected['index'],'historical Health',[(h['entity'],h['value']) for h in health[:8]])
 finally:proc.close()
if __name__=='__main__':main()
