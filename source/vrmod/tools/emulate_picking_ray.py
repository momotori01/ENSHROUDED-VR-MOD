"""Execute the native PickingTarget ray construction in an isolated CPU.

Starts at 3d68f0 after target bookkeeping, ends before the ECS query-system
lookup at 3d6ab1. No query is submitted and no collision result is simulated.
Original float world-coordinate conversion is preserved, including its existing
precision loss. Inputs cover the C++ walking adapter and independent roll poses.
"""
import json, math, pathlib, struct
from emulate_camera_math import CameraMath, SHA256
from check_recorded_xr_native import rotate
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RBP, UC_X86_REG_RIP,
    UC_X86_REG_R13, UC_X86_REG_XMM7, UC_X86_REG_XMM8, UC_X86_REG_XMM9,
    UC_X86_REG_XMM10, UC_X86_REG_XMM11, UC_X86_REG_XMM12, UC_X86_REG_XMM13,
    UC_X86_REG_XMM14, UC_X86_REG_MXCSR)


def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def packed(fmt,*values):return int.from_bytes(struct.pack(fmt,*values),'little')


def main():
    e=CameraMath(pathlib.Path('enshrouded.exe'));cpu=e.cpu
    e.allowed += [(0x3d68f0,0x3d6ab1),(0xd66870,0xd668eb),(0xd6ec00,0xd6ecc2)]
    # Read the constants loaded by the preceding native prologue, rather than
    # replacing its tiny zero-vector epsilon with an arbitrary tolerance.
    constant=lambda rva,n=4:int.from_bytes(cpu.mem_read(e.base+rva,n),'little')
    walking=json.loads(pathlib.Path('docs/analysis/walking_input_vectors.json').read_text())
    original=json.loads(pathlib.Path('docs/analysis/input_math_vectors.json').read_text())
    vectors=[v['aim_q'] for v in walking+original]
    rows=[];max_origin=max_forward=0.;max_float_forward=0.
    for i,q in enumerate(vectors):
        q=struct.unpack('<4f',struct.pack('<4f',*q));forward=rotate(q,[0,0,1])
        p=tuple(round(x*2**32) for x in (3740.125+i/1024,843.5-i/2048,1428.125+i/4096))
        for distance in (0.,1.538461566,4.):
            sp=e.stack+0xc000;bp=sp+0x100;inp=e.data+0x2000
            source=bytearray(1320);struct.pack_into('<3q4ff',source,0x28,*p,*q,distance)
            distance=struct.unpack_from('<f',source,0x50)[0]
            cpu.mem_write(sp,b'\0'*0x1000);cpu.mem_write(inp,bytes(source))
            cpu.mem_write(sp+0x58,struct.pack('<Q',inp));cpu.mem_write(bp+0xc0,struct.pack('<4f',*q))
            regs=[(UC_X86_REG_RSP,sp),(UC_X86_REG_RBP,bp),(UC_X86_REG_R13,0),
                  (UC_X86_REG_XMM7,packed('<q',p[2])),(UC_X86_REG_XMM10,packed('<2q',*p[:2])),
                  (UC_X86_REG_XMM8,constant(0x12aa050)),(UC_X86_REG_XMM9,constant(0x12aa3c8)),
                  (UC_X86_REG_XMM11,0),(UC_X86_REG_XMM12,constant(0x12ab320)),
                  (UC_X86_REG_XMM13,constant(0x12ad420)),
                  (UC_X86_REG_XMM14,constant(0x12aa040,16)),(UC_X86_REG_MXCSR,0x1f80)]
            for reg,value in regs:cpu.reg_write(reg,value)
            cpu.emu_start(e.base+0x3d68f0,e.base+0x3d6ab1,timeout=1_000_000,count=2048)
            if cpu.reg_read(UC_X86_REG_RIP)!=e.base+0x3d6ab1:raise ValueError('unexpected fragment stop')
            transform=bytes(cpu.mem_read(bp+0x50,56));actual_p=struct.unpack_from('<3q',transform)
            ray=struct.unpack('<3f',cpu.mem_read(bp-0x70,12))
            endpoints=struct.unpack('<8f',cpu.mem_read(bp,32))
            expected_origin=[p[j]/2**32+distance*forward[j] for j in range(3)]
            # d6ec00 normalizes the float endpoint difference, not the original
            # basis. Preserve and report that native precision characteristic.
            delta=[endpoints[j+4]-endpoints[j] for j in range(3)]
            length=math.sqrt(sum(x*x for x in delta));expected_ray=[x/length for x in delta]
            pe=max(abs(actual_p[j]/2**32-expected_origin[j]) for j in range(3))
            re=max(abs(ray[j]-expected_ray[j]) for j in range(3))
            fe=max(abs(ray[j]-forward[j]) for j in range(3))
            max_origin=max(max_origin,pe);max_forward=max(max_forward,re);max_float_forward=max(max_float_forward,fe)
            preserved=bytes(cpu.mem_read(inp,len(source)))==bytes(source)
            ok=(pe<.001 and re<3e-7 and fe<.001 and preserved and
                struct.unpack_from('<4f',transform,24)==(0,0,0,1) and struct.unpack_from('<3f',transform,40)==(1,1,1))
            rows.append(dict(vector=i,distance=distance,status='PASS' if ok else 'FAIL',origin_error_m=pe,
                             normalized_endpoint_error=re,native_forward_quantization_error=fe))
    checks=dict(all_native_cases=all(r['status']=='PASS' for r in rows),no_external_substitutions=not e.calls)
    report=dict(scope=__doc__,exe_sha256=SHA256,native_cases=len(rows),checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},
                max_origin_error_m=max_origin,max_endpoint_direction_error=max_forward,
                max_native_forward_quantization_error=max_float_forward,cases=rows)
    pathlib.Path('docs/analysis/emulated_picking_ray.json').write_text(json.dumps(report,indent=2,allow_nan=False)+'\n',encoding='utf8')
    print(report['checks']);print(len(rows),'cases; errors',max_origin,max_forward,max_float_forward)
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
