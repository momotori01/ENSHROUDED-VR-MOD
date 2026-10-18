"""Run the local detail-view factory, array reserve and destructor offline.

Only PE instructions in an explicit allowlist run. CPU allocation, GPU blueprints
and byte copies are substituted. Native tag bit8 zeroes the allocation, including
when backing memory is poisoned. Deliberately broken-contract cases are marked
counterfactual. No game process or host GPU use.
Allocation failure is injected deliberately; it does not establish why a real
allocator failed in the recorded world probe.
"""
import json
from pathlib import Path
import struct
from emulate_camera_math import CameraMath, SHA256
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9


class UnsafePartialCleanup(ValueError):
    """The original destructor tried to release an uninitialized GPU handle."""


class DetailFactory(CameraMath):
    def __init__(self, fail_array=None, poison=False, honor_native_tag8=True):
        super().__init__(Path('enshrouded.exe'))
        self.allowed = [(0x47730,0x47b0f),(0x49370,0x49501),(0x4eac0,0x4ec46),
                        (0x7af760,0x7af785),(0x7a620,0x7a663),(0x7a670,0x7a71d),(0x7b560,0x7b605)]
        self.heap=0x6400000000
        self.cpu.mem_map(self.heap,0x100000)
        self.cursor=self.heap
        self.allocations={}
        self.freed=[]
        self.array_calls=[]
        self.fail_array=fail_array
        self.poison=poison
        self.honor_native_tag8=honor_native_tag8
        self.zeroed_objects=[]
        self.invalid_release=None
        self.resources={}
        self.released_resources=[]
        self.released_counters=[]
        self.next_handle=100
        self.service=self.data+0x1000
        self.context=self.data+0x2000
        self.counter_pool=self.data+0x3000
        self.counter_table=self.data+0x4000
        self.counter_free=self.data+0x8000
        self.cpu.mem_write(self.data,struct.pack('<Q',self.data+0x100))
        self.cpu.mem_write(self.data+0x108,struct.pack('<QQ',self.stop+0x100,self.stop+0x200))
        self.cpu.mem_write(self.service,struct.pack('<5Q',self.data,0,0xabc000,0,self.counter_pool))
        self.cpu.mem_write(self.context,struct.pack('<Q',self.data+0x2100))
        self.cpu.mem_write(self.counter_pool+0x48,struct.pack('<Q',self.counter_table))
        self.cpu.mem_write(self.counter_pool+0x58,struct.pack('<QQQ',self.counter_free,64,64))
        self.cpu.mem_write(self.counter_free,struct.pack('<64H',*range(1,65)))

    def finish_stub(self,value=0):
        self.cpu.reg_write(UC_X86_REG_RAX,value)
        sp=self.cpu.reg_read(UC_X86_REG_RSP)
        self.cpu.reg_write(UC_X86_REG_RIP,struct.unpack('<Q',self.cpu.mem_read(sp,8))[0])
        self.cpu.reg_write(UC_X86_REG_RSP,sp+8)

    def on_instruction(self,cpu,address,size,unused):
        rva=address-self.base
        if rva==0x7b560:
            rcx,rdx=(cpu.reg_read(r) for r in (UC_X86_REG_RCX,UC_X86_REG_RDX))
            assert rcx==self.counter_pool and rdx and rdx not in self.released_counters
            self.released_counters.append(rdx)
        if any(lo<=rva<hi for lo,hi in self.allowed): return
        rcx,rdx,r8,r9=(cpu.reg_read(r) for r in (UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9))
        if address==self.stop+0x100:
            assert rcx==self.data
            tag=cpu.mem_read(r9,1)[0]
            if tag==4:
                assert rdx in (0x5000,0x14000) and r8==4
                self.array_calls.append(rdx)
                if self.fail_array==len(self.array_calls):
                    self.finish_stub(); return
            else: assert tag==8 and rdx==0x210 and r8==8
            allocation=self.cursor
            self.cursor+=(rdx+0xff)&~0xff
            assert self.cursor<self.heap+0x100000
            self.allocations[allocation]=dict(size=rdx,tag=tag)
            if self.poison:cpu.mem_write(allocation,b'\xcd'*rdx)
            if self.honor_native_tag8 and tag&8:
                assert rdx==0x210
                cpu.mem_write(allocation,bytes(rdx))
                self.zeroed_objects.append(allocation)
            self.finish_stub(allocation)
        elif address==self.stop+0x200 or rva==0x7c41e0:
            assert rcx==self.data and rdx in self.allocations and rdx not in self.freed
            self.freed.append(rdx)
            self.finish_stub()
        elif rva==0x12a1ec0:
            if r8: cpu.mem_write(rcx,bytes(cpu.mem_read(rdx,r8)))
            self.finish_stub(rcx)
        elif rva==0xf8af30:
            cpu.mem_write(rcx,struct.pack('<3Q',rdx,0,0))
            self.finish_stub(rcx)
        elif rva==0xf8bc20:
            self.next_handle+=1
            self.resources[self.next_handle]=dict(destination=rdx,size=r8)
            cpu.mem_write(rdx,struct.pack('<QQ',0xabc000,self.next_handle))
            self.finish_stub(rdx)
        elif rva==0xf909b0:
            graph,handle=struct.unpack('<QQ',cpu.mem_read(rdx,16))
            if graph or handle:
                if not(graph==0xabc000 and handle in self.resources and handle not in self.released_resources):
                    self.invalid_release=dict(destination=hex(rdx),graph=hex(graph),handle=hex(handle))
                    raise UnsafePartialCleanup('original destructor releases an unknown GPU handle')
                self.released_resources.append(handle)
                cpu.mem_write(rdx,bytes(16))
            self.finish_stub()
        else: raise ValueError(f'execution escaped detail allowlist: {address:x}')


