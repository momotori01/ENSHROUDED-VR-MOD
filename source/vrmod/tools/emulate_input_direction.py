"""Execute the local game's quaternion and full-rotation movement instructions.

Isolated Unicorn CPU only: no game launch, writes, imports, syscalls, or network.
Confirms why feeding the head quaternion into all locomotion paths is incorrect.
"""
import argparse,json,math,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RSP,UC_X86_REG_RIP,UC_X86_REG_R14

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--vectors',type=pathlib.Path,default=pathlib.Path('docs/analysis/input_math_vectors.json'))
    p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('docs/analysis/emulated_input_direction.json'))
    a=p.parse_args();engine=CameraMath(pathlib.Path('enshrouded.exe'))
    engine.allowed.extend([(0x2883e1,0x28846f),(0x2888d0,0x2888e1)])
    vectors=json.loads(a.vectors.read_text(encoding='utf8'))
    if not 1<=len(vectors)<=1024:raise ValueError('unexpected vector count')
    cpu=engine.cpu
    def basis(q):
        cpu.mem_write(engine.data,struct.pack('<4f',*q))
        cpu.mem_write(engine.data+0x100,b'\0'*48)
        engine.invoke(0x1d3a0,engine.data,engine.data+0x100)
        return struct.unpack('<12f',cpu.mem_read(engine.data+0x100,48))
    def full_move(q,stick):
        m=basis(q);sp=engine.stack+0xe008;actor=engine.data+0x1000
        cpu.mem_write(actor,b'\0'*0x200)
        cpu.mem_write(actor+0x18c,struct.pack('<3f',stick[0],0,stick[1]))
        cpu.mem_write(sp+0x50,struct.pack('<12f',*m))
        cpu.reg_write(UC_X86_REG_RSP,sp);cpu.reg_write(UC_X86_REG_R14,actor)
        cpu.emu_start(engine.base+0x2883e1,engine.base+0x2888e1,timeout=1000000,count=256)
        if cpu.reg_read(UC_X86_REG_RIP)!=engine.base+0x2888e1:raise ValueError('movement fragment did not reach stop')
        return struct.unpack('<3f',cpu.mem_read(actor+0x198,12))
    rows=[]
    def check(name,ok,**fields):rows.append(dict(name=name,status='PASS' if ok else 'FAIL',**fields))
    raw_failures=no_roll_failures=0
    for i,v in enumerate(vectors):
        actual=full_move(v['yaw_q'],v['stick']);error=max(abs(x-y) for x,y in zip(actual,v['movement']))
        check(f'yaw_basis_native_movement_{i}',all(math.isfinite(x) for x in actual) and error<2e-6,max_error=error)
        aim=basis(v['aim_q'])[8:11];error=max(abs(x-y) for x,y in zip(aim,v['forward']))
        check(f'roll_free_native_aim_{i}',error<2e-6,max_error=error)
        raw=full_move(v['head_q'],v['stick']);no_roll=full_move(v['aim_q'],v['stick'])
        raw_failures+=max(abs(x-y) for x,y in zip(raw,v['movement']))>.001
        no_roll_failures+=max(abs(x-y) for x,y in zip(no_roll,v['movement']))>.001
    check('negative_control_head_rotation_is_not_yaw_only',raw_failures>=len(vectors)//2,mismatches=raw_failures)
    check('negative_control_removing_roll_still_leaks_pitch',no_roll_failures>=len(vectors)//2,mismatches=no_roll_failures)
    report=dict(exe_sha256=SHA256,scope=__doc__,checks=rows,passed=sum(r['status']=='PASS' for r in rows),
                failed=sum(r['status']=='FAIL' for r in rows),substitutions=engine.calls)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    for row in rows[-2:]:print(row)
    print('PASS',report['passed'],'FAIL',report['failed']);return int(bool(report['failed']))

if __name__=='__main__':raise SystemExit(main())
