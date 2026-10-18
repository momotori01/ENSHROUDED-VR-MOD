# read-only: for old->new function pairs, print size, strings, callees count and the first instructions of both
import sys, pathlib, bisect
sys.path.insert(0, 'vrmod/tools/python')
import pefile, capstone
from capstone import x86
class Img:
    def __init__(self, path):
        self.raw = pathlib.Path(path).read_bytes(); self.pe = pefile.PE(data=self.raw, fast_load=True)
        self.pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
        fns = sorted(self.pe.DIRECTORY_ENTRY_EXCEPTION, key=lambda f: f.struct.BeginAddress)
        self.ranges = [(f.struct.BeginAddress, f.struct.EndAddress) for f in fns]; self.starts = [r[0] for r in self.ranges]
        self.sections = [(s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData), s.Name.decode().strip('\x00')) for s in self.pe.sections]
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); self.md.detail = True
    def sec(self, r):
        for a, b, n in self.sections:
            if a <= r < b: return n
    def fn(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i >= 0 and self.ranges[i][0] <= rva < self.ranges[i][1]:
            lo, hi = self.ranges[i]
            while i + 1 < len(self.ranges) and self.ranges[i + 1][0] == hi: i += 1; hi = self.ranges[i][1]
            return lo, hi
        j = bisect.bisect_right(self.starts, rva); hi = self.starts[j] if j < len(self.starts) else rva + 1024
        return rva, min(hi, rva + 2048)
    def string_at(self, r):
        if self.sec(r) not in ('.rdata', '.data', '_RDATA'): return None
        b = self.pe.get_data(r, 100); n = 0
        while n < len(b) and 32 <= b[n] < 127: n += 1
        if n >= 4: return b[:n].decode()
        n = 0
        while n + 1 < len(b) and 32 <= b[n] < 127 and b[n + 1] == 0: n += 2
        if n >= 8: return 'L' + b[:n].decode('utf-16-le')
    def describe(self, rva, head=5):
        lo, hi = self.fn(rva); code = self.pe.get_data(lo, hi - lo)
        insns = list(self.md.disasm(code, lo)); strings = []; calls = 0; heads = []
        for k, insn in enumerate(insns):
            if insn.mnemonic == 'int3': insns = insns[:k]; break
            if insn.group(capstone.CS_GRP_CALL): calls += 1
            for op in insn.operands:
                if op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                    s = self.string_at((insn.address + insn.size + op.mem.disp) & 0xffffffff)
                    if s and s not in strings: strings.append(s)
            if k < head: heads.append(f'{insn.mnemonic} {insn.op_str}')
        return dict(lo=lo, hi=hi, n=len(insns), calls=calls, strings=strings[:6], heads=heads)
old, new = Img(sys.argv[1]), Img(sys.argv[2])
for pair in sys.argv[3:]:
    o, n = (int(x, 16) for x in pair.split('='))
    a, b = old.describe(o), new.describe(n)
    print(f'== {o:x} -> {n:x}')
    for tag, d in (('old', a), ('new', b)):
        print(f'  {tag} {d["lo"]:x}..{d["hi"]:x} insns {d["n"]} calls {d["calls"]} strings {d["strings"]}')
        print(f'      ' + ' | '.join(d['heads']))
