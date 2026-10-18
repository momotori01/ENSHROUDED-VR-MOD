"""Execute the native collider cancellation branch in a bounded CPU sandbox.
Only ECS iteration, event lookup, and physics deletion are substituted. Actual
36ba40 control flow, collider handle clearing, and change-counter code execute.
No live process, game file, component, or save is written.
"""
import pathlib,json,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import *

class CancellationCPU(CameraMath):
 def on_instruction(self,cpu,address,size,unused):
  rva=address-self.base
  def ret(value=None):
   if value is not None:cpu.reg_write(UC_X86_REG_RAX,value)
   sp=cpu.reg_read(UC_X86_REG_RSP);pc=struct.unpack('<Q',cpu.mem_read(sp,8))[0];cpu.reg_write(UC_X86_REG_RSP,sp+8);cpu.reg_write(UC_X86_REG_RIP,pc)
  if rva==0x8da7c0:ret();return
  if rva==0x8d5ca0:
   self.iterations+=1
   if self.iterations==1:
    self.query=cpu.reg_read(UC_X86_REG_RDX);cpu.mem_write(self.query,struct.pack('<10Q',*self.slots));ret(1)
   else:
    self.query_after=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_RDX),80));ret(0)
   return
  if rva==0x1d9250:self.lookups+=1;ret(0 if self.null_lookup else self.resource);return
  if rva==0x8b71e0:
   self.deletions.append((cpu.reg_read(UC_X86_REG_RCX),cpu.reg_read(UC_X86_REG_RDX)));ret();return
  if not 0x36ba40<=rva<0x36bf10:raise ValueError(f'Unapproved instruction {rva:x}')

def main():
 engine=CancellationCPU(pathlib.Path('enshrouded.exe'));cpu=engine.cpu;d=engine.data
 checks={};cases=[]
 for null in (True,False):
  for handle in (0,123456789):
   for counter in (0,d+0x800):
    engine.slots=[d,d+0x100,0,0,d+0x200,d+0x300,counter,d+0x400,d+0x500,0]
    engine.resource=d+0x1000;engine.null_lookup=null;engine.iterations=engine.lookups=0;engine.deletions=[]
    cpu.mem_write(d,b'\0'*0x2000);cpu.mem_write(d+0x200,struct.pack('<IB3x16s',2684906120,0,bytes.fromhex('7d1d1b1a96d7e54e98b4fd0d583f6cac')));cpu.mem_write(d+0x308,struct.pack('<Q',handle))
    if counter:cpu.mem_write(counter,struct.pack('<I',19))
    # Non-null resource with empty shapes takes the same native removal path.
    before=bytes(cpu.mem_read(d,0x2000));engine.invoke(0x36ba40,d)
    after=bytes(cpu.mem_read(d,0x2000));expected=bytearray(before)
    if handle:
     struct.pack_into('<Q',expected,0x308,0)
     if counter:struct.pack_into('<I',expected,0x800,20)
    name=f'null{int(null)}_handle{int(bool(handle))}_counter{int(bool(counter))}'
    ok=after==bytes(expected) and engine.deletions==([(d+0x500,handle)] if handle else []) and engine.lookups==1 and engine.iterations==2 and engine.query_after==struct.pack('<10Q',*engine.slots)
    checks[name]='PASS' if ok else 'FAIL';cases.append(dict(case=name,deletions=engine.deletions,query_preserved=engine.query_after==struct.pack('<10Q',*engine.slots),other_memory_preserved=after==bytes(expected)))
    print(checks[name],name)
 out=dict(executable_sha256=SHA256,checks=checks,cases=cases,limitations=__doc__)
 pathlib.Path('docs/analysis/emulated_collider_cancel.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 print('PASS',sum(v=='PASS' for v in checks.values()),'FAIL',sum(v=='FAIL' for v in checks.values()));return any(v!='PASS' for v in checks.values())
if __name__=='__main__':raise SystemExit(main())
