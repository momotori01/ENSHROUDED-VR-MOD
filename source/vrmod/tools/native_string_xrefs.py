"""Find instruction-boundary validated RIP LEA references to an exact local PE string.

Local-only reverse engineering evidence; candidate sites are not approved hooks.
"""
import argparse,bisect,hashlib,json,pathlib,re,struct,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64

def main():
    a=argparse.ArgumentParser(description=__doc__)
    a.add_argument('text');a.add_argument('--output',type=pathlib.Path,required=True);args=a.parse_args()
    raw=pathlib.Path('enshrouded.exe').read_bytes();pe=pefile.PE(data=raw)
    value=args.text.encode('ascii')+b'\0';targets=[];at=raw.find(value)
    while at>=0:
        targets.append(pe.get_rva_from_offset(at));at=raw.find(value,at+1)
    fs=sorted((f.struct.BeginAddress,f.struct.EndAddress) for f in pe.DIRECTORY_ENTRY_EXCEPTION)
    starts=[lo for lo,hi in fs];md=Cs(CS_ARCH_X86,CS_MODE_64);cache={};results=[]
    for section in pe.sections:
        if not section.Characteristics&0x20000000:continue
        b=section.get_data()
        for m in re.finditer(rb'[\x48\x4c]\x8d[\x05\x0d\x15\x1d\x25\x2d\x35\x3d][\s\S]{4}',b):
            site=section.VirtualAddress+m.start();target=site+7+struct.unpack_from('<i',b,m.start()+3)[0]
            if target not in targets:continue
            index=bisect.bisect_right(starts,site)-1
            if index<0 or site>=fs[index][1]:continue
            lo,hi=fs[index]
            if lo not in cache:cache[lo]={i.address:i.mnemonic+' '+i.op_str for i in md.disasm(pe.get_data(lo,hi-lo),lo)}
            if site not in cache[lo]:continue
            results.append(dict(site=hex(site),target=hex(target),fragment=hex(lo),instruction=cache[lo][site]))
    report=dict(exe_sha256=hashlib.sha256(raw).hexdigest(),text=args.text,targets=list(map(hex,targets)),sites=results,
        limit='Exact ASCII string and RIP LEA only. No indirect references; no runtime modification.')
    args.output.write_text(json.dumps(report,indent=2));print(json.dumps(report,indent=2))

if __name__=='__main__':main()
