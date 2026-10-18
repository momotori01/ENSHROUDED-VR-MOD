"""Locate code references to selected reflected rendering types (read-only)."""
import argparse,bisect,json,pathlib,re,struct,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--types',nargs='+',help='Exact qualified reflection names')
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('docs/analysis/render_metadata_xrefs.json'))
    parser.add_argument('--asm-directory',type=pathlib.Path,default=pathlib.Path('docs/analysis/render_functions'))
    args=parser.parse_args()
    raw=pathlib.Path('enshrouded.exe').read_bytes();pe=pefile.PE(data=raw,fast_load=True)
    pe.parse_data_directories(directories=[3]);base=pe.OPTIONAL_HEADER.ImageBase
    types=json.loads(pathlib.Path('docs/analysis/reflection.json').read_text())
    wanted={'keen::'+n for n in ['RenderGBufferConstants','RenderTransparentConstants','DeferredLightingConstants','TemporalAntiAliasingShaderParameters','ShaderChunkletCullingSubView','GpuCulling2SubViewConstants','CameraModifierConfig']}
    if args.types:wanted=set(args.types)
    selected={int(t['metadata_rva'],16):t['qualified_name'] for t in types if t['qualified_name'] in wanted}
    funcs=sorted((f.struct.BeginAddress,f.struct.EndAddress) for f in pe.DIRECTORY_ENTRY_EXCEPTION)
    starts=[s for s,e in funcs];md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True;records=[];dumped=set()
    out=args.asm_directory;out.mkdir(parents=True,exist_ok=True)
    for sec in pe.sections:
        if not sec.Characteristics&0x20000000:continue
        code=sec.get_data()
        for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d][\s\S]{4}',code):
            rva=sec.VirtualAddress+m.start();dst=rva+7+struct.unpack_from('<i',code,m.start()+3)[0]
            if dst not in selected:continue
            i=bisect.bisect_right(starts,rva)-1
            if i<0:continue
            lo,hi=funcs[i]
            if not lo<=rva<hi:continue
            records.append({'type':selected[dst],'reference_rva':hex(rva),'function_start':hex(lo),'function_end':hex(hi)})
            if lo in dumped:continue
            dumped.add(lo);off=pe.get_offset_from_rva(lo);lines=[]
            for ins in md.disasm(raw[off:off+min(hi-lo,65536)],lo):
                extra=''
                for op in ins.operands:
                    if op.type==3 and md.reg_name(op.mem.base)=='rip':
                        adr=ins.address+ins.size+op.mem.disp
                        if adr in selected:extra=' ; '+selected[adr]
                lines.append(f'{ins.address:08x}: {ins.mnemonic} {ins.op_str}{extra}')
            (out/(f'{lo:08x}.asm')).write_text('\n'.join(lines))
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(records,indent=2))
    for r in records:print(r['type'],r['reference_rva'],'function',r['function_start'],r['function_end'])

if __name__=='__main__':main()
