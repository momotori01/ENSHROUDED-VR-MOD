"""Execute actual attribute lookup instructions on captured tables in Unicorn.
Both fe5fe0 and fe5f30 execute without substitutions. Data pointers are rebased
inside the isolated emulator; no native game call or OS operation is emulated.
"""
import argparse,copy,json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from read_attribute_registry import KEY,locate
from unicorn.x86_const import UC_X86_REG_RAX,UC_X86_REG_RBX,UC_X86_REG_RDI

class AttributeLookupCPU(CameraMath):
 def on_instruction(self,c,address,size,unused):
  rva=address-self.base
  if not (0xfe5f30<=rva<0xfe5fbf or 0xfe5fe0<=rva<0xfe601f):raise ValueError(f'lookup escaped {rva:x}')
 def table(self,capacity,bits,keys,values,count):
  if capacity>1024 or len(values)!=capacity*32:raise ValueError('bounded captured table required')
  d=self.data;self.cpu.mem_write(d,bytes(0x10000));self.capacity=capacity;self.bits=bits;self.keys=keys
  header=bytearray(88)
  for off,val in [(8,d+0x800),(16,capacity),(40,d+0x1000),(64,d+0x3000)]:struct.pack_into('<Q',header,off,val)
  struct.pack_into('<I',header,84,count);self.cpu.mem_write(d+0x68,bytes(header));self.cpu.mem_write(d+0x800,bits);self.cpu.mem_write(d+0x1000,keys);self.cpu.mem_write(d+0x3000,values)
 def query(self,key):
  c=self.cpu;d=self.data;out=d+0x200;c.mem_write(out,b'\xa5'*16);before=bytes(c.mem_read(d,0x10000))
  c.reg_write(UC_X86_REG_RBX,0xabcdef123456);c.reg_write(UC_X86_REG_RDI,0x123abcdef456)
  self.invoke(0xfe5fe0,out,key,d);after=bytes(c.mem_read(d,0x10000));expected=bytearray(before);bucket=locate(key,self.capacity,self.bits,self.keys)
  expected[0x200]=0x1f if bucket is None else 0
  struct.pack_into('<Q',expected,0x208,0 if bucket is None else d+0x3000+32*bucket)
  return after==bytes(expected) and c.reg_read(UC_X86_REG_RAX)==out and c.reg_read(UC_X86_REG_RBX)==0xabcdef123456 and c.reg_read(UC_X86_REG_RDI)==0x123abcdef456

