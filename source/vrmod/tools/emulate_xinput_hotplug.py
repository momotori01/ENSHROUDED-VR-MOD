"""Run local native hotplug event20 and XInput polling in isolated CPU memory.
No OS input, process access, native DLL calls, or system device installation.
"""
import json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX,UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_RSP,UC_X86_REG_RIP
class Probe(CameraMath):
 def on_instruction(self,cpu,address,size,unused):
  rva=address-self.base
  if address==self.stop+0x100 or rva in (0xdaa9e0,0xdad060,0xdb5a80):
   if address==self.stop+0x100:
    user=cpu.reg_read(UC_X86_REG_RCX);self.gets.append(user);result=0 if user==self.connected else 1167
    if result==0:cpu.mem_write(cpu.reg_read(UC_X86_REG_RDX),struct.pack('<I12x',1))
   elif rva==0xdb5a80: self.other_polls+=1;result=0
   else:self.events.append(('connected' if rva==0xdaa9e0 else 'disconnected',cpu.reg_read(UC_X86_REG_RDX)));result=0
   sp=cpu.reg_read(UC_X86_REG_RSP);cpu.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',cpu.mem_read(sp,8))[0]);cpu.reg_write(UC_X86_REG_RSP,sp+8);cpu.reg_write(UC_X86_REG_RAX,result);return
  super().on_instruction(cpu,address,size,unused)
def main():
 e=Probe(pathlib.Path('enshrouded.exe'));e.allowed=[(0xdb23a0,0xdb2ad4),(0xdaa860,0xdaa8f3),(0xdb5b50,0xdb5c8a)]
 e.gets=[];e.events=[];e.other_polls=0;e.connected=-1;checks={}
 manager=e.data;backend=manager+0x400;event=manager+0x900;out=manager+0xa00
 memory=bytearray(0x900);struct.pack_into('<Q',memory,0x48,backend);struct.pack_into('<Q',memory,0x400,manager);struct.pack_into('<Q',memory,0x410,e.stop+0x100)
 for port in range(4):
  at=0x430+port*0x40;struct.pack_into('<Q',memory,at,manager);struct.pack_into('<I',memory,at+8,20+port);struct.pack_into('<I',memory,at+0x10,port);struct.pack_into('<Q',memory,at+0x18,backend+8)
 for kind in (0,6,20,255):
  e.cpu.mem_write(manager,bytes(memory));ev=bytes([kind])+bytes(63);e.cpu.mem_write(event,ev);e.invoke(0xdb23a0,manager,event)
  wanted=bytearray(memory);wanted[0x428]=int(kind==20)
  checks[f'event_{kind}_only_pending_rescan_changes']=bytes(e.cpu.mem_read(manager,len(memory)))==bytes(wanted) and bytes(e.cpu.mem_read(event,64))==ev and (e.cpu.reg_read(UC_X86_REG_RAX)&255)==int(kind==20)
 e.cpu.mem_write(manager,bytes(memory));e.invoke(0xdaa860,backend);checks['unrequested_disconnected_backend_never_polls']=not e.gets and not e.events
 e.connected=0;e.cpu.mem_write(event,bytes([20])+bytes(63));e.invoke(0xdb23a0,manager,event);e.invoke(0xdaa860,backend)
 checks['requested_scan_polls_four_native_ports']=e.gets==[0,1,2,3] and e.other_polls==4
 checks['native_connect_and_pending_flag_consumption']=e.events==[('connected',20)] and bytes(e.cpu.mem_read(backend+0x28,1))==b'\0' and bytes(e.cpu.mem_read(backend+0x50,1))==b'\1'
 e.invoke(0xdb5b50,backend+0x30,out,1);checks['connected_device_polled_on_normal_frame']=e.gets==[0,1,2,3,0] and len(e.events)==1 and struct.unpack('<I',e.cpu.mem_read(out,4))[0]==1
 e.connected=-1;e.invoke(0xdb5b50,backend+0x30,out,1);checks['native_disconnect_event_and_connected_flag']=e.events==[('connected',20),('disconnected',20)] and bytes(e.cpu.mem_read(backend+0x50,1))==b'\0'
 count=len(e.gets);e.invoke(0xdb5b50,backend+0x30,out,1);checks['disconnected_device_returns_to_unpolled_state']=len(e.gets)==count
 result=dict(exe_sha256=SHA256,scope=__doc__,checks=checks,events=e.events,get_state_ports=e.gets,external_calls='GetState and event delivery are stubbed; native state machine instructions execute unchanged')
 pathlib.Path('docs/analysis/emulated_xinput_hotplug.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 return not all(checks.values())
if __name__=='__main__':raise SystemExit(main())
