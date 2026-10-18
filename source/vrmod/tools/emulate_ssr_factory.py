"""Execute original SSR CPU factory/destructor with explicit allocator/GPU stubs.

This proves CPU object/creation ownership only, never real GPU or world safety.
"""
import json
from pathlib import Path
import struct
from emulate_camera_math import CameraMath
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8


class Factory(CameraMath):
    def __init__(self):
        super().__init__(Path('enshrouded.exe'))
        self.allowed=[(0x152ce0,0x152efe),(0x153f00,0x153fd7)]
        self.heap=0x6300000000
        self.cpu.mem_map(self.heap,0x10000)
        self.allocated=[]
        self.freed=[]
        self.resources={}
        self.released=[]
        self.next_handle=100
        self.stubs=set()
        self.cpu.mem_write(self.data,struct.pack('<Q',self.data+0x100))
        self.cpu.mem_write(self.data+0x108,struct.pack('<QQ',self.stop+0x100,self.stop+0x200))
        self.cpu.mem_write(self.base+0x1ef1a60,struct.pack('<Q',self.data+0x2000))

    def finish_stub(self,value=0):
        self.cpu.reg_write(UC_X86_REG_RAX,value)
        sp=self.cpu.reg_read(UC_X86_REG_RSP)
        self.cpu.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',self.cpu.mem_read(sp,8))[0])
        self.cpu.reg_write(UC_X86_REG_RSP,sp+8)

    def on_instruction(self,cpu,address,size,unused):
        rva=address-self.base
        if any(lo<=rva<hi for lo,hi in self.allowed):return
        rcx,rdx,r8=(cpu.reg_read(r) for r in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8))
        if address==self.stop+0x100:
            assert rcx==self.data and rdx==0x4b8 and r8==8
            allocation=self.heap+0x1000*len(self.allocated)
            assert allocation<self.heap+0x10000
            self.allocated.append(allocation)
            self.stubs.add('CPU allocator')
            self.finish_stub(allocation)
        elif address==self.stop+0x200:
            assert rcx==self.data and rdx in self.allocated and rdx not in self.freed
            self.freed.append(rdx)
            self.stubs.add('CPU free')
            self.finish_stub()
        elif rva in (0x9bdef0,0xdda210):
            self.next_handle+=1
            self.stubs.add('shader registration / sampler creation')
            self.finish_stub(self.next_handle)
        elif rva in (0xf8bc20,0xf8c320):
            self.next_handle+=1
            self.resources[self.next_handle]=dict(kind='buffer' if rva==0xf8bc20 else 'texture',destination=rdx,size=r8)
            cpu.mem_write(rdx,struct.pack('<QQ',0xabcdef00,self.next_handle))
            self.stubs.add('GPU creation blueprint')
            self.finish_stub(rdx)
        elif rva in (0xf90de0,0xf909b0):
            graph,handle=struct.unpack('<QQ',cpu.mem_read(rdx,16))
            assert graph==0xabcdef00 and handle in self.resources and handle not in self.released
            self.released.append(handle)
            cpu.mem_write(rdx,bytes(16))
            self.stubs.add('GPU creation resource release')
            self.finish_stub()
        elif rva in (0xde1f50,0x9bf260):
            self.stubs.add('sampler / shader registration release')
            self.finish_stub()
        else:raise ValueError(f'execution escaped factory allowlist: {address:x}')


def main():
    f=Factory()
    checks=[]
    def check(name,ok):checks.append(dict(name=name,status='PASS' if ok else 'FAIL'))
    config=f.data+0x1000
    values=(0xaaa000,0xbbb000,0xccc000,0xddd000,0xeee000)
    f.cpu.mem_write(config,struct.pack('<5Q',*values))
    objects=[]
    for i in range(4):
        f.invoke(0x152ce0,f.data,config)
        obj=f.cpu.reg_read(UC_X86_REG_RAX)
        objects.append(obj)
        raw=bytes(f.cpu.mem_read(obj,0x4b8))
        check(f'context_{i}_new_allocation',obj==f.allocated[i] and objects.count(obj)==1)
        check(f'context_{i}_borrowed_services',struct.unpack_from('<5Q',raw)==(f.data,values[0],values[1],values[3],values[2]))
        check(f'context_{i}_empty_history_vector',raw[0x448:0x460]==bytes(24))
        handles=[struct.unpack_from('<Q',raw,off+8)[0] for off in (0x460,0x470,0x480,0x490)]
        check(f'context_{i}_four_distinct_creation_resources',len(set(handles))==4 and all(h in f.resources for h in handles))
        check(f'context_{i}_source_config_unchanged',bytes(f.cpu.mem_read(config,40))==struct.pack('<5Q',*values))
    for obj in reversed(objects):f.invoke(0x153f00,obj)
    check('all_private_CPU_allocations_freed_once',f.freed==list(reversed(objects)))
    check('all_creation_blueprints_released_once',set(f.released)==set(f.resources) and len(f.released)==16)
    report=dict(checks=checks,substitutions=sorted(f.stubs),resources=f.resources,
        limitation=__doc__,pass_count=sum(c['status']=='PASS' for c in checks),fail_count=sum(c['status']=='FAIL' for c in checks))
    Path('docs/analysis/emulated_ssr_factory.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print('RESULT',report['pass_count'],'PASS',report['fail_count'],'FAIL')
    for c in checks:
        if c['status']=='FAIL':print(c)
    return bool(report['fail_count'])


if __name__=='__main__':raise SystemExit(main())
