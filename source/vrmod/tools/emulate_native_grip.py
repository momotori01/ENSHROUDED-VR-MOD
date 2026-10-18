"""Original RenderPose hierarchy math in isolated Unicorn; compare raw scaled-quaternion FK."""
import argparse,json,math,random,struct
from pathlib import Path
from emulate_camera_math import CameraMath,SHA256
from analyze_native_weapon_grip_affine import mv,mm,inv
from unicorn.x86_const import UC_X86_REG_RSP,UC_X86_REG_RIP,UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_MXCSR

def rawmat(q,s):
 x,y,z,w=q
 return [[v*s[j] for j,v in enumerate(row)] for row in [[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],[2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],[2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]]]
def combine(a,b):return mm(a[0],b[0]),[u+v for u,v in zip(a[1],mv(a[0],b[1]))]
def relative(a,b):
 ai=inv(a[0]);return mm(ai,b[0]),mv(ai,[u-v for u,v in zip(b[1],a[1])])
def matrix(raw):
 v=struct.unpack('<16f',raw);return [[v[j*4+i] for j in range(3)] for i in range(3)],list(v[12:15])
class Engine(CameraMath):
 def __init__(self):
  super().__init__(Path('enshrouded.exe'));self.allowed=[r for r in self.allowed if r[0] in (0xd7a0c0,0x1d3a0)]+[(0x1471c0,0x147684)]
 def invoke(self,rva,rcx=0,rdx=0,r8=0):
  sp=self.stack+0xf008;self.cpu.mem_write(sp,struct.pack('<Q',self.stop))
  for reg,value in [(UC_X86_REG_RSP,sp),(UC_X86_REG_RCX,rcx),(UC_X86_REG_RDX,rdx),(UC_X86_REG_R8,r8),(UC_X86_REG_MXCSR,0x1f80)]:self.cpu.reg_write(reg,value)
  self.cpu.emu_start(self.base+rva,self.stop,timeout=3000000,count=100000)
  if self.cpu.reg_read(UC_X86_REG_RIP)!=self.stop:raise ValueError('native grip bounded invocation failed')
 def bones(self,joints,parents):
  n=len(parents);h=self.data+0x200;src=self.data+0x2000;dst=self.data+0x4000;pa=self.data+0x1000
  self.cpu.mem_write(self.data+0x100,struct.pack('<4Q',dst,n,src,n));self.cpu.mem_write(h,bytes(72));self.cpu.mem_write(h+24,struct.pack('<iI',pa-h-24,n));self.cpu.mem_write(h+48,struct.pack('<I',n));self.cpu.mem_write(pa,struct.pack('<'+'H'*n,*parents));self.cpu.mem_write(src,joints);self.cpu.mem_write(dst,bytes(64*n))
  before=bytes(self.cpu.mem_read(h,0x4000-0x200));self.invoke(0x1471c0,self.data+0x100,self.data+0x110,h)
  if bytes(self.cpu.mem_read(h,0x4000-0x200))!=before:raise ValueError('native math changed sources')
  return [matrix(bytes(self.cpu.mem_read(dst+i*64,64))) for i in range(n)]
 def model(self,raw,origin):
  self.cpu.mem_write(self.data+0x100,raw);self.cpu.mem_write(self.data+0x200,origin);self.invoke(0xd7a0c0,self.data+0x100,self.data+0x300,self.data+0x200)
  if bytes(self.cpu.mem_read(self.data+0x100,len(raw)))!=raw:raise ValueError('native model input changed')
  return matrix(bytes(self.cpu.mem_read(self.data+0x300,64)))
def compare(native,raw,parents):
 result=[]
 for i,p in enumerate(parents):
  v=struct.unpack_from('<12f',raw,i*48);a=(rawmat(v[:4],v[8:11]),list(v[4:7]));result.append(a if p==65535 else combine(result[p],a))
 return max(max(abs(a-b) for r,s in zip(x[0],y[0]) for a,b in zip(r,s)) for x,y in zip(native,result)),max(max(abs(a-b) for a,b in zip(x[1],y[1])) for x,y in zip(native,result))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();parents=struct.unpack('<102H',(a.probe/'hand-pose/joint-parents.bin').read_bytes());engine=Engine();checks=[];frames=[]
 def run(name,joints):
  actual=engine.bones(joints,parents);em,ep=compare(actual,joints,parents);checks.append(dict(name=name,matrix_error=em,position_error=ep,status='PASS' if em<1e-5 and ep<1e-5 else 'FAIL'));return actual
 run('native_default',(a.probe/'hand-pose/default-joints.bin').read_bytes()[:4896]);rng=random.Random(0x1471c0)
 for k in range(32):
  data=bytearray()
  for i in range(102):
   q=[rng.uniform(-1,1) for _ in range(4)];norm=math.sqrt(sum(x*x for x in q));q=[x/norm*rng.uniform(.999,1.001) for x in q];v=q+[rng.uniform(-.3,.3) for _ in range(3)]+[0]+[rng.uniform(.95,1.05) for _ in range(3)]+[0];data.extend(struct.pack('<12f',*v))
  run('scaled_nonunit_'+str(k),bytes(data))
 events=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
 for sample in [e for e in events if e['event']=='native_grip_sample']:
  f=a.probe/sample['folder'];j=(f/'joints.bin').read_bytes();bones=run('frame_'+str(sample['frame']),j);hand=(f/'hand-model.bin').read_bytes()[0x38:0x70];weapon=(f/'weapon-model.bin').read_bytes()[0x38:0x70];origin=hand[:24];hm=engine.model(hand,origin);wm=engine.model(weapon,origin);l,r=[combine(hm,bones[i]) for i in (64,94)];rw=relative(r,wm)
  frames.append(dict(frame=sample['frame'],left_in_weapon=relative(wm,l),right_in_weapon=relative(wm,r),right_to_weapon=rw,hand_distance=math.dist(l[1],r[1])))
 out=dict(scope=__doc__,exe_sha256=SHA256,checks=checks,frames=frames,substitutions=engine.calls,calibration_approved=False)
 path=a.probe/'native-grip-emulation.json';path.write_text(json.dumps(out,indent=2)+'\n');print('PASS',sum(x['status']=='PASS' for x in checks),'FAIL',sum(x['status']=='FAIL' for x in checks),'substitutions',engine.calls)
 print('left origin error max',max(math.sqrt(sum(v*v for v in x['left_in_weapon'][1])) for x in frames));print('right position ranges',[[min(x['right_in_weapon'][1][i] for x in frames),max(x['right_in_weapon'][1][i] for x in frames)] for i in range(3)])
 return int(any(x['status']!='PASS' for x in checks) or bool(engine.calls))
if __name__=='__main__':raise SystemExit(main())
