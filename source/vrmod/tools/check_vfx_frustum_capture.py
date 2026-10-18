"""Check captured VfxUpdateBuffer planes against submitted native stereo corners.

Requires a known VFX dispatch exported by evr_renderdoc buffers in the final
captured game frame. It checks real GPU constants, not effect draw coverage or
all shader variants. Full-world origin changes need separate captures.
"""
import argparse
import hashlib
import json
import math
import struct
from pathlib import Path


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe',type=Path)
    parser.add_argument('bindings',type=Path)
    parser.add_argument('event',type=int)
    parser.add_argument('--native-oracle',action='store_true',help='Reproduce unmodified native frustum math in isolated Unicorn')
    args=parser.parse_args()
    native=[json.loads(s) for s in (args.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    bindings=[json.loads(s) for s in args.bindings.read_text().splitlines()]
    matches=[e for e in bindings if e['event']==args.event and e['stage']==5 and e['category']=='constant' and e['index']==1 and e['size']==848]
    if len(matches)!=1:raise ValueError('expected one 848-byte VfxUpdateBuffer at compute constant slot 1')
    raw=(args.bindings.parent/matches[0]['file']).read_bytes()
    if len(raw)!=848:raise ValueError('incomplete uniform export')
    center=[e for e in native if e['event']=='world_shared_culling_camera'][-1]
    serial,frame=center['serial'],center['frame']
    complete=[e for e in native if e['event']=='live_native_pair_complete'][-1]
    eyes={e['eye']:e for e in native if e['event']=='calibration_eye_transform' and e['serial']==serial}
    applied={e['eye']:e for e in native if e['event']=='recorded_xr_eye_applied' and e['serial']==serial}
    anchor=next(e for e in native if e['event']=='recorded_xr_anchor' and e['recorded_xr_frame']==frame)
    planes=[struct.unpack_from('<4f',raw,192+i*16) for i in range(5)]
    gpu_camera=struct.unpack_from('<3f',raw,128)
    camera=[x/2**32 for x in center['camera_q32']]
    camera_error=max(abs(x-y) for x,y in zip(camera,gpu_camera))
    checks={'last_submitted_frame_has_both_native_eyes':complete['serial']==serial and complete['frame']==frame and set(eyes)==set(applied)=={0,1},
            'gpu_vfx_camera_matches_shared_camera':camera_error<.001,
            'gpu_vfx_planes_finite_and_unit':all(math.isfinite(v) for p in planes for v in p) and max(abs(sum(x*x for x in p[:3])-1) for p in planes)<2e-6}
    maxima=[-math.inf]*5;corners=[]
    for eye in (0,1):
        m=eyes[eye]['view_matrix'];position=[x/2**32 for x in eyes[eye]['camera_q32']]
        for depth in (anchor['near'],anchor['far']):
            for x in map(math.tan,applied[eye]['fov'][:2]):
                for y in map(math.tan,applied[eye]['fov'][2:]):
                    ray=[x*depth,y*depth,depth]
                    world=[position[c]+sum(m[c*4+k]*ray[k] for k in range(3)) for c in range(3)]
                    distances=[sum(a*b for a,b in zip(p[:3],world))+p[3] for p in planes]
                    maxima=[max(a,b) for a,b in zip(maxima,distances)]
                    corners.append(dict(eye=eye,depth=depth,world=world,distances=distances))
    # Independent logged pose/matrix rounding is not zero. This 1mm reporting
    # tolerance is much smaller than the prior 7.70m far-corner failure.
    checks['gpu_vfx_planes_enclose_stereo_corners']=max(maxima)<=.001
    # f9226 is the world-space frustum; f9240 is camera-centered (w near zero).
    # The VFX update uniform consumes the former, verified by all 20 floats.
    stable=[e for e in native if e['event']=='shared_stable_frustum' and e['serial']==serial and e['caller']=='0xf9226']
    if stable:
        checks['gpu_vfx_planes_equal_hook_output']=len(stable)==1 and b''.join(struct.pack('<4f',*p) for p in stable[0]['planes'][:5])==raw[192:272]
    report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},frame=frame,serial=serial,event=args.event,
                shader_sha256=hashlib.sha256((args.bindings.parent/f'event_{args.event}_stage_5.spv').read_bytes()).hexdigest(),
                maximum_corner_distances_m=maxima,camera_error_m=camera_error,planes=planes,corners=corners,limitation=__doc__)
    if args.native_oracle:
        from emulate_frustum_math import FrustumMath
        engine=FrustumMath(Path('enshrouded.exe'))
        l,r,t,b=map(math.tan,center['fov']);n=center['near'];descriptor=bytearray(80)
        struct.pack_into('<I6f',descriptor,0,3,l*n,r*n,b*n,t*n,n,center['far']);descriptor[72]=1
        engine.cpu.mem_write(engine.data+0x400,struct.pack('<16f',*center['view_matrix']))
        engine.cpu.mem_write(engine.data+0x500,bytes(descriptor))
        engine.invoke(0xd647e0,engine.data+0x600,engine.data+0x400,engine.data+0x500)
        output=bytes(engine.cpu.mem_read(engine.data+0x600,96))
        report['unmodified_native_matches_gpu_exactly']=output[:80]==raw[192:272]
        report['unmodified_native_planes']=[struct.unpack_from('<4f',output,16*i) for i in range(6)]
    (args.bindings.parent/'vfx-frustum-checks.json').write_text(json.dumps(report,indent=2,allow_nan=False))
    for name,status in report['checks'].items():print(status,name)
    print('maximum corner distances (m)',maxima)
    print('PASS',sum(checks.values()),'FAIL',len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
