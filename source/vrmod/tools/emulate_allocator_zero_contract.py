"""Run the observed allocator's native tag decisions against poisoned blocks.

TLSF allocation, maximum/allocation size and memset are explicit bounded stubs.
The real game allocator policy/branch instructions execute; no game is accessed.
"""
import json
from pathlib import Path
import struct

from emulate_camera_math import CameraMath, SHA256
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_R9, UC_X86_REG_RSP, UC_X86_REG_RIP


class Allocator(CameraMath):
    def __init__(self):
        super().__init__(Path('enshrouded.exe'))
        self.allowed = [(0x7e2e00, 0x7e2f73), (0x7e3be0, 0x7e3c40)]
        self.heap = 0x6500000000
        self.cpu.mem_map(self.heap, 0x10000)
        self.current_size = 0
        self.current_alignment = 0
        self.current_path = ''
        self.allocations = 0
        self.zero_calls = []

    def finish(self, value=0):
        self.cpu.reg_write(UC_X86_REG_RAX, value)
        sp = self.cpu.reg_read(UC_X86_REG_RSP)
        self.cpu.reg_write(UC_X86_REG_RIP, struct.unpack('<Q', self.cpu.mem_read(sp, 8))[0])
        self.cpu.reg_write(UC_X86_REG_RSP, sp+8)

    def on_instruction(self, cpu, address, size, unused):
        rva = address-self.base
        if any(lo <= rva < hi for lo, hi in self.allowed):
            return
        rcx, rdx, r8 = (cpu.reg_read(r) for r in (UC_X86_REG_RCX, UC_X86_REG_RDX, UC_X86_REG_R8))
        if rva == 0x402c80:
            self.finish(0x1000000)
        elif rva in (0x403520, 0x403570):
            requested = rdx if rva == 0x403520 else r8
            assert requested == self.current_size
            if rva == 0x403570:
                assert rdx == self.current_alignment
            self.allocations += 1
            self.finish(0 if self.current_path == 'grow' and self.allocations == 1 else self.heap)
        elif rva == 0x402c60:
            assert rcx == self.heap
            self.finish(self.current_size)
        elif rva == 0x7e3d00:
            assert self.current_path == 'grow'
            self.finish(self.data+0x8000)
        elif rva == 0x12a2560:
            assert rcx == self.heap and rdx == 0 and r8 == self.current_size
            self.zero_calls.append(dict(pointer=hex(rcx), size=r8))
            cpu.mem_write(rcx, bytes(r8))
            self.finish(rcx)
        else:
            raise ValueError(f'allocator escaped allowlist: {rva:x}')

    def run_case(self, path, size, alignment, tag):
        self.cpu.mem_write(self.data, bytes(0x10000))
        self.cpu.mem_write(self.heap, b'\xa5'*0x10000)
        self.current_size, self.current_alignment, self.current_path = size, alignment, path
        self.allocations, self.zero_calls = 0, []
        tag_address = self.data+0x1000
        self.cpu.mem_write(tag_address, bytes([tag]))
        # A synthetic TLSF allocator pointer; the low-level allocation is stubbed.
        self.cpu.mem_write(self.data, struct.pack('<Q', 0xabc000))
        if path == 'growth_helper':
            self.cpu.reg_write(UC_X86_REG_R9, tag_address)
            self.invoke(0x7e3be0, self.data, size, alignment)
            pointer = self.cpu.reg_read(UC_X86_REG_RAX)
        else:
            if path == 'grow':
                self.cpu.mem_write(self.data+0x80, struct.pack('<QQ', 0xdef000, 0x10000))
            self.cpu.reg_write(UC_X86_REG_R9, alignment)
            sp = self.stack+0xf008
            self.cpu.mem_write(sp+0x28, struct.pack('<Q', tag_address))
            self.cpu.mem_write(sp+0x30, bytes(8))
            self.invoke(0x7e2e00, self.data, self.data+0x2000, size)
            pointer = struct.unpack('<Q', self.cpu.mem_read(self.data+0x2000, 8))[0]
        expected = bytes(size) if tag & 8 else b'\xa5'*size
        return dict(path=path, size=size, alignment=alignment, tag=tag, pointer=hex(pointer),
                    allocations=self.allocations, zero_calls=self.zero_calls,
                    checks=dict(returned_owned_block=pointer == self.heap,
                                tag_controls_zero_fill=bytes(self.cpu.mem_read(self.heap, size)) == expected,
                                zero_call_count=len(self.zero_calls) == int(bool(tag & 8)),
                                does_not_write_past_requested_size=bytes(self.cpu.mem_read(self.heap+size, 32)) == b'\xa5'*32))


def main():
    fixture = Allocator()
    cases = [fixture.run_case(path, size, alignment, tag)
             for path in ('primary', 'grow', 'growth_helper')
             for size in (0x210, 0x4b8) for alignment in (8, 16) for tag in (0, 4, 8, 12)]
    passed = sum(value for c in cases for value in c['checks'].values())
    failed = sum(not value for c in cases for value in c['checks'].values())
    report = dict(executable_sha256=SHA256, scope=__doc__, cases=cases, passed=passed, failed=failed,
                  observed_vtable_rva='0x12a8ff8', observed_allocate_rva='0x7e3840',
                  conclusion='Tag bit 8 guarantees zero filling in the tested native primary/growth paths. '
                             'Factory poison tests that omit this contract are counterfactual.')
    Path('docs/analysis/emulated_allocator_zero_contract.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('RESULT', passed, 'PASS', failed, 'FAIL;', len(cases), 'tag/size/alignment/path cases')
    for case in cases:
        if not all(case['checks'].values()):
            print(case)
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
