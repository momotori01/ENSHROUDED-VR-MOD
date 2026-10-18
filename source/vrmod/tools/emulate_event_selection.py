"""Replay actual native event selection and GUID conditions in an isolated CPU.
No game process or shared asset is modified. Only relative pointers are relocated
inside a private fixture; native instructions 1d93d0 and 3366a0 run unmodified.
"""
import argparse,json,pathlib,struct,copy
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import *
IMPACT=0x64b5dc20
ZERO=bytes(16)

def allowed(event,tags):
 b=bytes.fromhex(event['base_event']);required=b[16:32]
 if required!=ZERO and required not in tags:return False
 for at in range(32,96,16):
  denied=b[at:at+16]
  if denied==ZERO:break
  if denied in tags:return False
 return True

def select_reference(events,start,end,tags,type_hash=None,overlap=False):
 if start==end:return []
 found=[]
 for i,event in enumerate(events):
  at,duration=struct.unpack_from('<qq',bytes.fromhex(event['base_event']))
  # Authoring fixture uses ordinary finite times; sentinel cases separately.
  if at>=end:break
  if at<start and not(overlap and at+duration>=start):continue
  if type_hash is not None and type_hash!=event['type_hash']:continue
  if allowed(event,tags):found.append(i)
 return found

class EventCPU(CameraMath):
 def on_instruction(self,cpu,address,size,unused):
  rva=address-self.base
  if not(0x1d93d0<=rva<0x1d95cc or 0x3366a0<=rva<0x33679d):raise ValueError(f'Unexpected native instruction {rva:x}')
 def select(self,events,start,end,tags,type_hash=None,overlap=False,capacity=256,null=False):
  if len(events)>64 or len(tags)>8:raise ValueError('bounded fixture only')
  d=self.data;c=self.cpu;c.mem_write(d,bytes(0x10000))
  sub=d+0x1000;variants=d+0x1100;payload=d+0x2000;out=d+0x100;array=d+0xb000;state=d+0xc000;span=d+0x200;filt=d+0x300
  c.mem_write(sub,struct.pack('<iI',variants-sub,len(events)))
  for i,event in enumerate(events):
   v=variants+i*12;p=payload+i*0x200;b=bytes.fromhex(event['base_event'])
   if len(b)!=96:raise ValueError('base event size')
   c.mem_write(v,struct.pack('<IiI',event['type_hash'],p-(v+4),len(b)));c.mem_write(p,b)
  c.mem_write(state,b''.join(tags));c.mem_write(span,struct.pack('<QQ',state,len(tags)))
  c.mem_write(out,struct.pack('<4Q',array,0,capacity,0));c.mem_write(filt,struct.pack('<IB',type_hash or 0,int(type_hash is not None)))
  before=bytes(c.mem_read(d,0x10000));sp=self.stack+0xf008
  c.mem_write(sp+0x28,struct.pack('<qqB',start,end,int(overlap)));c.reg_write(UC_X86_REG_R9,filt)
  self.invoke(0x1d93d0,out,0 if null else sub,span)
  ok=bool(c.reg_read(UC_X86_REG_RAX)&255);ptr,count,cap,alloc=struct.unpack('<4Q',c.mem_read(out,32))
  if ptr!=array or count>capacity or cap!=capacity or alloc:raise ValueError('native output header violated bounds')
  addresses=struct.unpack('<'+'Q'*count,c.mem_read(array,count*8)) if count else []
  selected=[(p-variants)//12 for p in addresses]
  if any(p!=variants+i*12 or not 0<=i<len(events) for p,i in zip(addresses,selected)):raise ValueError('output is not a fixture variant')
  expected=bytearray(before);struct.pack_into('<Q',expected,0x108,count);expected[0xb000:0xb000+count*8]=c.mem_read(array,count*8)
  unchanged=bytes(c.mem_read(d,0x10000))==bytes(expected)
  return selected,ok,unchanged

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('docs/analysis/emulated_event_selection.json'));a=p.parse_args()
 rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];asset=next(r for r in rows if r['event']=='sequence_asset_snapshot');events=asset['subsequences'][0]['events'];selections=[r for r in rows if r['event']=='motion_event_selection']
 active=[r for r in rows if r['event']=='action_sequence_snapshot' and bytes.fromhex(r['base_actor_bytes'])[0x1b1]&1]
 tagsets={bytes.fromhex(r['base_actor_bytes'])[0xec:0x16c] for r in active}
 if any('state_tags' not in r for r in selections) and len(tagsets)!=1:raise ValueError('Older trace requires observed constant tags; do not guess nearest snapshot')
 fallback=next(iter(tagsets));engine=EventCPU(pathlib.Path('enshrouded.exe'));checks={};cases=[]
 def test(name,source,lo,hi,tags,expected=None,**kwargs):
  actual,ok,unchanged=engine.select(source,lo,hi,tags,**kwargs)
  if expected is None:expected=select_reference(source,lo,hi,tags,kwargs.get('type_hash'),kwargs.get('overlap',False))
  success=actual==expected and ok and unchanged
  checks[name]=success;cases.append(dict(name=name,start=lo,end=hi,selected=actual,expected=expected,input_unchanged=unchanged,**kwargs))
 for r in selections:
  raw=bytes.fromhex(r['state_tags']) if 'state_tags' in r else fallback;tags=[raw[i:i+16] for i in range(0,128,16)]
  expected=[next(i for i,e in enumerate(events) if e['address']==v['payload']) for v in r['events']]
  test('live_range_'+str(r['sample']),events,*r['range'],tags,expected)
 tags=[fallback[i:i+16] for i in range(0,128,16)];impacts=[i for i,e in enumerate(events) if e['type_hash']==IMPACT and allowed(e,tags)]
 if len(impacts)!=1:raise ValueError('single observed melee event fixture required')
 at,duration=struct.unpack_from('<qq',bytes.fromhex(events[impacts[0]]['base_event']))
 for name,lo,hi,overlap,expected in [('before',0,at,False,[]),('start_inclusive',at,at+1,False,impacts),('end_exclusive',at-1,at,False,[]),('after_start',at+1,at+2,False,[]),('active_overlap',at+1,at+2,True,impacts),('end_touch',at+duration,at+duration+1,True,impacts),('expired',at+duration+1,at+duration+2,True,[]),('zero_range',at,at,True,[])]:
  test(name,events,lo,hi,tags,expected,type_hash=IMPACT,overlap=overlap)
 test('only_impact_preserves_type_filter',events,0,asset['native_length'],tags,impacts,type_hash=IMPACT)
 test('missing_required_tag',events,at,at+1,[ZERO]*8,[],type_hash=IMPACT)
 alternate=next(i for i,e in enumerate(events) if e['type_hash']==IMPACT and i not in impacts);alt=bytes.fromhex(events[alternate]['base_event'])[16:32]
 test('alternate_required_tag',events,at,at+1,[alt]+[ZERO]*7,[alternate],type_hash=IMPACT)
 test('unknown_filter',events,0,asset['native_length'],tags,[],type_hash=123)
 test('null_subsequence',events,0,asset['native_length'],tags,[],null=True)
 test('empty_subsequence',[],0,100,tags,[])
 # Exercise all four native exclusion GUID slots, including terminal zero.
 b=bytearray(96);struct.pack_into('<qq',b,0,10,20);tag=bytes.fromhex('1234567890abcdef1234567890abcdef');b[16:32]=tag
 for index in range(4):
  v=bytearray(b)
  for j in range(index):v[32+j*16:48+j*16]=bytes([j+1])*16
  v[32+index*16:48+index*16]=tag
  synthetic=[dict(type_hash=IMPACT,base_event=v.hex())]
  test('deny_slot_'+str(index),synthetic,10,11,[tag],[],type_hash=IMPACT)
 test('required_guid_exact', [dict(type_hash=IMPACT,base_event=b.hex())],10,11,[tag[:-1]+bytes([tag[-1]^1])],[],type_hash=IMPACT)
 selected,ok,unchanged=engine.select(events,0,asset['native_length'],tags,capacity=0)
 checks['bounded_capacity_failure']=not ok and selected==[] and unchanged
 out=dict(executable_sha256=SHA256,source=str(a.probe),checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},passed=sum(checks.values()),failed=sum(not v for v in checks.values()),cases=cases,limitations=__doc__+' Older live trace replay uses one observed constant tag set; this is evidence for this fixture only.')
 a.output.write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in checks.items():
  if not v:print('FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],'native event selection/condition checks');return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
