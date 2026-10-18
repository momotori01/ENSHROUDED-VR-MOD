"""Verify wide-pose coverage from submitted OpenXR frames, excluding warmup.

This verifies actual Runtime tracking samples used by the native frame path.
It does not certify movement input, aim, collision handling, or perceived comfort.
"""
import argparse
import json
import math
from pathlib import Path
from check_recorded_xr_native import rotate,normalize


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
    native=[json.loads(l) for l in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    start,end=min(e['us'] for e in native),max(e['us'] for e in native)
    xr=[json.loads(l) for l in (a.probe.parent/'native-xr/openxr.jsonl').read_text().splitlines()]
    xr=[e for e in xr if start<=e['us']<=end]
    frames={e['frame'] for e in xr if e['event']=='native_game_pair_submitted'}
    heads=[e for e in xr if e['event']=='hmd' and e['frame'] in frames]
    angles=[]
    for e in heads:
        q=normalize(e['orientation']);f=rotate(q,[0,0,-1]);up=rotate(q,[0,1,0])
        yaw=math.degrees(math.atan2(-f[0],-f[2]));pitch=math.degrees(math.asin(max(-1.,min(1.,f[1]))))
        roll=math.degrees(math.atan2(-up[0],up[1])) if abs(yaw)<1 and abs(pitch)<1 else None
        angles.append((yaw,pitch,roll))
    def distance(a,b):return abs((a-b+180)%360-180)
    checks={'all_submitted_frames_have_pose':bool(frames) and len(heads)==len(frames) and {e['frame'] for e in heads}==frames}
    for yaw in (0,90,-90,180):
        checks[f'horizontal_yaw_{yaw}']=any(distance(y,yaw)<3.1 and abs(p)<8 for y,p,_ in angles)
    checks['look_up_80']=any(p>=79 for _,p,_ in angles)
    checks['look_down_80']=any(p<=-79 for _,p,_ in angles)
    checks['roll_both_directions_25']=any(r is not None and r>=24 for _,_,r in angles) and any(r is not None and r<=-24 for _,_,r in angles)
    spans=[max(e['position'][i] for e in heads)-min(e['position'][i] for e in heads) for i in range(3)] if heads else [0,0,0]
    checks['physical_x_30cm']=spans[0]>=.299
    checks['physical_y_16cm']=spans[1]>=.159
    checks['physical_z_30cm']=spans[2]>=.299
    report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},position_spans_m=spans,
                frames=len(frames),limitation=__doc__)
    (a.probe/'pose-coverage-checks.json').write_text(json.dumps(report,indent=2))
    for k,v in report['checks'].items():print(v,k)
    print('PASS',sum(checks.values()),'FAIL',len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
