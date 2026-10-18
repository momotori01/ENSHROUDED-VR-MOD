"""Native Guid hash and category registry getters in isolated Unicorn; no game calls/writes."""
import json,random,struct,subprocess
from pathlib import Path
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX

def fnv(g):
 if not any(g):return 0
 h=0x811c9dc5
 for b in g:h=((h^b)*0x1000193)&0xffffffff
 return h

def main():
 cases=json.loads(subprocess.check_output(['vrmod/build/bin/Release/evr_weapon_category_tests.exe','--vectors'],text=True))
 e=CameraMath(Path('enshrouded.exe'));e.allowed=[(0x1e4600,0x1e4730),(0xcc0200,0xcc022f),(0xca5ce0,0xca5d6f)];report=[]
 reg=e.data+0x100;bits=e.data+0x1000;keys=e.data+0x2000;vals=e.data+0x4000;res=e.data+0x6000;reference=e.data+0x8000
 for c in cases:
  mem=bytearray(0x10000);cap=c['capacity'];count=c['collisions']+1;h=c['hash'];g=bytes(c['guid'])
  for off,v in [(0x3c8,bits),(0x3d0,cap),(0x3e8,keys),(0x400,vals)]:struct.pack_into('<Q',mem,off,v)
  struct.pack_into('<I',mem,0x414,count)
  for n in range(count):
   i=(h+n)&(cap-1);off=0x1000+i//64*8;word=struct.unpack_from('<Q',mem,off)[0];struct.pack_into('<Q',mem,off,word|(1<<(i%64)))
   struct.pack_into('<I',mem,0x2000+i*4,h if n==count-1 else (h+(n+1)*cap)&0xffffffff);struct.pack_into('<Q',mem,0x4000+i*8,res)
  mem[0x6000:0x6010]=g;mem[0x6054]=c['type'];mem[0x8000:0x8010]=g;e.cpu.mem_write(e.data,bytes(mem))
  e.invoke(0x1e4600,reference);actualHash=e.cpu.reg_read(UC_X86_REG_RAX)&0xffffffff
  e.invoke(0xcc0200,reg,h);pointer=e.cpu.reg_read(UC_X86_REG_RAX);value=e.cpu.mem_read(pointer+68,1)[0] if pointer else None
  preserved=bytes(e.cpu.mem_read(e.data,len(mem)))==bytes(mem)
  ok=actualHash==h==fnv(g) and pointer==c['pointer']+16 and value==c['type'] and preserved
  report.append(dict(kind='category',capacity=cap,collisions=c['collisions'],type=c['type'],native_hash=actualHash,native_pointer=pointer,preserved=preserved,status='PASS' if ok else 'FAIL'))
  # A known empty bucket must not return a category, including zero count.
  struct.pack_into('<I',mem,0x414,0);e.cpu.mem_write(e.data,bytes(mem));e.invoke(0xcc0200,reg,h)
  report.append(dict(kind='empty',status='PASS' if not e.cpu.reg_read(UC_X86_REG_RAX) and bytes(e.cpu.mem_read(e.data,len(mem)))==bytes(mem) else 'FAIL'))
 rng=random.Random(0x1234)
 for g in [bytes(16)]+[rng.randbytes(16) for _ in range(256)]:
  e.cpu.mem_write(reference,g);e.invoke(0x1e4600,reference);got=e.cpu.reg_read(UC_X86_REG_RAX)&0xffffffff
  report.append(dict(kind='hash',guid=g.hex(),value=got,status='PASS' if got==fnv(g) and bytes(e.cpu.mem_read(reference,16))==g else 'FAIL'))
 out=dict(exe_sha256=SHA256,scope=__doc__,substitutions=e.calls,checks=report,passed=sum(x['status']=='PASS' for x in report),failed=sum(x['status']=='FAIL' for x in report))
 Path('docs/analysis/emulated_weapon_category.json').write_text(json.dumps(out,indent=2)+'\n');print('PASS',out['passed'],'FAIL',out['failed'],'external substitutions',len(e.calls));return int(bool(out['failed'] or e.calls))
if __name__=='__main__':raise SystemExit(main())
