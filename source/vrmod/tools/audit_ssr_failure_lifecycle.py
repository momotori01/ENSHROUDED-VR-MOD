"""Offline native SSR lifecycle audit using poisoned CPU allocations.

Only allowlisted instructions execute in Unicorn. GPU calls are recording stubs.
This deliberately violates the observed allocator's tag-8 zero-fill contract.
It locates constructor responsibilities, not a reproduced real-game defect.
"""
import json
from pathlib import Path
import struct

from emulate_ssr_factory import Factory
from unicorn.x86_const import UC_X86_REG_RAX, UC_X86_REG_RDX


class PoisonedFactory(Factory):
    def __init__(self):
        super().__init__()
        self.cpu.mem_write(self.heap, b'\xa5' * 0x10000)
        self.allowed.append((0x153fe0, 0x1541d9))
        self.resolution_releases = []
        self.inspect_resolution = False

    def on_instruction(self, cpu, address, size, unused):
        if self.inspect_resolution and address-self.base in (0xf90de0, 0xf909b0):
            pointer = cpu.reg_read(UC_X86_REG_RDX)
            raw = bytes(cpu.mem_read(pointer, 16))
            self.resolution_releases.append(dict(offset=hex(pointer-self.allocated[0]), bytes=raw.hex()))
            # Record and clear; never dereference a poisoned graph pointer.
            cpu.mem_write(pointer, bytes(16))
            self.finish_stub()
            return
        super().on_instruction(cpu, address, size, unused)


def main():
    rows = []
    def check(name, actual):
        rows.append(dict(name=name, status='PASS' if actual else 'FAIL'))
    f = PoisonedFactory()
    config = f.data+0x1000
    f.cpu.mem_write(config, struct.pack('<5Q', 0xaaa000, 0xbbb000, 0xccc000, 0xddd000, 0xeee000))
    f.invoke(0x152ce0, f.data, config)
    obj = f.cpu.reg_read(UC_X86_REG_RAX)
    creation = bytes(f.cpu.mem_read(obj+0x460, 0x48))
    check('factory_returned_owned_object', obj == f.allocated[0])
    check('resolution_handle_48_is_not_initialized_by_factory', bytes(f.cpu.mem_read(obj+0x48, 16)) == b'\xa5'*16)
    check('resolution_mode_3d_is_not_initialized_by_factory', bytes(f.cpu.mem_read(obj+0x3d, 1)) == b'\xa5')
    f.inspect_resolution = True
    f.invoke(0x153fe0, obj)
    f.inspect_resolution = False
    check('premature_resolution_cleanup_consumes_19_uninitialized_handles',
          len(f.resolution_releases) == 19 and all(r['bytes'] == 'a5'*16 for r in f.resolution_releases))
    check('resolution_cleanup_does_not_release_creation_resources',
          bytes(f.cpu.mem_read(obj+0x460, 0x48)) == creation and not f.released)
    f.invoke(0x153f00, obj)
    check('final_creation_cleanup_frees_owned_CPU_allocation_once', f.freed == [obj])
    check('final_creation_cleanup_releases_4_creation_handles_once', len(f.released) == 4 and set(f.released) == set(f.resources))
    # A fresh factory object must instead skip resolution cleanup altogether.
    g = PoisonedFactory()
    g.cpu.mem_write(config, struct.pack('<5Q', 0xaaa000, 0xbbb000, 0xccc000, 0xddd000, 0xeee000))
    g.invoke(0x152ce0, g.data, config)
    fresh = g.cpu.reg_read(UC_X86_REG_RAX)
    g.invoke(0x153f00, fresh)
    check('creation_only_cleanup_succeeds_with_poisoned_resolution_storage',
          g.freed == [fresh] and len(g.released) == 4 and not g.resolution_releases)
    report = dict(scope=__doc__, checks=rows, premature_resolution_releases=f.resolution_releases,
                  passed=sum(r['status']=='PASS' for r in rows), failed=sum(r['status']=='FAIL' for r in rows),
                  interpretation='COUNTERFACTUAL: the actual Renderer allocator zeroes tag-8 allocations. '
                                 'Poisoned storage deliberately violates that contract; this proves only '
                                 'that the factory relies on allocator initialization. See '
                                 'emulated_allocator_zero_contract.json for native tag policy verification.')
    Path('docs/analysis/ssr_failure_lifecycle_audit.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    print('RESULT', report['passed'], 'PASS', report['failed'], 'FAIL')
    for row in rows:
        print(row['status'], row['name'])
    return bool(report['failed'])


if __name__ == '__main__':
    raise SystemExit(main())