def main():
    checks=[]
    cases=[]
    def check(name,ok):checks.append(dict(name=name,status='PASS' if ok else 'FAIL'))
    for fail_at in (None,1,2,3,4):
        f=DetailFactory(fail_at)
        prefix=f'array_failure_{fail_at}' if fail_at else 'two_independent_views'
        service_before=bytes(f.cpu.mem_read(f.service,40))
        objects=[]
        for eye in range(2):
            f.invoke(0x47730,f.service,f.context)
            obj=f.cpu.reg_read(UC_X86_REG_RAX)
            objects.append(obj)
            should_fail=(fail_at in (1,2) and eye==0) or (fail_at in (3,4) and eye==1)
            check(f'{prefix}_eye_{eye}_return',bool(obj)!=should_fail)
            if obj:
                raw=bytes(f.cpu.mem_read(obj,0x210))
                check(f'{prefix}_eye_{eye}_capacities',struct.unpack_from('<Q',raw,0xd8)[0]==0x200 and struct.unpack_from('<Q',raw,0x108)[0]==0x800)
                check(f'{prefix}_eye_{eye}_self_and_service',struct.unpack_from('<Q',raw,0xa0)[0]==f.service and struct.unpack_from('<Q',raw,0xb0)[0]==obj)
                handles=[struct.unpack_from('<Q',raw,off+8)[0] for off in range(0x18,0x98,0x10)]
                check(f'{prefix}_eye_{eye}_8_distinct_blueprints',len(set(handles))==8 and all(h in f.resources for h in handles))
        for obj in reversed(objects):
            if obj:f.invoke(0x49370,f.service,obj)
        check(f'{prefix}_service_unchanged',bytes(f.cpu.mem_read(f.service,40))==service_before)
        check(f'{prefix}_all_cpu_allocations_freed_once',len(f.freed)==len(f.allocations) and set(f.freed)==set(f.allocations))
        check(f'{prefix}_all_gpu_blueprints_released_once',len(f.released_resources)==len(f.resources) and set(f.released_resources)==set(f.resources))
        check(f'{prefix}_all_six_counters_released',len(f.released_counters)==6 and len(set(f.released_counters))==6)
        cases.append(dict(name=prefix,returned=[hex(x) for x in objects],array_allocation_sizes=f.array_calls,
                          cpu_allocations=len(f.allocations),released_counters=f.released_counters))
    poison_cases=[]
    for fail_at in (None,1,2):
        f=DetailFactory(fail_at,poison=True,honor_native_tag8=False)
        hazard=None
        objects=[]
        try:
            for eye in range(2):
                f.invoke(0x47730,f.service,f.context)
                objects.append(f.cpu.reg_read(UC_X86_REG_RAX))
            for obj in reversed(objects):
                if obj:f.invoke(0x49370,f.service,obj)
        except UnsafePartialCleanup as error:hazard=str(error)
        prefix=f'counterfactual_broken_tag8_array_failure_{fail_at}' if fail_at else 'counterfactual_broken_tag8_two_successful_views'
        if fail_at:
            check(prefix+'_uninitialized_cleanup_hazard_detected',hazard is not None and
                  f.invalid_release['graph']=='0xcdcdcdcdcdcdcdcd' and f.invalid_release['handle']=='0xcdcdcdcdcdcdcdcd')
            check(prefix+'_failure_before_first_gpu_blueprint',not f.resources)
        else:
            check(prefix+'_two_nonnull_objects',len(objects)==2 and all(objects) and not hazard)
            check(prefix+'_all_cpu_allocations_freed_once',len(f.freed)==len(f.allocations) and set(f.freed)==set(f.allocations))
            check(prefix+'_all_gpu_blueprints_released_once',len(f.released_resources)==16)
        poison_cases.append(dict(name=prefix,status='COUNTERFACTUAL_UNSAFE_PARTIAL_CLEANUP' if hazard else 'COUNTERFACTUAL_SAFE_IN_CPU_FIXTURE',
                                 hazard=hazard,invalid_release=f.invalid_release,returned=[hex(x) for x in objects]))
    facade_cases=[]
    for fail_at in (None,1,2,3,4):
        f=DetailFactory(fail_at,poison=True,honor_native_tag8=True)
        objects=[]
        for eye in range(2):
            f.invoke(0x47730,f.service,f.context)
            objects.append(f.cpu.reg_read(UC_X86_REG_RAX))
        for obj in reversed(objects):
            if obj:f.invoke(0x49370,f.service,obj)
        prefix=f'native_tag8_poison_backing_array_failure_{fail_at}' if fail_at else 'native_tag8_poison_backing_success'
        check(prefix+'_two_owned_bodies_zeroed',len(f.zeroed_objects)==2)
        check(prefix+'_expected_return_pattern',sum(bool(o) for o in objects)==(1 if fail_at else 2))
        check(prefix+'_all_cpu_allocations_freed_once',len(f.freed)==len(f.allocations) and set(f.freed)==set(f.allocations))
        check(prefix+'_all_gpu_blueprints_released_once',len(f.released_resources)==len(f.resources) and set(f.released_resources)==set(f.resources))
        check(prefix+'_counter_free_count_restored',struct.unpack('<Q',f.cpu.mem_read(f.counter_pool+0x60,8))[0]==64)
        facade_cases.append(dict(name=prefix,returned=[hex(o) for o in objects],
                                 zeroed_objects=[hex(o) for o in f.zeroed_objects],invalid_release=f.invalid_release))
    report=dict(exe_sha256=SHA256,checks=checks,cases=cases,limitation=__doc__,
                poison_cases=poison_cases,
                facade_cases=facade_cases,
                pass_count=sum(c['status']=='PASS' for c in checks),fail_count=sum(c['status']=='FAIL' for c in checks))
    Path('docs/analysis/emulated_detail_factory.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print('RESULT',report['pass_count'],'PASS',report['fail_count'],'FAIL')
    for c in checks:
        if c['status']=='FAIL':print(c)
    return bool(report['fail_count'])


if __name__=='__main__':raise SystemExit(main())
