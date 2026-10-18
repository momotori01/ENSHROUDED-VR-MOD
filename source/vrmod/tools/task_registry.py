"""Read-only investigation of task-name references and nearby function pointers."""
import argparse,json,pathlib,re,struct,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile
data=pathlib.Path('enshrouded.exe').read_bytes();pe=pefile.PE(data=data);base=pe.OPTIONAL_HEADER.ImageBase
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--names',nargs='+',default=['player_camera_control','player_camera_transform','actor_camera','actor_camera_shake','g_camera_world_position'])
p.add_argument('--functions',nargs='+',type=lambda v:int(v,0),help='Resolve task descriptors whose code pointer matches these RVAs')
p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('docs/analysis/task_registry.json'))
args=p.parse_args()
targets=[]
if args.functions:
    # Task descriptor layout: name pointer, name length, function pointer.
    # A code pointer alone is not proof of a task; validate its preceding name.
    for function in args.functions:
        for match in re.finditer(re.escape(struct.pack('<Q',base+function)),data):
            at=match.start()
            if at<16:continue
            pointer,length=struct.unpack_from('<QQ',data,at-16)
            if not (base<=pointer<base+pe.OPTIONAL_HEADER.SizeOfImage and 1<=length<=160):continue
            try:name=pe.get_data(pointer-base,length+1)
            except pefile.PEFormatError:continue
            if len(name)!=length+1 or name[-1]!=0 or not re.fullmatch(rb'[a-z][a-z0-9_]+',name[:-1]):continue
            targets.append(dict(rva=hex(pointer-base),text=name[:-1].decode('ascii'),function_rva=hex(function),descriptor_rva=hex(pe.get_rva_from_offset(at-16))))
for name in ([] if args.functions else args.names):
    for match in re.finditer(re.escape(name.encode('ascii'))+b'\0',data):
        if match.start() and data[match.start()-1]!=0:continue
        targets.append(dict(rva=hex(pe.get_rva_from_offset(match.start())),text=name))
def describe(value):
    if not base<=value<base+pe.OPTIONAL_HEADER.SizeOfImage:return hex(value)
    rva=value-base
    for sec in pe.sections:
        if sec.VirtualAddress<=rva<sec.VirtualAddress+sec.SizeOfRawData:
            if sec.Characteristics&0x20000000:return 'CODE '+hex(rva)
            off=pe.get_offset_from_rva(rva);s=data[off:off+120].split(b'\0')[0]
            if len(s)>3 and all(32<=x<127 for x in s):return 'TEXT '+repr(s.decode())
    return 'DATA '+hex(rva)
out=[]
for target in targets:
    ptr=base+int(target['rva'],16)
    refs=[]
    for match in re.finditer(re.escape(struct.pack('<Q',ptr)),data):
        o=match.start();rows=[]
        for p in range(max(0,o-96),min(len(data)-8,o+144),8):
            value=struct.unpack_from('<Q',data,p)[0]
            rows.append({'rva':hex(pe.get_rva_from_offset(p)),'value':describe(value)})
        refs.append({'pointer_rva':hex(pe.get_rva_from_offset(o)),'nearby':rows})
    out.append({'target':target,'references':refs})
args.output.parent.mkdir(parents=True,exist_ok=True)
args.output.write_text(json.dumps(out,indent=2))
for t in out:
    print(t['target']['text'],len(t['references']),'references')
    for ref in t['references']:
        print(ref['pointer_rva'],[(r['rva'],r['value']) for r in ref['nearby'] if r['value'].startswith(('CODE','TEXT'))])
