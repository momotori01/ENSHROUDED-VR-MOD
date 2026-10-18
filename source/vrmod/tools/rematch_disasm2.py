# read-only: disassemble a function (by the unwind table, or N bytes) of an executable, resolving rip-relative strings
# usage: disasm2.py <exe> <rva> [max_insns] [--raw <bytes>]
import sys, pathlib, bisect
sys.path.insert(0, 'vrmod/tools/python')
import pefile, capstone
from capstone import x86
exe, rva = sys.argv[1], int(sys.argv[2], 16)
limit = int(sys.argv[3]) if len(sys.argv) > 3 and not sys.argv[3].startswith('--') else 10 ** 9
raw = pathlib.Path(exe).read_bytes(); pe = pefile.PE(data=raw, fast_load=True)
pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
fns = sorted(pe.DIRECTORY_ENTRY_EXCEPTION, key=lambda f: f.struct.BeginAddress); starts = [f.struct.BeginAddress for f in fns]
i = bisect.bisect_right(starts, rva) - 1
if i >= 0 and fns[i].struct.BeginAddress <= rva < fns[i].struct.EndAddress:
    lo, hi = fns[i].struct.BeginAddress, fns[i].struct.EndAddress
    while i + 1 < len(fns) and fns[i + 1].struct.BeginAddress == hi:
        i += 1; hi = fns[i].struct.EndAddress
else:
    lo = rva; hi = starts[bisect.bisect_right(starts, rva)] if bisect.bisect_right(starts, rva) < len(starts) else rva + 1024
    hi = min(hi, lo + 2048)
sections = [(s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData), s.Name.decode().strip('\x00')) for s in pe.sections]
def sec(r):
    for a, b, n in sections:
        if a <= r < b: return n
    return None
def string_at(r):
    if sec(r) not in ('.rdata', '.data', '_RDATA'): return None
    b = pe.get_data(r, 120); n = 0
    while n < len(b) and 32 <= b[n] < 127: n += 1
    if n >= 4: return b[:n].decode()
    n = 0
    while n + 1 < len(b) and 32 <= b[n] < 127 and b[n + 1] == 0: n += 2
    if n >= 8: return 'L' + b[:n].decode('utf-16-le')
    return None
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64); md.detail = True
print(f'{exe}: function {lo:x}..{hi:x} ({hi - lo} bytes)')
n = 0
for insn in md.disasm(pe.get_data(lo, hi - lo), lo):
    note = ''
    for op in insn.operands:
        if op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP:
            t = (insn.address + insn.size + op.mem.disp) & 0xffffffff
            s = string_at(t); note += f'  ; {t:x}' + (f' "{s}"' if s else f' ({sec(t)})')
    print(f'{insn.address:x}: {insn.mnemonic} {insn.op_str}{note}')
    n += 1
    if n >= limit: print('...'); break
