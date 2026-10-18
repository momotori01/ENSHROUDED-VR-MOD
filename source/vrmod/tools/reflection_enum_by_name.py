"""enum_by_name.py <exe> <qualified name>: find the reflected enum by its name string (typedef name at +32/+40) and
print its values (kfc-parser layout as in vrmod/tools/reflection_enums.py)."""
import sys, struct, pathlib
sys.path.insert(0, r'S:/SteamLibrary/steamapps/common/Enshrouded/vrmod/tools/python')
import pefile
exe, wanted = sys.argv[1], sys.argv[2].encode()
raw = pathlib.Path(exe).read_bytes(); pe = pefile.PE(data=raw, fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
secs = [(s.VirtualAddress, s.VirtualAddress + s.SizeOfRawData, s.PointerToRawData) for s in pe.sections]
def off(rva):
    for lo, hi, p in secs:
        if lo <= rva < hi: return p + rva - lo
def rva_of(o):
    for lo, hi, p in secs:
        if p <= o < p + (hi - lo): return lo + o - p
def name(ptr, n):
    o = off(ptr - base); return raw[o:o + n].decode('utf8', 'replace')
pos = 0; hits = []
while True:
    i = raw.find(wanted + b'\0', pos)
    if i < 0: break
    if raw[i - 1] == 0 or True: hits.append(i)
    pos = i + 1
for h in hits:
    va = base + rva_of(h)
    pat = struct.pack('<QQ', va, len(wanted))
    j = 0
    while True:
        k = raw.find(pat, j)
        if k < 0: break
        j = k + 1
        rec = k - 32
        size, align, element, count, primitive = struct.unpack_from('<IHHIB', raw, rec + 64)
        if primitive not in (12, 13, 14, 15, 16) or count > 4096: continue
        start = off(struct.unpack_from('<Q', raw, rec + 96)[0] - base)
        print('typedef at rva', hex(rva_of(rec)), 'count', count, 'primitive', primitive)
        for n in range(count):
            p, l, v = struct.unpack_from('<QQQ', raw, start + n * 40)
            print('  ', v, name(p, l))
