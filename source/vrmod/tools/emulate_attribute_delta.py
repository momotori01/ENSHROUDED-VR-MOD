"""Execute the SHA-pinned native attribute delta routine in isolated Unicorn.
Registry lookups and expression evaluation are explicitly stubbed; the original
addition, gates, dependent-expression loop and assignments execute unchanged.
Synthetic values prove ABI/memory effects only, never a live target's damage.
"""
import json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import *

class AttributeCPU(CameraMath):
 def ret(self):
  c=self.cpu;sp=c.reg_read(UC_X86_REG_RSP)
  c.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',c.mem_read(sp,8))[0]);c.reg_write(UC_X86_REG_RSP,sp+8)
 def on_instruction(self,c,address,size,unused):
  rva=address-self.base;d=self.data
  if rva==0xfe5fe0:
   out=c.reg_read(UC_X86_REG_RCX)
   assert c.reg_read(UC_X86_REG_RDX)&0xffffffff==self.key
   assert c.reg_read(UC_X86_REG_R8)==d+0x2000
   c.mem_write(out,struct.pack('<B7xQ',int(self.missing),d+0x2800));self.lookup+=1;self.ret();return
  if rva==0xfe6020:
   out=c.reg_read(UC_X86_REG_RCX)
   assert c.reg_read(UC_X86_REG_RDX)&0xffffffff==0x12345678
   assert c.reg_read(UC_X86_REG_R8)==d+0x2000
   c.mem_write(out,struct.pack('<B7xQ',int(self.root_missing),d+0x3000));self.root_lookup+=1;self.ret();return
  if rva==0x1e1b00:
   out=c.reg_read(UC_X86_REG_RCX);array=c.reg_read(UC_X86_REG_RDX);expr=c.reg_read(UC_X86_REG_R8)
   assert c.reg_read(UC_X86_REG_R9)==0
   assert struct.unpack('<QI',c.mem_read(array,12))==(d+0x1000,8)
   exprptr,count=struct.unpack('<QQ',c.mem_read(expr,16));assert count==1
   assert exprptr in (d+0x3200,d+0x3210)
   idx=1 if exprptr==d+0x3200 else 2
   self.evaluated.append(idx);self.eval_arrays.append(list(struct.unpack('<8i',c.mem_read(d+0x1000,32))))
   c.mem_write(out,struct.pack('<B3xi',int(idx==2),self.derived_value));self.ret();return
  if not 0x364730<=rva<0x364881:raise ValueError(f'Unexpected attribute call {rva:x}')

def main():
 e=AttributeCPU(pathlib.Path('enshrouded.exe'));c=e.cpu;d=e.data;checks={};cases=[]
 nonvolatile=[UC_X86_REG_RBX,UC_X86_REG_RBP,UC_X86_REG_RSI,UC_X86_REG_RDI,UC_X86_REG_R14]
 # Last case intentionally demonstrates the original routine trusts its caller's
 # registry index: no live invocation or host memory is used for that case.
 fixtures=[]
 for mode in ['active','inactive','null','missing','root_missing','derived']:
  for index in [0,1,7]:
   for before,delta in [(100,-17),(100,17),(0,0),(1,-10),(-2147483648,-1),(2147483647,1)]:
    fixtures.append((mode,index,before,delta))
 fixtures.append(('caller_index_precondition',31,100,-17))
 for mode,index,value,delta in fixtures:
  c.mem_write(d,bytes(0x8000));container=d+0x100;e.key=0x8eb84995;e.missing=mode=='missing';e.root_missing=mode=='root_missing';e.lookup=e.root_lookup=0;e.evaluated=[];e.eval_arrays=[];e.derived_value=2468
  c.mem_write(container,struct.pack('<IIII B3x',0x12345678,0,0xf00,8,int(mode!='inactive')))
  c.mem_write(d+0x2800+12,struct.pack('<H',index));c.mem_write(d+0x3000+12,struct.pack('<iI',0xf4,3 if mode=='derived' else 0))
  for i,expr in [(1,d+0x3200),(2,d+0x3210)]:
   field=d+0x3100+i*16+8;c.mem_write(field,struct.pack('<iI',expr-field,1))
  c.mem_write(d+0x1000,struct.pack('<32i',*range(500,532)));c.mem_write(d+0x1000+index*4,struct.pack('<i',value))
  before=bytes(c.mem_read(d,0x8000));expected=bytearray(before);active=mode not in ['inactive','null','missing']
  if active:struct.pack_into('<I',expected,0x1000+index*4,(value+delta)&0xffffffff)
  after_delta=struct.unpack_from('<8i',expected,0x1000)
  if mode=='derived':struct.pack_into('<i',expected,0x1000+4,e.derived_value)
  for i,reg in enumerate(nonvolatile):c.reg_write(reg,0x123400000000+i)
  c.reg_write(UC_X86_REG_R9,e.key);e.invoke(0x364730,0 if mode=='null' else container,d+0x2000,delta&0xffffffff)
  got=bytes(c.mem_read(d,0x8000));registers=all(c.reg_read(reg)==0x123400000000+i for i,reg in enumerate(nonvolatile))
  exact=got==bytes(expected);lookup=e.lookup==1 and e.root_lookup==int(active)
  deps=e.evaluated==([2,1] if mode=='derived' else []) and (not e.evaluated or all(a==list(after_delta) for a in e.eval_arrays))
  name=f'{mode}_index{index}_value{value}_delta{delta}';checks[name]=exact and lookup and deps and registers
  cases.append(dict(name=name,exact_memory=exact,nonvolatile_registers=registers,lookup_contract=lookup,dependent_loop=deps,expected_active=active,requested_delta=delta,index=index,declared_count=8,before=value,after=struct.unpack('<i',c.mem_read(d+0x1000+index*4,4))[0]))
 out=dict(executable_sha256=SHA256,function_rva='0x364730',checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},passed=sum(checks.values()),failed=sum(not v for v in checks.values()),cases=cases,substitutions=['fe5fe0: attribute metadata lookup','fe6020: dependent attribute root lookup','1e1b00: explicit success/error expression result'],limitations=__doc__,caller_contract='Native code does not bound the registry index by container count. A read-only observer must validate index<count and safe relative storage before dereferencing; the out-of-range example is sandbox-only.')
 pathlib.Path('docs/analysis/emulated_attribute_delta.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in checks.items():
  if not v:print('FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],'native attribute delta (synthetic, no live damage)');return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
