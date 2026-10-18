"""Find validated RIP-relative LEA references inside PE executable sections.
Read-only candidates, not approved runtime hook points.
"""
import bisect,json,pathlib,re,struct,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
data=pathlib.Path('enshrouded.exe').read_bytes();pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
strings=json.loads(pathlib.Path('docs/analysis/interesting_strings.json').read_text())
selected={int(x['rva'],16):x['text'] for x in strings if x['text'] in ['player_camera_transform','player_camera_control','Invalid camera projection type %d','[camera] Too many concurrent transitions. Dropping oldest one.','LocalPlayerCameraTransform','LocalPlayerCamera','actor_camera','g_camera_world_position']}
funcs=sorted((f.struct.BeginAddress,f.struct.EndAddress) for f in pe.DIRECTORY_ENTRY_EXCEPTION)
starts=[s for s,e in funcs];md=Cs(CS_ARCH_X86,CS_MODE_64);md.skipdata=True;results=[]
for sec in pe.sections:
    if not sec.Characteristics&0x20000000:continue
    raw=sec.get_data()
    for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d][\s\S]{4}',raw):
        rva=sec.VirtualAddress+m.start();dst=rva+7+struct.unpack_from('<i',raw,m.start()+3)[0]
        if dst not in selected:continue
        index=bisect.bisect_right(starts,rva)-1
        if index<0:continue
        lo,hi=funcs[index]
        if not lo<=rva<hi:continue
        off=pe.get_offset_from_rva(lo)
        dis=[{'rva':hex(i.address-base),'instruction':i.mnemonic+' '+i.op_str,'bytes':i.bytes.hex(' ')} for i in md.disasm(data[off:off+min(hi-lo,16384)],base+lo)]
        results.append({'reference_rva':hex(rva),'target_rva':hex(dst),'string':selected[dst],'function_start':hex(lo),'function_end':hex(hi),'instructions':dis})
pathlib.Path('docs/analysis/camera_xrefs.json').write_text(json.dumps(results,indent=2))
for r in results:print(r['string'],r['reference_rva'],'function',r['function_start'],r['function_end'])
