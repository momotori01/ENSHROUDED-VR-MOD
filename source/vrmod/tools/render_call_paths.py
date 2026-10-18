"""Find direct call sites and chained-unwind function owners in the local game PE.
Candidate call graphs only: indirect calls and runtime dispatch are not recovered.
"""
import bisect
import argparse
import hashlib
import json
import pathlib
import struct
import sys

sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_64
from capstone.x86 import X86_OP_MEM, X86_OP_IMM, X86_REG_RIP


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--roots',nargs='+',type=lambda x:int(x,0))
    parser.add_argument('--sites',nargs='+',type=lambda x:int(x,0),default=[],help='Resolve inside-function RVAs through chained unwind ownership')
    parser.add_argument('--depth',type=int,choices=range(0,5),default=3)
    parser.add_argument('--output',type=pathlib.Path,default=pathlib.Path('docs/analysis/render_call_paths'))
    args=parser.parse_args()
    raw=pathlib.Path('enshrouded.exe').read_bytes()
    pe=pefile.PE(data=raw)
    functions=sorted(pe.DIRECTORY_ENTRY_EXCEPTION,key=lambda f:f.struct.BeginAddress)
    starts=[f.struct.BeginAddress for f in functions]
    by_start=dict(zip(starts,functions))
    owners={}
    def owner(f):
        start=f.struct.BeginAddress
        if start in owners:return owners[start]
        visited=set()
        while f.unwindinfo.Flags&4:
            current=f.struct.BeginAddress
            if current in visited:raise ValueError('cyclic chained unwind info')
            visited.add(current)
            count=f.unwindinfo.CountOfCodes
            offset=f.struct.UnwindData+4+2*((count+1)&~1)
            begin,end,unwind=struct.unpack('<III',pe.get_data(offset,12))
            f=by_start[begin]
            if (f.struct.EndAddress,f.struct.UnwindData)!=(end,unwind):raise ValueError('unwind chain mismatch')
        owners[start]=f.struct.BeginAddress
        return owners[start]
    for f in functions:owner(f)
    if args.roots is None:args.roots=[] if args.sites else [0xe4a50,0xd65b60,0xd7a0c0,0xd7ce70,0x2662a0]
    resolved_sites=[]
    for site in args.sites:
        index=bisect.bisect_right(starts,site)-1
        if index<0 or site>=functions[index].struct.EndAddress:raise ValueError(f'site outside known unwind range: {site:x}')
        root=owner(functions[index])
        resolved_sites.append(dict(site=hex(site),fragment=hex(functions[index].struct.BeginAddress),owner=hex(root)))
        if root not in args.roots:args.roots.append(root)
    md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True
    cache={}
    def disassemble(f):
        lo,hi=f.struct.BeginAddress,f.struct.EndAddress
        if lo not in cache:
            cache[lo]=list(md.disasm(pe.get_data(lo,hi-lo),lo))
        return cache[lo]
    all_calls=[]
    for sec in pe.sections:
        if not sec.Characteristics&0x20000000:continue
        data=sec.get_data();at=data.find(b'\xe8')
        while at>=0:
            if at+5<=len(data):
                site=sec.VirtualAddress+at
                target=site+5+struct.unpack_from('<i',data,at+1)[0]
                if target in by_start or target in args.roots or target in (0xd61400,0x141b30):
                    all_calls.append((site,target))
            at=data.find(b'\xe8',at+1)
    selected=set(args.roots)
    frontier=set(selected);edges=[];seen=set()
    for depth in range(args.depth):
        next_frontier=set()
        for site,target in all_calls:
            if target not in frontier or (site,target) in seen:continue
            index=bisect.bisect_right(starts,site)-1
            if index<0:continue
            f=functions[index]
            if not site<f.struct.EndAddress:continue
            instructions=disassemble(f)
            if not any(i.address==site and i.mnemonic=='call' for i in instructions):continue
            parent=owner(f)
            edges.append(dict(site=hex(site),target=hex(target),owner=hex(parent),fragment=hex(f.struct.BeginAddress),depth=depth))
            seen.add((site,target));next_frontier.add(parent);selected.add(parent)
        frontier=next_frontier
    out=args.output;out.mkdir(parents=True,exist_ok=True)
    reports=[]
    for start in sorted(selected):
        fragments=[f for f in functions if owner(f)==start]
        lines=[];strings=[];vector_instructions=[];calls=[]
        leaf_instructions=[];leaf_termination=None;leaf_call_site=None
        if not fragments:
            # Leaf functions need no unwind entry. Only inspect an explicit root
            # reached by an instruction-boundary-validated direct call. Never
            # silently emit an empty function for an unsupported arbitrary RVA.
            for site,target in all_calls:
                if target!=start:continue
                index=bisect.bisect_right(starts,site)-1
                if index<0 or site>=functions[index].struct.EndAddress:continue
                if any(i.address==site and i.mnemonic=='call' for i in disassemble(functions[index])):
                    leaf_call_site=hex(site);break
            if leaf_call_site is None:
                raise ValueError(f'no unwind range or validated direct caller for {start:x}')
            containing=bisect.bisect_right(starts,start)-1
            if containing>=0 and start<functions[containing].struct.EndAddress:
                raise ValueError(f'root {start:x} lies inside another unwind function')
            section=next((s for s in pe.sections if s.Characteristics&0x20000000 and
                          s.VirtualAddress<=start<s.VirtualAddress+s.Misc_VirtualSize),None)
            if section is None:raise ValueError(f'leaf {start:x} is outside executable sections')
            next_start=starts[containing+1] if containing+1<len(starts) else pe.OPTIONAL_HEADER.SizeOfImage
            end=min(start+4096,next_start,section.VirtualAddress+section.Misc_VirtualSize)
            for insn in md.disasm(pe.get_data(start,end-start),start):
                leaf_instructions.append(insn)
                if insn.mnemonic=='ret':
                    leaf_termination='return';break
                if insn.mnemonic=='jmp' and insn.operands[0].type==X86_OP_IMM and insn.operands[0].imm in by_start:
                    leaf_termination='tail_jump_to_unwind_function';break
                if insn.mnemonic=='int3':break
            if not leaf_termination:
                raise ValueError(f'bounded leaf {start:x} lacks a return/known tail target; manual control-flow analysis required')
            lines.append('; leaf candidate: direct caller validated; linear disassembly to FIRST return/known tail, not complete CFG')
        groups=[(f'; unwind fragment {f.struct.BeginAddress:x}..{f.struct.EndAddress:x}',disassemble(f)) for f in fragments]
        if leaf_instructions:groups.append(('',leaf_instructions))
        for label,instructions in groups:
            if label:lines.append(label)
            for i in instructions:
                note=''
                if i.mnemonic.startswith('v'):vector_instructions.append(dict(rva=hex(i.address),instruction=i.mnemonic+' '+i.op_str))
                for operand in i.operands:
                    if operand.type==X86_OP_MEM and operand.mem.base==X86_REG_RIP:
                        target=i.address+i.size+operand.mem.disp
                        if 0<=target<pe.OPTIONAL_HEADER.SizeOfImage:
                            data=pe.get_data(target,256).split(b'\0')[0]
                            if len(data)>=5 and all(32<=c<127 for c in data):
                                value=data.decode();strings.append(dict(site=hex(i.address),target=hex(target),text=value))
                                note=' ; '+value
                if i.mnemonic=='call':calls.append(dict(site=hex(i.address),operand=i.op_str))
                lines.append(f'{i.address:x}: {i.mnemonic} {i.op_str}{note}')
        omitted_leaf_branches=[]
        if leaf_instructions:
            addresses={i.address for i in leaf_instructions}
            omitted_leaf_branches=sorted({hex(i.operands[0].imm) for i in leaf_instructions
                if i.mnemonic.startswith('j') and i.operands[0].type==X86_OP_IMM and
                i.operands[0].imm not in addresses})
            if omitted_leaf_branches:lines.append('; branch/tail targets not disassembled here: '+', '.join(omitted_leaf_branches))
        (out/f'{start:x}.asm').write_text('\n'.join(lines),encoding='utf8')
        reports.append(dict(owner=hex(start),fragments=[[hex(f.struct.BeginAddress),hex(f.struct.EndAddress)] for f in fragments],
                            strings=strings,vector_instructions=vector_instructions,calls=calls,
                            leaf_candidate=bool(leaf_instructions),leaf_termination=leaf_termination,
                            validated_leaf_call_site=leaf_call_site,
                            omitted_leaf_branch_targets=omitted_leaf_branches))
    report=dict(exe_sha256=hashlib.sha256(raw).hexdigest(),note='direct-call candidates only; not verified render hook points',resolved_sites=resolved_sites,edges=edges,functions=reports)
    (out/'index.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print('Functions',len(reports),'validated direct call edges',len(edges))
    for f in reports:
        names=sorted({s['text'] for s in f['strings']})
        if names:print(f['owner'],names[:15])
    camera=next((f for f in reports if f['owner']=='0x2662a0'),None)
    if camera:print('camera fragments',camera['fragments'],'VEX/EVEX mnemonic candidates',len(camera['vector_instructions']))


if __name__=='__main__':main()
