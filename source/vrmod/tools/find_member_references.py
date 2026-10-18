"""Read-only member-displacement references, validated from PE unwind boundaries.

Offsets do not establish object types. Use each containing function's dataflow.
"""
import argparse
import bisect
import hashlib
import json
from pathlib import Path
import struct
import sys
sys.path.insert(0, str(Path(__file__).parent/'python'))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_REG_RIP


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--offsets', nargs='+', type=lambda x:int(x,0), required=True)
    p.add_argument('--output', type=Path, required=True)
    a = p.parse_args()
    raw = Path('enshrouded.exe').read_bytes()
    pe = pefile.PE(data=raw)
    functions = sorted((f.struct.BeginAddress,f.struct.EndAddress) for f in pe.DIRECTORY_ENTRY_EXCEPTION)
    starts = [lo for lo,hi in functions]
    candidates = set()
    for section in pe.sections:
        if not section.Characteristics & 0x20000000:
            continue
        code = section.get_data()
        for offset in a.offsets:
            if offset < 128:
                raise ValueError('This bounded scanner requires disp32 offsets >=128')
            needle = struct.pack('<i',offset)
            at = code.find(needle)
            while at >= 0:
                rva = section.VirtualAddress + at
                index = bisect.bisect_right(starts,rva)-1
                if index >= 0 and rva < functions[index][1]:
                    candidates.add(functions[index])
                at = code.find(needle,at+1)
    md = Cs(CS_ARCH_X86,CS_MODE_64)
    md.detail = True
    results = []
    for lo,hi in sorted(candidates):
        for instruction in md.disasm(pe.get_data(lo,hi-lo),lo):
            for operand in instruction.operands:
                if operand.type == X86_OP_MEM and operand.mem.base != X86_REG_RIP and operand.mem.disp in a.offsets:
                    results.append(dict(rva=hex(instruction.address),fragment=hex(lo),offset=hex(operand.mem.disp),
                        instruction=instruction.mnemonic+' '+instruction.op_str,bytes=instruction.bytes.hex(' '),access=operand.access))
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text(json.dumps(dict(exe_sha256=hashlib.sha256(raw).hexdigest(),references=results,
        limitation='Member displacement only; no object-type inference. Leaf functions without unwind metadata omitted.'),indent=2),encoding='utf8')
    for row in results:
        print(row['rva'],row['fragment'],row['instruction'])


if __name__ == '__main__':main()
