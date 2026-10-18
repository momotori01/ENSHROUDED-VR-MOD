"""Execute native TLSF init/allocate/free on independently supplied backing.

No process access or host syscalls. Native name copying and memset are bounded
stubs; allocator policy, free-list and range/accounting instructions are real.
The 1 GiB case maps only used boundary pages, not a GiB host allocation.
"""
import json
import struct
from pathlib import Path
from emulate_camera_math import CameraMath, SHA256
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP

class Arena(CameraMath):
    def __init__(self, capacity):
        super().__init__(Path('enshrouded.exe'))
        self.allowed=[(0x7e4a00,0x7e4c02),(0x402030,0x403f00),(0x7e3840,0x7e387a),
                      (0x7e2e00,0x7e31bf),(0x7e64d0,0x7e64f3),(0x7e5e70,0x7e6018),(0x7df330,0x7df358)]
        self.region=0x6500000000;self.capacity=capacity
        self.cpu.mem_map(self.region,0x200000)
        if capacity>0x200000:self.cpu.mem_map(self.region+capacity-0x10000,0x10000)
        self.name_calls=0;self.failures=[]

    def finish(self,value=0):
        self.cpu.reg_write(UC_X86_REG_RAX,value)
        sp=self.cpu.reg_read(UC_X86_REG_RSP)
        self.cpu.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',self.cpu.mem_read(sp,8))[0])
        self.cpu.reg_write(UC_X86_REG_RSP,sp+8)

    def on_instruction(self,cpu,address,size,unused):
        rva=address-self.base
        if any(lo<=rva<hi for lo,hi in self.allowed):return
        rcx,rdx,r8=(cpu.reg_read(r)for r in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8))
        if rva==0x25ef40:
            name,length=struct.unpack('<QQ',cpu.mem_read(rdx,16))
            assert bytes(cpu.mem_read(name,length))==b'Game_Render' and length==11
            cpu.mem_write(rcx,b'Game_Render\0');self.name_calls+=1;self.finish(rcx)
        elif rva==0x12a2560:
            assert r8<=0x200000
            cpu.mem_write(rcx,bytes([rdx&255])*r8);self.finish(rcx)
        else:raise ValueError(f'allocator escaped allowlist: {rva:x}')

    def run(self):
        self.cpu.mem_write(self.data+0x1000,struct.pack('<QQ',self.region,self.capacity))
        self.cpu.mem_write(self.data+0x2000,b'Game_Render')
        self.cpu.mem_write(self.data+0x2100,struct.pack('<QQ',self.data+0x2000,11))
        self.cpu.reg_write(UC_X86_REG_R9,self.data+0x2100)
        self.invoke(0x7e4bd0,self.data,self.data+0x1000,self.data+0x3000)
        actual=struct.unpack('<QQ',self.cpu.mem_read(self.data+0x78,16))
        checks={'native_initializer_uses_supplied_region':actual==(self.region,self.capacity),'native_name_copy':self.name_calls==1}
        allocations=[]
        for eye in range(2):
            for size,tag in ((528,8),(20480,4),(81920,4)):
                self.cpu.mem_write(self.data+0x3000,bytes([tag]))
                self.cpu.mem_write(self.stack+0xf008+0x28,bytes(8))
                self.cpu.reg_write(UC_X86_REG_R9,self.data+0x3000)
                self.invoke(0x7e3840,self.data,size,8 if tag==8 else 4)
                pointer=self.cpu.reg_read(UC_X86_REG_RAX)
                assert pointer and self.region<=pointer<=self.region+self.capacity-size
                if tag==8:assert bytes(self.cpu.mem_read(pointer,size))==bytes(size)
                allocations.append((pointer,size))
        checks['two_eye_detail_allocations_owned']=len(allocations)==6 and len({p for p,s in allocations})==6
        before=struct.unpack('<Q',self.cpu.mem_read(self.data+0xa0,8))[0]
        checks['native_live_accounting']=before>=sum(s for p,s in allocations)
        for pointer,size in reversed(allocations):
            self.cpu.mem_write(self.data+0x3000,b'\0')
            self.invoke(0x7e64d0,self.data,pointer,self.data+0x3000)
        checks['native_free_restores_live_bytes']=struct.unpack('<Q',self.cpu.mem_read(self.data+0xa0,8))[0]==0
        checks['native_free_restores_allocation_count']=struct.unpack('<I',self.cpu.mem_read(self.data+0xb0,4))[0]==0
        # Recoverable failure must remain NULL with no fallback/assertion.
        self.cpu.mem_write(self.data+0x3000,b'\4');self.cpu.reg_write(UC_X86_REG_R9,self.data+0x3000)
        self.cpu.mem_write(self.stack+0xf008+0x28,bytes(8))
        self.invoke(0x7e3840,self.data,self.capacity+4096,4)
        checks['oversized_tag4_allocation_returns_null']=self.cpu.reg_read(UC_X86_REG_RAX)==0
        return dict(capacity=self.capacity,checks=checks)

def main():
    cases=[Arena(capacity).run()for capacity in (2*1024*1024,1024*1024*1024)]
    passed=sum(v for c in cases for v in c['checks'].values());failed=sum(not v for c in cases for v in c['checks'].values())
    Path('docs/analysis/emulated_render_cpu_arena.json').write_text(json.dumps(dict(scope=__doc__,sha256=SHA256,cases=cases,passed=passed,failed=failed),indent=2),encoding='utf8')
    print('RESULT',passed,'PASS',failed,'FAIL;',len(cases),'native backing sizes')
    return bool(failed)

if __name__=='__main__':raise SystemExit(main())
