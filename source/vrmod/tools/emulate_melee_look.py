"""Run native 337490 body-look and 1dfe90 quaternion code without launching the game.
Only body branch is exercised here; conditional branch identity is audited from
raw BaseActor and local query operands by check_motion_attack.native_look_branch.
"""
import argparse,json,pathlib,struct,math
from emulate_camera_math import CameraMath,SHA256,pefile

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--probe',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,required=True);a=p.parse_args()
 engine=CameraMath(pathlib.Path('enshrouded.exe'));pe=pefile.PE('enshrouded.exe');cpu=engine.cpu
 for rva in (0x337490,0x1dfe90):
  f=next(f for f in pe.DIRECTORY_ENTRY_EXCEPTION if f.struct.BeginAddress==rva);engine.allowed.append((rva,f.struct.EndAddress))
 def run(q):
  actor=engine.data+0x1000;transform=engine.data+0x2000;raw=bytes([0xa5])*440;body=struct.pack('<3q4f',123,456,789,*q)
  cpu.mem_write(actor,raw);cpu.mem_write(transform,body);engine.invoke(0x337490,actor,transform)
  actual=bytes(cpu.mem_read(actor,440));unchanged=actual[:420]==raw[:420] and actual[432:]==raw[432:] and bytes(cpu.mem_read(transform,40))==body
  return list(struct.unpack_from('<3f',actual,420)),unchanged
 rows=[]
 for d in range(-180,181,10):
  radians=math.radians(d);actual,unchanged=run([0,math.sin(radians/2),0,math.cos(radians/2)])
  err=max(abs(x-y) for x,y in zip(actual,[math.sin(radians),0,math.cos(radians)]));rows.append(dict(name='known_yaw_'+str(d),pass_=err<2e-6 and unchanged,error=err))
 if a.probe:
  from check_motion_attack import native_look_branch
  native=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
  seen=set()
  for r in native:
   if r['event']!='action_sequence_snapshot' or native_look_branch(r)[0]!='body_337490':continue
   b=r['native_look_context']['body_transform_bytes'];key=(b,tuple(r['world_look']))
   if key in seen:continue
   seen.add(key);q=struct.unpack_from('<4f',bytes.fromhex(b),24);actual,unchanged=run(q)
   same=struct.pack('<3f',*actual)==struct.pack('<3f',*r['world_look'])
   rows.append(dict(name='actual_body_'+str(len(seen)),pass_=same and unchanged,bit_exact=same,actual=actual,observed=r['world_look']))
  rows.append(dict(name='actual_body_samples_present',pass_=bool(seen),unique_samples=len(seen)))
 report=dict(exe_sha256=SHA256,scope=__doc__,checks=rows,passed=sum(r['pass_'] for r in rows),failed=sum(not r['pass_'] for r in rows),substitutions=engine.calls)
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n');print('PASS',report['passed'],'FAIL',report['failed']);return bool(report['failed'])
if __name__=='__main__':raise SystemExit(main())
