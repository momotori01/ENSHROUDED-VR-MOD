"""Isolate the complete native Crosshair transform, including event offsets.

Execute local, hash-verified game instructions in Unicorn only. No game process,
ammo, player component, syscall or executable modification. Random angle ranges
are degenerate (min=max); nonzero random radius and random sampling are excluded.
The PE's scalar sin/cos implementation executes unchanged with default CPU flags.
"""
import json
import math
import pathlib
import struct
from emulate_camera_math import CameraMath, SHA256
from check_recorded_xr_native import rotate, normalize
from unicorn.x86_const import UC_X86_REG_R9, UC_X86_REG_RAX


def product(a, b):
    x,y,z,w=a; X,Y,Z,W=b
    return [w*X+x*W+y*Z-z*Y, w*Y-x*Z+y*W+z*X,
            w*Z+x*Y-y*X+z*W, w*W-x*X-y*Y-z*Z]


def axis(index, degrees):
    q=[0.,0.,0.,math.cos(math.radians(degrees)/2)]
    q[index]=math.sin(math.radians(degrees)/2)
    return q


def main():
    engine=CameraMath(pathlib.Path('enshrouded.exe')); cpu=engine.cpu
    # All fragments manually inspected in spawn_offset_*_paths. Narrow RNG range
    # deliberately rejects the min<max branch instead of substituting a PRNG.
    engine.allowed += [(0x1e4460,0x1e45fb), (0x1e3220,0x1e3365),
                       (0x1e3020,0x1e30fa), (0x334950,0x334959),
                       (0x87c570,0x87c68b), (0x87cf80,0x87d087),
                       (0x87d1f0,0x87d2c6), (0x1dfe90,0x1dffde),
                       (0xd69930,0xd69a88), (0x1235310,0x123580f),
                       (0x1235810,0x1235f06)]
    fixtures=[
        dict(name='identity', local=[0,0,0],world=[0,0,0],q=[0,0,0,1],angles=[0,0,0]),
        dict(name='local_muzzle',local=[.125,-.25,.5],world=[0,0,0],q=axis(1,15),angles=[0,0,0]),
        dict(name='world_offset',local=[0,0,0],world=[-.5,.25,.125],q=axis(0,-10),angles=[5,0,0]),
        dict(name='combined',local=[.125,-.25,.5],world=[-.5,.25,.125],q=axis(2,20),angles=[5,-10,15]),
    ]
    vectors=json.loads(pathlib.Path('docs/analysis/input_math_vectors.json').read_text())
    results=[];max_position=max_orientation=0.;negative_local_as_world=False;negative_ignored_angles=False
    out,context,inp,event=engine.data,engine.data+0x1000,engine.data+0x2000,engine.data+0x4000
    for i,vector in enumerate(vectors):
        q=struct.unpack('<4f',struct.pack('<4f',*vector['aim_q']))
        p=(16064195854336+i*14316557,3622804914176-i*735001,6133436121088+i*984321)
        for distance in (0.,1.538461566,4.):
            source=bytearray(1320);struct.pack_into('<3q4ff',source,0x28,*p,*q,distance)
            native_distance=struct.unpack_from('<f',source,0x50)[0]
            for fixture in fixtures:
                data=bytearray(336)
                struct.pack_into('<10f',data,0xc0,*fixture['local'],*fixture['world'],*fixture['q'])
                for n,value in enumerate(fixture['angles']):struct.pack_into('<2f',data,0x10c+n*8,value,value)
                cpu.mem_write(out,b'\xcc'*56);cpu.mem_write(context,b'\0'*0x200)
                cpu.mem_write(context+0x88,struct.pack('<Q',inp));cpu.mem_write(inp,bytes(source));cpu.mem_write(event,bytes(data))
                cpu.reg_write(UC_X86_REG_R9,event)
                engine.invoke(0x1e4460,out,0,context)
                raw=bytes(cpu.mem_read(out,56));actual_p=struct.unpack_from('<3q',raw)
                actual_q=struct.unpack_from('<4f',raw,24);scale=struct.unpack_from('<3f',raw,40)
                local=rotate(q,fixture['local']);forward=rotate(q,[0,0,1])
                expected_p=[p[j]/2**32+forward[j]*native_distance+local[j]+fixture['world'][j] for j in range(3)]
                # ds Euler quaternion uses negative Y-Z-X composition.
                x,y,z=fixture['angles'];spread=product(product(axis(1,-y),axis(2,-z)),axis(0,-x))
                offset_q=struct.unpack_from('<4f',data,0xd8)
                expected_q=product(product(q,offset_q),spread)
                pe=max(abs(actual_p[j]/2**32-expected_p[j]) for j in range(3))
                qe=min(max(abs(actual_q[j]-sign*expected_q[j]) for j in range(4)) for sign in (-1,1))
                preserved=bytes(cpu.mem_read(inp,len(source)))==bytes(source) and bytes(cpu.mem_read(event,len(data)))==bytes(data)
                ok=pe<3e-6 and qe<3e-6 and scale==(1.,1.,1.) and preserved and cpu.reg_read(UC_X86_REG_RAX)&255==1
                max_position=max(max_position,pe);max_orientation=max(max_orientation,qe)
                results.append(dict(vector=i,distance=distance,fixture=fixture['name'],status='PASS' if ok else 'FAIL',position_error_m=pe,quaternion_error=qe))
                negative_local_as_world |= max(abs(local[j]-fixture['local'][j]) for j in range(3))>.1
                base=product(q,offset_q)
                negative_ignored_angles |= max(abs(a-b) for a,b in zip(rotate(normalize(expected_q),[0,0,1]),rotate(normalize(base),[0,0,1])))>.05
    checks=dict(complete_crosshair_event_transform=all(e['status']=='PASS' for e in results),
                no_external_substitutions=not engine.calls,
                negative_local_as_world_distinguishable=negative_local_as_world,
                negative_ignored_spread_distinguishable=negative_ignored_angles)
    report=dict(exe_sha256=SHA256,scope=__doc__,native_cases=len(results),max_position_error_m=max_position,
                max_quaternion_error=max_orientation,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},fixtures=fixtures,cases=results)
    pathlib.Path('docs/analysis/emulated_crosshair_offsets.json').write_text(json.dumps(report,indent=2,allow_nan=False),encoding='utf8')
    print(report['checks']);print('Native cases',len(results),'position error',max_position,'quaternion error',max_orientation)
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
