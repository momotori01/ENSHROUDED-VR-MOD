"""reflection_type_by_hash.py <exe> <hex hash> [...]: the reflected type whose type hash is <hash> (the hash the game
data keeps next to an event or a resource), by the layout of a known type: the name (pointer, length) at +32/+40 of the
typedef and the hash found by looking at a known pair (RootMotionWarpEvent, d3080aa1). Read-only.
reflection_type_by_hash.py <exe> --name <qualified name> [...]: the other way, the hash and size of a named type."""
import sys, struct, pathlib
sys.path.insert(0, r'S:/SteamLibrary/steamapps/common/Enshrouded/vrmod/tools/python')
import pefile
exe = sys.argv[1]; raw = pathlib.Path(exe).read_bytes(); pe = pefile.PE(data=raw, fast_load=True); base = pe.OPTIONAL_HEADER.ImageBase
secs = [(s.VirtualAddress, s.VirtualAddress + s.SizeOfRawData, s.PointerToRawData) for s in pe.sections]
def off(rva):
    for lo, hi, p in secs:
        if lo <= rva < hi: return p + rva - lo
def rva_of(o):
    for lo, hi, p in secs:
        if p <= o < p + (hi - lo): return lo + o - p
def records(name):
    w = name.encode(); out = []; pos = 0
    while True:
        i = raw.find(w + b'\0', pos)
        if i < 0: break
        pos = i + 1
        if raw[i - 1] not in (0, 58): continue
        va = base + rva_of(i); pat = struct.pack('<QQ', va, len(w)); j = 0
        while True:
            k = raw.find(pat, j)
            if k < 0: break
            j = k + 1; out.append(k - 32)
    return out
known = None
for rec in records('RootMotionWarpEvent'):
    j = raw[rec - 64:rec + 192].find(struct.pack('<I', 0xd3080aa1))
    if j >= 0: known = j - 64; break
if known is None: sys.exit('the known pair was not found')
print('hash at typedef +%d' % known)
if len(sys.argv)>2 and sys.argv[2]=='--name':
    for name in sys.argv[3:]:
        short=name.split('::')[-1];out=[]
        for rec in records(short):
            q=raw[rec+32:rec+48];qa,ql=struct.unpack('<QQ',q)
            h=struct.unpack_from('<I',raw,rec+known)[0];size=struct.unpack_from('<I',raw,rec+64)[0]
            out.append('%08x size %d'%(h,size))
        print(name,sorted(set(out)))
    sys.exit(0)
for h in sys.argv[2:]:
    v = int(h, 16); b = struct.pack('<I', v); p = raw.find(b); found = []
    while p >= 0:
        rec = p - known
        try:
            q, l = struct.unpack_from('<QQ', raw, rec + 32)
            o = off(q - base) if base < q < base + 0x20000000 else None
            if o and 0 < l < 200:
                s = raw[o:o + l]
                if s.isascii(): found.append(s.decode())
        except Exception: pass
        p = raw.find(b, p + 1)
    print(h, found)
