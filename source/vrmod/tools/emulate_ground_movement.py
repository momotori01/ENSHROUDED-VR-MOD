"""Execute native non-targeted ground movement math from the local game PE.

This is a bounded CPU math fixture, not a whole locomotion task or live test.
It starts after query/control-state selection. ECS name/debug lookups between
288652 and 288669 are omitted; normal math branches do not consume their outputs.
No game process, imports, syscalls, or external function substitutions are used.
"""
import argparse,json,math,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import (UC_X86_REG_RSP,UC_X86_REG_RBP,UC_X86_REG_RIP,
    UC_X86_REG_R14,UC_X86_REG_R12,UC_X86_REG_RBX,UC_X86_REG_XMM13,UC_X86_REG_XMM14)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=pathlib.Path,default=pathlib.Path('docs/analysis/emulated_ground_movement.json'))
    p.add_argument('--adapter-vectors',type=pathlib.Path,help='Actual private walking camera quaternions/sticks emitted by C++ adapter test')
    a=p.parse_args();e=CameraMath(pathlib.Path('enshrouded.exe'));cpu=e.cpu
    e.allowed.extend([(0x288578,0x288652),(0x288669,0x288709),(0x288838,0x2888e1),
                      (0x1e1890,0x1e1b00),(0xd65010,0xd651bf)])
    def basis(q):
        cpu.mem_write(e.data,struct.pack('<4f',*q));cpu.mem_write(e.data+0x100,b'\0'*48)
        e.invoke(0x1d3a0,e.data,e.data+0x100)
        return bytes(cpu.mem_read(e.data+0x100,48))
    def run(lo,hi):
        cpu.emu_start(e.base+lo,e.base+hi,timeout=1_000_000,count=2048)
        if cpu.reg_read(UC_X86_REG_RIP)!=e.base+hi:raise ValueError('native math fragment did not reach expected stop')
    def move(q,stick):
        matrix=basis(q);sp=e.stack+0xc000;bp=sp+0x100;actor=e.data+0x1000
        cpu.mem_write(sp,b'\0'*0x1000);cpu.mem_write(actor,b'\0'*0x200)
        cpu.mem_write(actor+0x18c,struct.pack('<3f',stick[0],0,stick[1]))
        cpu.mem_write(bp+0xb0,matrix);cpu.mem_write(bp+0x80,struct.pack('<3f',0,-1,0))
        for reg,value in [(UC_X86_REG_RSP,sp),(UC_X86_REG_RBP,bp),(UC_X86_REG_R14,actor),
                          (UC_X86_REG_R12,e.data+0x4000),(UC_X86_REG_RBX,0),
                          (UC_X86_REG_XMM13,0x80000000),(UC_X86_REG_XMM14,0x3f800000)]:cpu.reg_write(reg,value)
        run(0x288578,0x288652);run(0x288669,0x288709);run(0x288838,0x2888e1)
        return struct.unpack('<3f',cpu.mem_read(actor+0x198,12))
    def mul(a,b):
        x,y,z,w=a;X,Y,Z,W=b
        return (w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z)
    def quat(y,p,r):
        return mul(mul((0,math.sin(y/2),0,math.cos(y/2)),(math.sin(p/2),0,0,math.cos(p/2))),
                   (0,0,math.sin(r/2),math.cos(r/2)))
    rows=[];raw_mismatches=0
    for yaw in (-math.pi,-math.pi/2,-.4,0,.7,math.pi/2,math.pi):
        for pitch in (-math.pi/2,-1.55,-.8,0,.8,1.55,math.pi/2):
            for stick in ((0,1),(1,0),(.3,.7),(0,0)):
                expected=(stick[0]*math.cos(yaw)+stick[1]*math.sin(yaw),0,-stick[0]*math.sin(yaw)+stick[1]*math.cos(yaw))
                actual=move(quat(yaw,pitch,0),stick)
                error=max(abs(x-y) for x,y in zip(actual,expected))
                rows.append(dict(name='roll_free_native_ground',yaw=yaw,pitch=pitch,stick=stick,
                    status='PASS' if all(math.isfinite(x) for x in actual) and error<2e-6 else 'FAIL',max_error=error))
                # Nonzero roll is a negative control, not a production input.
                raw=move(quat(yaw,pitch,.6),stick)
                raw_mismatches+=max(abs(x-y) for x,y in zip(raw,expected))>1e-3
    rows.append(dict(name='negative_control_physical_roll_changes_ground_heading',
                     status='PASS' if raw_mismatches>50 else 'FAIL',mismatches=raw_mismatches))
    if a.adapter_vectors:
        vectors=json.loads(a.adapter_vectors.read_text())
        if not 0<len(vectors)<=4096:raise ValueError('unexpected walking adapter vector count')
        for index,vector in enumerate(vectors):
            actual=move(vector['aim_q'],vector['stick']);expected=vector['movement']
            error=max(abs(x-y) for x,y in zip(actual,expected))
            rows.append(dict(name='private_walking_input_native_ground',index=index,
                             status='PASS' if all(math.isfinite(x) for x in actual) and error<2e-6 else 'FAIL',max_error=error))
    if e.calls:raise ValueError('unexpected external math substitution')
    report=dict(exe_sha256=SHA256,scope=__doc__,checks=rows,
                passed=sum(r['status']=='PASS' for r in rows),failed=sum(r['status']=='FAIL' for r in rows))
    a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
    print('PASS',report['passed'],'FAIL',report['failed'],'raw roll mismatches',raw_mismatches)
    return int(bool(report['failed']))


if __name__=='__main__':raise SystemExit(main())
