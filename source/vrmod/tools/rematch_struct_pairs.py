# read-only: for old=new function pairs, align the instructions (difflib over the loose shape: mnemonic and operand
# kinds) and print every aligned pair whose memory displacement differs (structure offsets that moved), whatever the
# registers; rsp/rbp/rip are left out. --all prints every aligned memory access; --grep keeps the lines with a text.
# usage: rematch_struct_pairs.py old=new [old=new ...] [--all] [--grep +0x28]
import sys, pathlib, bisect, difflib, argparse
sys.path.insert(0, 'vrmod/tools/python')
import pefile, capstone
from capstone import x86


class Img:
    def __init__(self, path):
        self.raw = pathlib.Path(path).read_bytes(); self.pe = pefile.PE(data=self.raw, fast_load=True)
        self.pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
        fns = sorted(self.pe.DIRECTORY_ENTRY_EXCEPTION, key=lambda f: f.struct.BeginAddress)
        self.ranges = [(f.struct.BeginAddress, f.struct.EndAddress) for f in fns]; self.starts = [r[0] for r in self.ranges]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); self.md.detail = True

    def fn(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i >= 0 and self.ranges[i][0] <= rva < self.ranges[i][1]:
            lo, hi = self.ranges[i]
            while i + 1 < len(self.ranges) and self.ranges[i + 1][0] == hi: i += 1; hi = self.ranges[i][1]
            return lo, hi
        return rva, rva + 2048

    def insns(self, rva):
        lo, hi = self.fn(rva); out = []
        for insn in self.md.disasm(self.pe.get_data(lo, hi - lo), lo):
            shape = [insn.mnemonic]; mems = []
            for op in insn.operands:
                if op.type == x86.X86_OP_REG: shape.append('R')
                elif op.type == x86.X86_OP_IMM: shape.append('I')
                elif op.type == x86.X86_OP_MEM:
                    shape.append('M')
                    if op.mem.base not in (x86.X86_REG_RSP, x86.X86_REG_RBP, x86.X86_REG_RIP):
                        mems.append((insn.reg_name(op.mem.base) if op.mem.base else '', op.mem.disp))
                else: shape.append('?')
            out.append((insn.address, ' '.join(shape), f'{insn.mnemonic} {insn.op_str}', mems))
        return lo, hi, out


p = argparse.ArgumentParser(); p.add_argument('pairs', nargs='+'); p.add_argument('--all', action='store_true'); p.add_argument('--grep', action='append', default=[])
a = p.parse_args()
old = Img('vrmod/output/exe-backups/af2f5a12-20260630/enshrouded.exe'); new = Img('enshrouded.exe')
for pair in a.pairs:
    o, n = (int(x, 16) for x in pair.split('='))
    olo, ohi, oi = old.insns(o); nlo, nhi, ni = new.insns(n)
    print(f'== old {olo:x}..{ohi:x} ({len(oi)} insns)  new {nlo:x}..{nhi:x} ({len(ni)} insns)')
    sm = difflib.SequenceMatcher(None, [x[1] for x in oi], [x[1] for x in ni], autojunk=False)
    for tag, i1, i2, j1, j2 in sm.get_opcodes():
        if tag not in ('equal', 'replace') or (i2 - i1) != (j2 - j1):
            continue
        for i, j in zip(range(i1, i2), range(j1, j2)):
            x, y = oi[i], ni[j]
            if not x[3] and not y[3]: continue
            changed = [d for d in zip(x[3], y[3]) if d[0][1] != d[1][1]]
            if not a.all and not changed: continue
            line = f'  {x[0]:x}: {x[2]:<48} | {y[0]:x}: {y[2]}' + ('' if changed else '   (same)')
            if a.grep and not any(g in line for g in a.grep): continue
            print(line)
