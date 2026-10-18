"""Validate actual native entity destructor return addresses, including the two
small tail bridges. No game process access and no code modification.
"""
import bisect,hashlib,json,struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).parent/'python'))
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_64

def validate(rows):
 raw=Path('enshrouded.exe').read_bytes();sha=hashlib.sha256(raw).hexdigest()
 reference=json.loads(Path('docs/analysis/entity_lifetime_destroy/index.json').read_text())
 if sha!=reference['exe_sha256']:raise ValueError('game build differs from destructor analysis')
 pe=pefile.PE(data=raw,fast_load=True);pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
 md=Cs(CS_ARCH_X86,CS_MODE_64)
 bridges=[(0x3f4460,bytes.fromhex('488b4908e9373c4c00'),0x8b80a0),(0x8b80a0,bytes.fromhex('4881c11842cc004533c9e901030000'),0x8b83b0)]
 reports=[]
 for start,expected,target in bridges:
  actual=pe.get_data(start,len(expected));ins=list(md.disasm(actual,start))
  if actual!=expected or not ins or ins[-1].mnemonic!='jmp' or int(ins[-1].op_str,16)!=target:raise ValueError('tail bridge bytes/control flow differ')
  reports.append({'start':hex(start),'target':hex(target),'bytes':actual.hex(),'instructions':[f'{i.address:x}: {i.mnemonic} {i.op_str}' for i in ins]})
 functions=sorted(pe.DIRECTORY_ENTRY_EXCEPTION,key=lambda f:f.struct.BeginAddress);starts=[f.struct.BeginAddress for f in functions]
 callers=sorted({int(r['caller'],16) for r in rows if r['event'] in ('contact_actor_owner_destroy_begin','contact_actor_absent_destroy_begin')})
 allowed=set();calls=[]
 for ret in callers:
  site=ret-5;index=bisect.bisect_right(starts,site)-1
  if index<0:raise ValueError('caller has no unwind range')
  f=functions[index].struct
  if site>=f.EndAddress:raise ValueError('caller outside unwind fragment')
  decoded=list(md.disasm(pe.get_data(f.BeginAddress,f.EndAddress-f.BeginAddress),f.BeginAddress))
  i=next((i for i in decoded if i.address==site),None)
  if not i or i.size!=5 or i.mnemonic!='call' or i.bytes[0]!=0xe8:raise ValueError('caller is not an instruction-aligned direct call')
  target=ret+struct.unpack('<i',bytes(i.bytes[1:]))[0]
  if target not in (0x3f4460,0x8b80a0,0x8b83b0):raise ValueError('caller does not reach verified destructor/tail path')
  allowed.add(ret);calls.append({'return':hex(ret),'site':hex(site),'target':hex(target),'fragment':[hex(f.BeginAddress),hex(f.EndAddress)],'bytes':bytes(i.bytes).hex()})
 return allowed,{'exe_sha256':sha,'scope':__doc__,'bridges':reports,'callers':calls,'status':'PASS' if allowed else 'FAIL'}

if __name__=='__main__':
 probe=Path(sys.argv[1]);rows=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 _,report=validate(rows);(probe/'actor-owner-callers.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
