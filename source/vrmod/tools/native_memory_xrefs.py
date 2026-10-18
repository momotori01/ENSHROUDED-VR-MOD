"""Read-only PE memory-displacement references; candidate evidence, never approved hooks."""
import argparse,bisect,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent/'python'))
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64
from capstone.x86 import X86_OP_MEM
p=argparse.ArgumentParser(description=__doc__);p.add_argument('offset',type=lambda s:int(s,0));p.add_argument('--mnemonic',default='');p.add_argument('--output',type=Path,required=True);a=p.parse_args()
pe=pefile.PE('enshrouded.exe');fs=sorted((f.struct.BeginAddress,f.struct.EndAddress) for f in pe.DIRECTORY_ENTRY_EXCEPTION);starts=[f[0] for f in fs];md=Cs(CS_ARCH_X86,CS_MODE_64);md.detail=True;cache={};hits={}
for sec in pe.sections:
 if not sec.Characteristics&0x20000000:continue
 raw=sec.get_data();at=-1
 while True:
  at=raw.find(struct.pack('<i',a.offset),at+1)
  if at<0:break
  site=sec.VirtualAddress+at;j=bisect.bisect_right(starts,site)-1
  if j<0:continue
  lo,hi=fs[j]
  if not lo<=site<hi:continue
  if lo not in cache:cache[lo]=list(md.disasm(pe.get_data(lo,hi-lo),lo))
  for ins in cache[lo]:
   if ins.address<=site<ins.address+ins.size and (not a.mnemonic or ins.mnemonic==a.mnemonic) and any(o.type==X86_OP_MEM and o.mem.disp==a.offset for o in ins.operands):hits[ins.address]=dict(site=hex(ins.address),function=hex(lo),end=hex(hi),instruction=ins.mnemonic+' '+ins.op_str)
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(list(hits.values()),indent=2)+'\n');print('references',len(hits),'functions',len({x['function'] for x in hits.values()}))
for x in list(hits.values())[:50]:print(x)
