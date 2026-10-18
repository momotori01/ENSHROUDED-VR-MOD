"""Execute original forEachHitEvent in isolation, including empty/exhausted lists.
Synthetic HitEvent values test ABI decoding only; they do not prove live damage.
"""
import json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import *
class HitCPU(CameraMath):
 def on_instruction(self,cpu,address,size,unused):
  rva=address-self.base
  if not 0x35a310<=rva<0x35ab25:raise ValueError(f'Unexpected hit callback call {rva:x}')

def main():
 e=HitCPU(pathlib.Path('enshrouded.exe'));c=e.cpu;d=e.data;checks={};cases=[]
 # Native public output pin order; scalar offsets also checked against reflection.
 mapping={10:(0xe3,'B'),11:(0xdf,'B'),13:(0xdc,'H'),14:(0x90,'I'),15:(0xa4,'I'),16:(0xa0,'I'),17:(0x94,'I'),18:(0xac,'I'),19:(0x6c,'3f'),20:(0x78,'3f'),21:(0xbc,'i'),22:(0xc0,'i')}
 bits=[1,0,2,5,3,9,13,4,6,7]
 for count in [0,1,3]:
  for index in sorted({0,count,max(0,count-1),0xffffffff}):
   for flags in [0,0xffffffff]:
    for iterator in [False,True]:
     c.mem_write(d,bytes(0x10000));output=d+0x100;args=d+0x200;ids=d+0x300;vm=d+0x400;table=d+0x500;scope=d+0x1000;queue=d+0x2000;data=d+0x3000;values=d+0x5000
     c.mem_write(args,struct.pack('<Q',ids));c.mem_write(vm,struct.pack('<Q',table));c.mem_write(vm+0x10,struct.pack('<Q',values));c.mem_write(vm+0x30,struct.pack('<Q',scope));c.mem_write(scope+0x448,struct.pack('<Q',queue));c.mem_write(queue+0x18,struct.pack('<Q',data));c.mem_write(queue+0x28,struct.pack('<I',count))
     for i in range(24):
      if i==12 or(i==23 and not iterator):continue
      c.mem_write(ids+i*4,struct.pack('<I',i+1));c.mem_write(table+(i+1)*24+14,struct.pack('<H',i*16));c.mem_write(values+i*16,b'\xcc'*16)
     c.mem_write(values+23*16,struct.pack('<I',index));hits=[]
     for i in range(count):
      h=bytearray(232);struct.pack_into('<I',h,0x98,flags);h[0xe3]=4+i;h[0xdf]=7+i;struct.pack_into('<H',h,0xdc,0x1234+i)
      for at,val in [(0x90,1),(0xa4,101),(0xa0,201),(0x94,301),(0xac,401)]:struct.pack_into('<I',h,at,val+i)
      struct.pack_into('<3f',h,0x6c,1+i,2+i,3+i);struct.pack_into('<3f',h,0x78,0,1,0);struct.pack_into('<ii',h,0xbc,-10-i,-3-i);c.mem_write(data+i*232,bytes(h));hits.append(h)
     before=bytes(c.mem_read(d,0x10000));expected=bytearray(before);valid=iterator and index<count;result=0x100 if valid else 0 if iterator else 0x18;struct.pack_into('<H',expected,0x100,result)
     if valid:
      struct.pack_into('<I',expected,0x5000+23*16,index+1);h=hits[index]
      for i,bit in enumerate(bits):expected[0x5000+i*16]=(flags>>bit)&1
      for i,(at,fmt) in mapping.items():size=struct.calcsize('<'+fmt);expected[0x5000+i*16:0x5000+i*16+size]=h[at:at+size]
     e.invoke(0x35a310,output,args,vm);after=bytes(c.mem_read(d,0x10000));name=f'count{count}_index{index}_flags{flags}_iterator{iterator}'
     ok=after==bytes(expected) and c.reg_read(UC_X86_REG_RAX)==output;checks[name]=ok;cases.append(dict(name=name,expected_result=result,expected_valid_hit=valid,source_and_outputs_exact=ok))
 out=dict(executable_sha256=SHA256,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},passed=sum(checks.values()),failed=sum(not v for v in checks.values()),cases=cases,limitations=__doc__)
 pathlib.Path('docs/analysis/emulated_hit_iterator.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in checks.items():
  if not v:print('FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],'native HitEvent ABI');return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
