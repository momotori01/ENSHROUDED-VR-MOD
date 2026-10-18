"""Explicit synthetic body/hand calibration for native guard verification. No game launch."""
import argparse,json,math
from pathlib import Path
from generate_two_hand_fixture import generate as base
from grip_preview_math import calibrated
from check_recorded_xr_native import rotate
PROFILE='EVR_SYNTHETIC_GUARD_1 0 -0.65 0 1 0.15 0.10 0.30 0.20 0.04 0.60 0.08 12 30 100000000 100000000 0.035\n'
def generate(profile,delay=0):
    seed=base(profile)[498];local=[seed[i]-seed[i+13] for i in (1,2,3)];rows=[]
    # Start INSIDE: must not inject LB until actual lowering and re-entry.
    keys=[(1,.50,1.25),(60,.50,1.25),(90,0,.80),(100,0,.80),(125,.50,1.25),(150,.50,1.25),
          (175,0,.80),(195,0,.80),(230,0,1.90),(255,0,1.90),(290,0,.80),(1000,0,.80)]
    for f in range(1,1001):
        for (fa,a,y),(fb,b,z) in zip(keys,keys[1:]):
            if fa<=f<=fb:
                t=(f-fa)/(fb-fa);front=a+(b-a)*t;height=y+(z-y)*t;break
        left=[.2,height,-front];right=[x-d for x,d in zip(left,local)];row=seed.copy();row[0]=f;row[1:4]=left;row[14:17]=right;rows.append(row)
    if delay:
        rows=[[f,*rows[max(1,f-delay)-1][1:]] for f in range(1,1001)]
    return rows

def verify(path,profile,delay=0):
    rows=[[float(v) for v in l.split(',')] for l in path.read_text().splitlines() if l and not l.startswith('#')];expected=generate(profile,delay)
    if len(rows)!=1000 or any(len(r)!=29 for r in rows) or any(abs(x-y)>1e-12 for r,e in zip(rows,expected) for x,y in zip(r,e)):raise ValueError('unexpected guard pose sequence')
    gaps=[]
    for row in rows:
        p,q,_=calibrated(row,profile);off=[a+b for a,b in zip(p,rotate(q,profile['off_hand_in_scaled_model_XR']))];gaps.append(math.dist(off,row[1:4]))
    if max(gaps)>1e-6:raise ValueError('two-hand support mismatch')
    return dict(poses=1000,max_support_gap=max(gaps),synthetic_head_to_navel=[0,-.65,0],native_state_writes=False)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--delay-frames',type=int,default=0);p.add_argument('--output',type=Path);p.add_argument('--verify',type=Path);a=p.parse_args();profile=json.loads(a.profile.read_text())
    if a.output:
        if a.output.exists():raise ValueError('Do not overwrite a retained fixture')
        a.output.write_text('# Synthetic calibrated body; initial inside ignored, front guard, overhead guard, neutral.\n'+''.join(','.join(map(str,r))+'\n' for r in generate(profile,a.delay_frames)))
    if a.verify:print('PASS',verify(a.verify,profile,a.delay_frames))
if __name__=='__main__':main()
