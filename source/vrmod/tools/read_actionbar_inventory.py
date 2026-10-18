"""Read-only discovery of the local actionbar contents (which item is in which slot).
Uses the same-launch probe log for the World address (motion_visual_selection) and the
actionbar entity IDs (input_slot_selection, SlotSelection+216), then PROCESS_VM_READ only:
registry -> World+0x148 entity map -> archetype layout -> component storage.
It dumps component bytes and one level of pointed-to memory and searches for known item IDs.
Never writes memory, calls game code, injects input or changes the save. Layout findings are
candidates until confirmed by a second launch.
"""
import argparse,json,pathlib,struct,time
from read_game_memory import Process
from inject_camera_observer import creation_filetime
from read_component_registry import read_registry

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path)
 p.add_argument('--known',type=lambda s:[int(x) for x in s.split(',')],default=[1940764111,327157552,3851016100,3936779840,3474096535])
 a=p.parse_args();launch=a.probe.parent;game=pathlib.Path('enshrouded.exe').resolve();meta=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
 if time.time()-(launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('managed telemetry stale; the game must still be running')
 rows=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines() if x.strip()]
 worlds=sorted({r['world'] for r in rows if r['event']=='motion_visual_selection' and r.get('world')})
 bars=sorted({e for r in rows if r['event']=='input_slot_selection' for e in r['actionbar_entities'] if e})
 slots=[r['actionbar_slot'] for r in rows if r['event']=='input_slot_selection']
 if not worlds or not bars:raise ValueError('probe log lacks World or actionbar entity evidence')
 proc=Process(meta['pid'],game);out=dict(process=meta['pid'],creation_filetime=meta['creation_filetime'],worlds=worlds,actionbar_entities=bars,logged_slots=slots,known_items=a.known,read_only=True,results=[])
 def read(addr,size):
  b=proc.read(addr,size)
  if b is None:raise ValueError(f'unreadable {addr:x}')
  return b
 def value(addr,fmt):return struct.unpack('<'+fmt,read(addr,struct.calcsize('<'+fmt)))[0]
 def heap(v):return 0x10000<v<0x7fffffffffff and v&7==0
 def find(blob,ids):
  hits=[]
  for i in ids:
   key=struct.pack('<I',i);at=blob.find(key)
   while at>=0:hits.append(dict(item=i,offset=at));at=blob.find(key,at+1)
  return hits
 try:
  if creation_filetime(proc.handle)!=int(meta['creation_filetime']):raise ValueError('process generation changed')
  for w in worlds:
   world=int(w,16);entry=dict(world=w)
   try:
    reg=read_registry(proc,world);types=reg['records'];entry['registry_count']=reg['count']
    entry['interesting_types']=[dict(id=r['id'],name=r['qualified_name'],size=r['size']) for r in types if any(k in r['qualified_name'] for k in ('Inventory','Actionbar','ActionBar','Slot','Item','Equipment'))]
    bitmap,capacity,keys,values=[value(world+0x148+off,'Q') for off in [8,16,40,64]]
    if not 0<capacity<=0x100000 or capacity&(capacity-1):raise ValueError('World map bounds')
    bits=read(bitmap,(capacity+7)//8);ids=read(keys,capacity*4);records=read(values,capacity*8)
    occupied=[i for i in range(capacity) if bits[i//8]>>(i%8)&1]
    entry['entities']=[]
    for eid in bars:
     at=next((i for i in occupied if struct.unpack_from('<I',ids,i*4)[0]==eid),None)
     if at is None:entry['entities'].append(dict(entity=eid,present=False));continue
     record=struct.unpack_from('<Q',records,at*8)[0];raw=read(record,56)
     layout,data,row=struct.unpack_from('<Q',raw,24)[0],struct.unpack_from('<Q',raw,32)[0],struct.unpack_from('<I',raw,48)[0]
     desc=read(layout,0xf84);comps=[]
     for idx in range(min(640,reg['count'])):
      if not (struct.unpack_from('<Q',desc,(idx//64)*8)[0]>>(idx%64)&1):continue
      stride=struct.unpack_from('<H',desc,0xa84+idx*2)[0];offset=struct.unpack_from('<H',desc,0x84+idx*2)[0]
      t=types[idx];c=dict(id=idx,name=t['qualified_name'],size=t['size'],stride=stride)
      if stride and stride==t['size'] and stride<=4096:
       addr=data+offset+row*stride;b=read(addr,stride);c['bytes']=b.hex();c['hits']=find(b,a.known);c['pointers']=[]
       for o in range(0,len(b)-7,8):
        v=struct.unpack_from('<Q',b,o)[0]
        if heap(v):
         target=proc.read(v,512)
         if target:c['pointers'].append(dict(offset=o,address=hex(v),bytes=target.hex(),hits=find(target,a.known)))
      comps.append(c)
     entry['entities'].append(dict(entity=eid,present=True,row=row,components=comps))
   except ValueError as e:entry['error']=str(e)
   out['results'].append(entry)
  path=a.probe/'actionbar-inventory-dump.json';path.write_text(json.dumps(out,indent=1)+'\n',encoding='utf8')
  print(path)
  for e in out['results']:
   print('world',e['world'],'error',e.get('error'),'types',[(t['id'],t['name'],t['size']) for t in e.get('interesting_types',[])][:30])
   for ent in e.get('entities',[]):
    print(' entity',ent['entity'],'present',ent['present'])
    for c in ent.get('components',[]):
     hits=c.get('hits',[])+[dict(ptr=p['offset'],**h) for p in c.get('pointers',[]) for h in p['hits']]
     print('  ',c['id'],c['name'],c['size'],'hits',hits[:12])
 finally:proc.close()
if __name__=='__main__':main()
