"""Execute the local input-backend destructor in isolated Unicorn memory.
No game process, OS input, device driver or host DLL calls. Disconnect delivery,
device registration removal, FreeLibrary and allocator free are recorded stubs.
A backend page is unmapped at native free to detect any following stale read.
"""
import json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX,UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_RSP,UC_X86_REG_RIP

class Probe(CameraMath):
 def on_instruction(self,cpu,address,size,unused):
  rva=address-self.base
  if rva in (0xdad060,0xdac610) or address in (self.stop+0x100,self.stop+0x200):
   if rva in (0xdad060,0xdac610):
    self.events.append(dict(event='disconnect' if rva==0xdad060 else 'unregister',manager=cpu.reg_read(UC_X86_REG_RCX),device=cpu.reg_read(UC_X86_REG_RDX)))
   elif address==self.stop+0x100:
    self.events.append(dict(event='unload',module=cpu.reg_read(UC_X86_REG_RCX),api=list(struct.unpack('<3Q',cpu.mem_read(self.backend+0x10,24)))))
   else:
    if cpu.reg_read(UC_X86_REG_RCX)!=self.allocator or cpu.reg_read(UC_X86_REG_RDX)!=self.backend:raise ValueError('wrong native allocator owner')
    self.beforeFree=bytes(cpu.mem_read(self.backend,0x648));self.events.append(dict(event='free',backend=self.backend));cpu.mem_unmap(self.backend,0x1000);self.freed=True
   sp=cpu.reg_read(UC_X86_REG_RSP);cpu.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',cpu.mem_read(sp,8))[0]);cpu.reg_write(UC_X86_REG_RSP,sp+8);cpu.reg_write(UC_X86_REG_RAX,1);return
  super().on_instruction(cpu,address,size,unused)

def main():
 e=Probe(pathlib.Path('enshrouded.exe'));e.allowed=[(0xdac6c0,0xdaca58)];e.allocator=e.data;e.backend=e.data+0x1000;manager=e.data+0x3000
 e.cpu.mem_write(e.allocator,struct.pack('<Q',e.data+0x100));e.cpu.mem_write(e.data+0x110,struct.pack('<Q',e.stop+0x200));e.cpu.mem_write(e.base+0x12a7860,struct.pack('<Q',e.stop+0x100))
 checks={};runs=[]
 for mask in range(16):
  if mask:e.cpu.mem_map(e.backend,0x1000)
  data=bytearray(0x648);struct.pack_into('<Q',data,0,manager);struct.pack_into('<4Q',data,8,123,e.stop+0x300,e.stop+0x400,e.stop+0x500)
  for port in range(4):
   at=0x30+port*0x40;struct.pack_into('<QIIQ',data,at,manager,20+port,port,e.backend+8);data[at+0x20]=bool(mask&(1<<port))
  e.cpu.mem_write(e.backend,bytes(data));e.events=[];e.freed=False;e.beforeFree=None;e.invoke(0xdac6c0,e.allocator,e.backend)
  expected=[]
  for port in range(4):
   if mask&(1<<port):expected.append(dict(event='disconnect',manager=manager,device=20+port))
   expected.append(dict(event='unregister',manager=manager,device=20+port))
  expected.extend([dict(event='unload',module=123,api=[0,0,0]),dict(event='free',backend=e.backend)])
  checks[f'mask_{mask}_exact_native_disconnect_unregister_clear_unload_free_order']=e.events==expected
  checks[f'mask_{mask}_all_api_and_dll_fields_zero_before_free']=struct.unpack_from('<4Q',e.beforeFree,8)==(0,0,0,0)
  checks[f'mask_{mask}_all_native_port_owners_removed']=all(struct.unpack_from('<Q',e.beforeFree,0x30+p*0x40)[0]==0 and struct.unpack_from('<Q',e.beforeFree,0x48+p*0x40)[0]==0 for p in range(4))
  checks[f'mask_{mask}_returns_after_unmapping_without_further_backend_read']=e.freed and e.cpu.reg_read(UC_X86_REG_RIP)==e.stop
  runs.append(dict(connected_mask=mask,events=e.events))
 out=dict(exe_sha256=SHA256,scope=__doc__,checks=checks,runs=runs,not_proven=['native destructor thread/order relative to World and Scene','concurrent callback quiescence','real disconnect delivery effects','safe MOD-hook detach during real owner destruction'])
 pathlib.Path('docs/analysis/emulated_input_backend_exit.json').write_text(json.dumps(out,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(sum(checks.values()),'PASS /',sum(not x for x in checks.values()),'FAIL');return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
