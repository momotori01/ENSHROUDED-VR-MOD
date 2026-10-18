"""Find displacement operands at validated instruction boundaries in local PE ranges.

Offsets alone do not identify a structure. Reports are candidates for manual
dataflow review; leaf functions absent from unwind metadata are not recovered.
"""
import argparse
import bisect
import hashlib
import json
import pathlib
import struct
import sys
sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_AC_WRITE
from capstone.x86 import X86_OP_MEM


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--offsets', nargs='+', required=True, type=lambda s: int(s, 0))
    p.add_argument('--begin', type=lambda s: int(s, 0), default=0)
    p.add_argument('--end', type=lambda s: int(s, 0), default=2**32)
    p.add_argument('--writes', action='store_true')
    p.add_argument('--output', type=pathlib.Path, required=True)
    args = p.parse_args()
    raw = pathlib.Path('enshrouded.exe').read_bytes()
    pe = pefile.PE(data=raw)
    functions = sorted(pe.DIRECTORY_ENTRY_EXCEPTION, key=lambda f: f.struct.BeginAddress)
    starts = [f.struct.BeginAddress for f in functions]
    by_start = dict(zip(starts, functions))
    def owner(f):
        visited = set()
        while f.unwindinfo.Flags & 4:
            start = f.struct.BeginAddress
            if start in visited: raise ValueError('cyclic unwind chain')
            visited.add(start)
            offset = f.struct.UnwindData + 4 + 2*((f.unwindinfo.CountOfCodes + 1) & ~1)
            begin, end, unwind = struct.unpack('<III', pe.get_data(offset, 12))
            f = by_start[begin]
            if (f.struct.EndAddress, f.struct.UnwindData) != (end, unwind): raise ValueError('invalid unwind chain')
        return f.struct.BeginAddress
    candidates = set()
    for section in pe.sections:
        if not section.Characteristics & 0x20000000: continue
        data = section.get_data()
        for offset in args.offsets:
            # x86 also encodes signed displacements in one byte. The old dword
            # prefilter missed most +0x30/+0x38 accesses. Decode still validates
            # every candidate's actual operand and instruction boundary below.
            needles = [struct.pack('<i', offset)]
            if -128 <= offset <= 127: needles.append(struct.pack('<b', offset))
            for needle in needles:
                at = data.find(needle)
                while at >= 0:
                    site = section.VirtualAddress + at
                    index = bisect.bisect_right(starts, site) - 1
                    if args.begin <= site < args.end and index >= 0 and site < functions[index].struct.EndAddress:
                        candidates.add(index)
                    at = data.find(needle, at + 1)
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    md.detail = True
    rows = []
    for index in sorted(candidates):
        f = functions[index]
        begin, end = f.struct.BeginAddress, f.struct.EndAddress
        for ins in md.disasm(pe.get_data(begin, end-begin), begin):
            if not args.begin <= ins.address < args.end: continue
            for operand in ins.operands:
                if operand.type != X86_OP_MEM or operand.mem.disp not in args.offsets: continue
                if args.writes and not operand.access & CS_AC_WRITE: continue
                rows.append(dict(rva=hex(ins.address), owner=hex(owner(f)), fragment=hex(begin),
                                 instruction=ins.mnemonic + ' ' + ins.op_str, displacement=hex(operand.mem.disp),
                                 writes=bool(operand.access & CS_AC_WRITE)))
    report = dict(executable_sha256=hashlib.sha256(raw).hexdigest(), scope=__doc__,
                  range=[hex(args.begin), hex(args.end)], offsets=[hex(x) for x in args.offsets], results=rows)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf8')
    for row in rows: print(row['rva'], row['owner'], row['instruction'])
    print('Candidates:', len(rows), 'Results:', args.output)


if __name__ == '__main__':
    main()
