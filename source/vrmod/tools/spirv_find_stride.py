"""spv_find.py <dir> <stride> [max bytes]: compute shaders that decorate an ArrayStride of <stride>; prints size, sha,
strides and the number of storage-buffer bindings."""
import sys, struct, pathlib, hashlib
d, stride = pathlib.Path(sys.argv[1]), int(sys.argv[2]); limit = int(sys.argv[3]) if len(sys.argv) > 3 else 1 << 30
for f in sorted(d.glob('shader_*.spv')):
    b = f.read_bytes()
    if len(b) > limit or len(b) < 20 or struct.unpack_from('<I', b, 0)[0] != 0x07230203: continue
    w = struct.unpack_from('<%dI' % (len(b) // 4), b); i = 5; comp = False; strides = []; bindings = 0
    while i < len(w):
        op = w[i] & 0xffff; n = w[i] >> 16
        if n == 0: break
        if op == 15 and w[i + 1] == 5: comp = True
        if op == 71 and n >= 4 and w[i + 2] == 6: strides.append(w[i + 3])
        if op == 71 and n >= 4 and w[i + 2] == 33: bindings += 1
        i += n
    if comp and stride in strides:
        print(f.name, len(b), hashlib.sha256(b).hexdigest()[:16], sorted(set(strides)), 'bindings', bindings)