def audit(doc):
 c={};cap=doc['capacity'];bits=bytes.fromhex(doc['bitmap']);keys=bytes.fromhex(doc['keys']);values=bytes.fromhex(doc['values']);header=bytes.fromhex(doc['header'])
 c['pinned_readonly_stable_table']=doc['exe_sha256']==SHA256 and doc['read_only'] and doc['table_stable'] and not doc['live_target_damage_proven']
 c['query_registry_matches_table']=struct.unpack_from('<Q',bytes.fromhex(doc['query_bytes']),24)[0]==int(doc['registry'],16) and int(doc['table'],16)==int(doc['registry'],16)+0x68
 occupied=[i for i in range(cap) if bits[i//8]>>(i%8)&1];entries=doc['entries']
 c['raw_count_capacity_and_widths']=doc['count']==len(entries)==len(occupied)==struct.unpack_from('<I',header,84)[0] and cap==struct.unpack_from('<Q',header,16)[0] and len(values)==cap*32 and len(keys)==cap*4
 c['all_raw_entries_match']=all(r['bucket'] in occupied and r['key']==struct.unpack_from('<I',keys,r['bucket']*4)[0]==r['metadata_key'] and r['bytes']==values[r['bucket']*32:(r['bucket']+1)*32].hex() and r['index']==struct.unpack_from('<H',bytes.fromhex(r['bytes']),12)[0] and r['root_key']==struct.unpack_from('<I',bytes.fromhex(r['bytes']))[0] and r['metadata_key']==struct.unpack_from('<I',bytes.fromhex(r['bytes']),4)[0] and bytes.fromhex(r['name_bytes']).decode('utf8')==r['name'] and len(bytes.fromhex(r['name_bytes']))==struct.unpack_from('<Q',bytes.fromhex(r['bytes']),24)[0] for r in entries) and {r['bucket'] for r in entries}==set(occupied)
 selected=doc['selected'];c['Health_identity_named']=selected in entries and doc['health_key']==KEY==selected['key']==selected['root_key'] and selected['name']=='Health' and 0<=selected['index']<8
 health=[r for r in entries if r['root_key']==KEY];c['Health_container_eight_unique_indices']=len(health)==8 and sorted(r['index'] for r in health)==list(range(8))
 c['raw_Health_snapshot_values']=bool(doc['health_snapshots']) and all(r['world']==doc['world'] and r['index']==selected['index'] and r['historical_snapshot'] and len(bytes.fromhex(r['health_bytes']))==68 and r['value']==struct.unpack_from('<i',bytes.fromhex(r['health_bytes']),36+4*r['index'])[0] for r in doc['health_snapshots'])
 return c

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('snapshot',type=pathlib.Path);a=p.parse_args();doc=json.loads(a.snapshot.read_text(encoding='utf-8-sig'));checks=audit(doc)
 e=AttributeLookupCPU(pathlib.Path('enshrouded.exe'));e.table(doc['capacity'],bytes.fromhex(doc['bitmap']),bytes.fromhex(doc['keys']),bytes.fromhex(doc['values']),doc['count'])
 for r in doc['entries']:checks['native_live_key_'+r['key_hex']]=e.query(r['key'])
 absent=range(0,32)
 for key in absent:checks['native_low_key_'+str(key)]=e.query(key)
 # Independent collision/wrap/empty fixtures, all accesses remain in emulator RAM.
 for cap in (1,2,8,64):
  bits=bytearray((cap+63)//64*8);keys=bytearray(cap*4);values=bytes(cap*32);placed=[]
  for i in range(min(3,cap-1)):
   key=cap-1+i*cap;at=key&(cap-1)
   while bits[at//8]>>(at%8)&1:at=(at+1)%cap
   bits[at//8]|=1<<(at%8);struct.pack_into('<I',keys,at*4,key);placed.append(key)
  e.table(cap,bytes(bits),bytes(keys),values,len(placed))
  for key in placed+[0,cap-1+3*cap,0xffffffff]:checks[f'native_wrap_{cap}_{key}']=e.query(key)
  e.table(cap,bytes(len(bits)),bytes(len(keys)),values,0);checks['native_empty_'+str(cap)]=e.query(0x8eb84995)
 for label,fn in [('name',lambda d:d['selected'].update(name='Stamina')),('index',lambda d:d['selected'].update(index=7)),('value',lambda d:d['health_snapshots'][0].update(value=-123456)),('query',lambda d:d.update(registry='0x0')),('count',lambda d:d.update(count=1)),('raw',lambda d:d['entries'][0].update(metadata_key=0)),('stable',lambda d:d.update(table_stable=False))]:
  bad=copy.deepcopy(doc);fn(bad);checks['synthetic_reject_'+label]=not all(audit(bad).values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},passed=sum(checks.values()),failed=sum(not v for v in checks.values()),substitutions=[],native_functions=['0xfe5fe0','0xfe5f30'],source=str(a.snapshot),live_records=doc['count'],health_index=doc['selected']['index'],health_names=[(r['index'],r['name']) for r in doc['entries'] if r['root_key']==KEY],live_target_damage_proven=False,limitation=__doc__)
 (a.snapshot.parent/'attribute-registry-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in checks.items():
  if not v:print('FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],'captured native attribute lookup; no live damage');return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
