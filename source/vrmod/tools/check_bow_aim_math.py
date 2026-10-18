"""Validate C++ two-hand bow aim using captured events and unchanged native Crosshair instructions."""
import argparse,csv,hashlib,json,math,struct,subprocess
from pathlib import Path
from emulate_camera_math import CameraMath,SHA256
from check_capture_cameras import qmul,rotate
from unicorn.x86_const import UC_X86_REG_R9,UC_X86_REG_RAX

def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('spawn_probe',type=Path);a.add_argument('pose_probe',type=Path);a.add_argument('--output',type=Path,required=True);v=a.parse_args()
 exe=Path('vrmod/build/bin/Release/evr_bow_aim_tests.exe').resolve()
 run=lambda args,inp=None:subprocess.run([str(exe)]+args,input=inp,text=True,capture_output=True,check=True).stdout
 vectors=[json.loads(s) for s in run(['--vectors']).splitlines()]
 source=[json.loads(s) for s in (v.spawn_probe/'view-resource-probe.jsonl').read_text().splitlines()];shot=next(e for e in source if e['event']=='bow_spawn_sample' and e['is_projectile'])
 assert shot['typed_dispatch_path'] and shot['sources_unchanged'] and shot['result']
 events=[json.loads(s) for s in (v.pose_probe/'view-resource-probe.jsonl').read_text().splitlines()]
 poses={e['frame']:e for e in events if e['event']=='motion_visual_pose' and e['item']==327157552}
 physical={e['frame']:e for e in events if e['event']=='motion_bow_physical_pose' and e['held']}
 hands={int(x[0]):list(map(float,x)) for x in csv.reader(x for x in (v.pose_probe.parent/'mock-hands.csv').read_text().splitlines() if x and not x.startswith('#'))}
 rows=[];frames=[570,600,620,650,680,700]
 for f in frames:
  assert f in physical;h=hands[f];m=poses[f];yaw,pitch,roll=[math.radians(t)/2 for t in h[4:7]]
  q=qmul(qmul([0,math.sin(yaw),0,math.cos(yaw)],[math.sin(pitch),0,0,math.cos(pitch)]),[0,0,math.sin(roll),math.cos(roll)])
  local=rotate([-q[0],-q[1],-q[2],q[3]],[a-b for a,b in zip(h[14:17],h[1:4])]);nock=[local[i]/m['scale'][i] for i in range(3)]
  rows.append(' '.join(map(str,m['q32']+m['orientation']+m['scale']+nock+[shot['event_bytes']])))
 recorded=[json.loads(s) for s in run(['--stdin'],'\n'.join(rows)+'\n').splitlines()];assert len(recorded)==len(frames)
 engine=CameraMath(Path('enshrouded.exe'));cpu=engine.cpu
 engine.allowed += [(0x1e4460,0x1e45fb),(0x1e3220,0x1e3365),(0x1e3020,0x1e30fa),(0x334950,0x334959),(0x87c570,0x87c68b),(0x87cf80,0x87d087),(0x87d1f0,0x87d2c6),(0x1dfe90,0x1dffde),(0xd69930,0xd69a88),(0x1235310,0x123580f),(0x1235810,0x1235f06)]
 out,ctx,inp,event=engine.data,engine.data+0x1000,engine.data+0x2000,engine.data+0x4000;results=[]
 for index,r in enumerate(vectors+recorded):
  raw=bytes.fromhex(r['event']);camera=bytes.fromhex(r['camera']);assert len(raw)==336 and len(camera)==44
  assert struct.unpack_from('<8f',raw,0x104)==(0.,)*8,'Random scatter not validated by this fixture'
  source=bytearray(1320);source[0x28:0x54]=camera
  cpu.mem_write(out,b'\xcc'*56);cpu.mem_write(ctx,bytes(0x200));cpu.mem_write(ctx+0x88,struct.pack('<Q',inp));cpu.mem_write(inp,bytes(source));cpu.mem_write(event,raw);cpu.reg_write(UC_X86_REG_R9,event);engine.invoke(0x1e4460,out,0,ctx)
  actual=bytes(cpu.mem_read(out,56));p=struct.unpack_from('<3q',actual);q=struct.unpack_from('<4f',actual,24)
  nq=r['model_q'];norm=math.sqrt(sum(x*x for x in nq));nq=[x/norm for x in nq]
  rest=[0,.00155006*r['scale'][1],0];nock=[r['nock'][i]*r['scale'][i]*(1 if i<2 else -1) for i in range(3)];delta=[x-y for x,y in zip(rest,nock)];span=math.sqrt(sum(x*x for x in delta));direction=rotate(nq,[x/span for x in delta]);off=rotate(nq,rest)
  expected=[r['model_q32'][i]/2**32+off[i] for i in range(3)];poserr=max(abs(p[i]/2**32-expected[i]) for i in range(3));direrr=max(abs(x-y) for x,y in zip(rotate(q,[0,0,1]),direction))
  same=bytes(cpu.mem_read(inp,len(source)))==bytes(source) and bytes(cpu.mem_read(event,336))==raw
  ok=poserr<3e-6 and direrr<3e-6 and same and cpu.reg_read(UC_X86_REG_RAX)&255==1 and struct.unpack_from('<3f',actual,40)==(1.,1.,1.)
  results.append(dict(case=index,recorded_frame=frames[index-len(vectors)] if index>=len(vectors) else None,position_error_m=poserr,forward_error=direrr,source_unchanged=same,status='PASS' if ok else 'FAIL'))
 checks=dict(native_math=all(x['status']=='PASS' for x in results),recorded_event_cases=len(recorded)==6,generated_cases=len(vectors)==128,no_substitutions=not engine.calls)
 report=dict(status='PASS' if all(checks.values()) else 'FAIL',exe_sha256=SHA256,checks=checks,max_position_error_m=max(x['position_error_m'] for x in results),max_forward_error=max(x['forward_error'] for x in results),source_spawn_probe=str(v.spawn_probe),source_pose_probe=str(v.pose_probe),source_log_sha256=[hashlib.sha256((p/'view-resource-probe.jsonl').read_bytes()).hexdigest() for p in [v.spawn_probe,v.pose_probe]],cases=results,limitation='Offline value math only. Uses a provisional arrow rest and controller grip. No live aim write, firing authority, projectile collision, physical damage scaling or hardware calibration. Nonzero random spread is not covered.')
 v.output.write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:x for k,x in report.items() if k!='cases'},indent=2));return report['status']!='PASS'
if __name__=='__main__':raise SystemExit(main())
