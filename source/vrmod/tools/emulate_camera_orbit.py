"""Validate native sensitivity/transition-weighted camera orbit in a CPU sandbox.
No process hooks, host calls, or distributed game bytes. Build hash is enforced.
"""
import itertools,json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_R9

def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def main():
 e=CameraMath(pathlib.Path('enshrouded.exe'));e.allowed=[(0xca5040,0xca512e)]
 data=e.data;extra=data+0x10000;e.cpu.mem_map(extra,0x30000)
 state=data+0x100;context=data+0x200;out=data;config=extra;over=extra+0x10000
 rows=[];before=(.125,-.25);sens=(.1,.2,.3,.4)
 for override,mode,combat,camera,weight,xy in itertools.product((False,True),(0,1,2),(False,True),(0,25,44),(0.,.25,1.),((0.,0.),(.125,-.5),(-.75,.875))):
  e.cpu.mem_write(state,struct.pack('<Q',config));e.cpu.mem_write(context+8,struct.pack('<Q',over))
  
  for off,v in zip((0x10,0x14,0x1c,0x20),sens):e.cpu.mem_write(context+off,struct.pack('<f',v))
  e.cpu.mem_write(context+0x174,bytes([mode]))
  e.cpu.mem_write(over+0xf500,struct.pack('<Q',1<<camera if override else 0))
  e.cpu.mem_write(over+camera*0x570+0x51,bytes([2 if combat else 0]))
  e.cpu.mem_write(config+camera*0x578+9,bytes([2 if combat else 0]))
  e.cpu.mem_write(out,struct.pack('<2f',*before));sp=e.stack+0xf008
  e.cpu.mem_write(sp+0x28,struct.pack('<2f',*xy));e.cpu.mem_write(sp+0x30,struct.pack('<f',weight))
  e.cpu.reg_write(UC_X86_REG_R9,camera)
  e.invoke(0xca5040,out,state,context)
  actual=struct.unpack('<2f',e.cpu.mem_read(out,8))
  scale=f32(sens[mode*2+int(combat)]) if mode<2 else 0.
  expect=tuple(f32(before[i]+f32(f32(f32(xy[i])*scale)*f32(weight))) for i in range(2))
  rows.append(dict(override=override,mode=mode,combat=combat,camera=camera,weight=weight,input=xy,actual=actual,expected=expect,status='PASS' if actual==expect else 'FAIL'))
 report=dict(exe_sha256=SHA256,read_only=True,cases=rows,passed=sum(x['status']=='PASS' for x in rows),failed=sum(x['status']=='FAIL' for x in rows))
 pathlib.Path('docs/analysis/emulated_camera_orbit.json').write_text(json.dumps(report,indent=2))
 print('RESULT',report['passed'],'PASS',report['failed'],'FAIL')
 return bool(report['failed'])
if __name__=='__main__':raise SystemExit(main())
