"""Check the original external-image wrapper loop in a bounded CPU emulator.

No game process, allocator, Vulkan call or game lifetime function is executed.
Two poison initializations distinguish assigned fields from unspecified bytes.
This is layout evidence, not an enabled game image import implementation.
"""
import argparse
import json
import pathlib
import struct
import xml.etree.ElementTree as ET
from emulate_camera_math import CameraMath, SHA256
from unicorn.x86_const import (UC_X86_REG_RAX, UC_X86_REG_RBX, UC_X86_REG_RDI,
                               UC_X86_REG_RSI, UC_X86_REG_R13, UC_X86_REG_RIP)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('docs/analysis/emulated_texture_wrapper.json'))
    args = p.parse_args()
    engine = CameraMath(pathlib.Path('enshrouded.exe'))
    engine.allowed.extend([(0xdca967, 0xdca9f7), (0xdeebb0, 0xdef18e)])
    registry = ET.parse('references/Vulkan-Headers/registry/vk.xml')
    names = {int(e.get('value')): e.get('name') for e in registry.findall("./enums[@name='VkFormat']/enum") if e.get('value')}
    formats = []
    for code in range(0x93):
        engine.invoke(0xdeebb0, code)
        value = engine.cpu.reg_read(UC_X86_REG_RAX)
        formats.append(dict(game_code=code, vulkan_format=value, name=names.get(value, str(value))))
    rows = []
    def check(name, actual, expected):
        rows.append(dict(name=name, status='PASS' if actual == expected else 'FAIL', actual=actual, expected=expected))
        print(rows[-1]['status'], name)
    wrapper, records = engine.data+0x1000, engine.data+0x2000
    image_array, view_array = engine.data+0x3000, engine.data+0x4000
    cases = []
    # Include formats used by OpenXRClient and asymmetric recommended extents.
    for vk_format in [37, 43, 44, 50, 97]:
        candidates = [f['game_code'] for f in formats if f['vulkan_format'] == vk_format]
        if len(candidates) != 1: raise ValueError(f'unsupported/ambiguous game format {vk_format}')
        code = candidates[0]
        for count in (1, 3):
            images = [0x1111222233330000+i*16 for i in range(count)]
            views = [0x4444555566660000+i*16 for i in range(count)]
            outputs = []
            for poison in (0x5a, 0xa5):
                obj = bytearray(0x80)
                struct.pack_into('<II', obj, 4, 2017, 2113)
                obj[0xc], obj[0x14] = code, 0x0b  # game transfer-src/dst + color usage
                struct.pack_into('<Q', obj, 0x38, image_array)
                struct.pack_into('<Q', obj, 0x50, view_array)
                struct.pack_into('<Q', obj, 0x70, records)
                engine.cpu.mem_write(wrapper, bytes(obj))
                engine.cpu.mem_write(image_array, struct.pack('<'+'Q'*count, *images))
                engine.cpu.mem_write(view_array, struct.pack('<'+'Q'*count, *views))
                engine.cpu.mem_write(records-16, bytes([poison])*(0x50*count+32))
                for reg, value in [(UC_X86_REG_RBX, wrapper), (UC_X86_REG_RDI, count),
                                   (UC_X86_REG_RSI, 0), (UC_X86_REG_R13, 1)]:
                    engine.cpu.reg_write(reg, value)
                engine.cpu.emu_start(engine.base+0xdca967, engine.base+0xdca9f7, timeout=100000, count=500)
                if engine.cpu.reg_read(UC_X86_REG_RIP) != engine.base+0xdca9f7:
                    raise ValueError('wrapper loop escaped its expected boundary')
                raw = bytes(engine.cpu.mem_read(records, 0x50*count))
                outputs.append(raw)
                check(f'format{vk_format}_{count}_{poison}_guard_before', list(engine.cpu.mem_read(records-16, 16)), [poison]*16)
                check(f'format{vk_format}_{count}_{poison}_guard_after', list(engine.cpu.mem_read(records+0x50*count, 16)), [poison]*16)
                for i in range(count):
                    record = raw[0x50*i:0x50*(i+1)]
                    actual = [struct.unpack_from('<I', record)[0], *struct.unpack_from('<III', record, 0x10),
                              struct.unpack_from('<H', record, 0x1e)[0], *record[0x20:0x23],
                              *record[0x24:0x27], *struct.unpack_from('<QQ', record, 0x30)]
                    expected = [0x6ffff, 2017, 2113, 1, 1, 1, code, 1, 1, 0x0b, 0x0b, images[i], views[i]]
                    check(f'format{vk_format}_{count}_{poison}_record{i}', actual, expected)
            assigned = [i for i in range(0x50) if outputs[0][i] == outputs[1][i]]
            cases.append(dict(vulkan_format=vk_format, game_code=code, count=count,
                              assigned_offsets=assigned, unspecified_offsets=[i for i in range(0x50) if i not in assigned]))
    report = dict(executable_sha256=SHA256, scope=__doc__, formats=formats, cases=cases, checks=rows,
                  passed=sum(r['status']=='PASS' for r in rows), failed=sum(r['status']=='FAIL' for r in rows))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf8')
    print('RESULT', report['passed'], 'PASS', report['failed'], 'FAIL')
    return int(report['failed'] != 0)


if __name__ == '__main__':
    raise SystemExit(main())
