"""callers.py <exe> <target rva hex>...: the E8 call sites (rva of the call) of each target in .text."""
import sys, struct, pathlib
sys.path.insert(0, r'S:/SteamLibrary/steamapps/common/Enshrouded/vrmod/tools/python')
import pefile
pe = pefile.PE(sys.argv[1], fast_load=True)
for s in pe.sections:
    if s.Name.rstrip(b'\0') == b'.text':
        data = s.get_data(); va = s.VirtualAddress
for t in sys.argv[2:]:
    target = int(t, 16); out = []
    i = data.find(b'\xe8')
    while i >= 0:
        if i + 5 <= len(data):
            rel = struct.unpack_from('<i', data, i + 1)[0]
            if va + i + 5 + rel == target: out.append(hex(va + i))
        i = data.find(b'\xe8', i + 1)
    print(t, out[:20])
