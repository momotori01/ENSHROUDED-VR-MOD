"""Instruction-validated immediate references to a native reflected event hash."""
import argparse,bisect,json,pathlib,struct,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_IMM

def main():
 p=argparse.ArgumentParser();p.add_argument('value',type=lambda v:int(v,0));p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
 raw=pathlib.Path('enshrouded.exe').read_bytes();pe=pefile.PE(data=raw);functions=sorted(pe.DIRECTORY_ENTRY_EXCEPTION,key=lambda f:f.struct.BeginAddress);starts=[f.struct.BeginAddress for f in functions];cache={};rows=[];md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True;needle=struct.pack('<I',a.value)
 for sec in pe.sections:
  if not sec.Characteristics&0x20000000:continue
  data=sec.get_data();at=data.find(needle)
  while at>=0:
   rva=sec.VirtualAddress+at;i=bisect.bisect_right(starts,rva)-1
   if i>=0:
    lo,hi=starts[i],functions[i].struct.EndAddress
    if lo<=rva<hi:
     if lo not in cache:cache[lo]=list(md.disasm(pe.get_data(lo,hi-lo),lo))
     for ins in cache[lo]:
      if ins.address<=rva<ins.address+ins.size and any(o.type==X86_OP_IMM and o.imm&0xffffffff==a.value for o in ins.operands):rows.append(dict(site=hex(ins.address),fragment=hex(lo),instruction=ins.mnemonic+' '+ins.op_str))
   at=data.find(needle,at+1)
 a.output.write_text(json.dumps(rows,indent=2)+'\n',encoding='utf8');print(json.dumps(rows,indent=2))
if __name__=='__main__':main()
