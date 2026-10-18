"""Execute native Crosshair spawn camera math before event-specific spawn offsets.

1e4460..1e45c5 reads PlayerInput camera at context+88, producing WorldTransform.
The subsequent 1e3220 event offset/attachment stage is deliberately out of scope.
No projectile, ammo, actor, game process, file modification or syscalls occur.
"""
import json,math,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from check_recorded_xr_native import rotate,normalize
from unicorn.x86_const import UC_X86_REG_RSP,UC_X86_REG_RCX,UC_X86_REG_RDX,UC_X86_REG_R8,UC_X86_REG_R9,UC_X86_REG_RIP


def main():
    engine=CameraMath(pathlib.Path('enshrouded.exe'));engine.allowed.append((0x1e4460,0x1e45c5));cpu=engine.cpu
    vectors=json.loads(pathlib.Path('docs/analysis/input_math_vectors.json').read_text())
    results=[];zero_exact=True;max_position=max_forward=0.
    for i,v in enumerate(vectors):
        for distance in (0.,1.538461566,4.):
            q=struct.unpack('<4f',struct.pack('<4f',*v['aim_q']))
            p=(16064195854336+i*14316557,3622804914176-i*735001,6133436121088+i*984321)
            source=bytearray(1320);struct.pack_into('<3q4ff',source,0x28,*p,*q,distance)
            out,context,inp,event=engine.data,engine.data+0x1000,engine.data+0x2000,engine.data+0x4000
            cpu.mem_write(out,b'\xcc'*56);cpu.mem_write(context,b'\0'*0x200);cpu.mem_write(context+0x88,struct.pack('<Q',inp));cpu.mem_write(inp,bytes(source))
            for reg,value in [(UC_X86_REG_RSP,engine.stack+0xf008),(UC_X86_REG_RCX,out),(UC_X86_REG_RDX,0),(UC_X86_REG_R8,context),(UC_X86_REG_R9,event)]:cpu.reg_write(reg,value)
            cpu.emu_start(engine.base+0x1e4460,engine.base+0x1e45c5,timeout=1000000,count=512)
            if cpu.reg_read(UC_X86_REG_RIP)!=engine.base+0x1e45c5:raise ValueError('crosshair fragment did not reach offset boundary')
            raw=bytes(cpu.mem_read(out,56));position=struct.unpack_from('<3q',raw);orientation=struct.unpack_from('<4f',raw,24);scale=struct.unpack_from('<3f',raw,40)
            native_distance=struct.unpack_from('<f',source,0x50)[0]
            f=rotate(q,[0,0,1]);wanted=[p[j]/2**32+f[j]*native_distance for j in range(3)]
            pe=max(abs(position[j]/2**32-wanted[j]) for j in range(3));fe=max(abs(x-y) for x,y in zip(rotate(normalize(orientation),[0,0,1]),v['forward']))
            preserved=bytes(cpu.mem_read(inp,len(source)))==bytes(source)
            max_position=max(max_position,pe);max_forward=max(max_forward,fe)
            if distance==0:zero_exact &= position==p
            results.append(dict(vector=i,distance=distance,status='PASS' if pe<2e-6 and fe<2e-6 and orientation==q and scale==(1.,1.,1.) and preserved else 'FAIL',position_error_m=pe,forward_error=fe))
    checks=dict(native_camera_transform=all(e['status']=='PASS' for e in results),first_person_distance_zero_exact=zero_exact,no_external_substitutions=not engine.calls)
    report=dict(exe_sha256=SHA256,scope=__doc__,native_calls=len(results),max_position_error_m=max_position,max_forward_error=max_forward,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},cases=results)
    pathlib.Path('docs/analysis/emulated_crosshair_spawn.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print(report['checks']);print('Native cases',len(results),'position error',max_position,'forward error',max_forward)
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
