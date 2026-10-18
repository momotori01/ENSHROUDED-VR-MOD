"""Generate a looping, frame-indexed wide-angle pose for bounded native world tests.

The mock consumes one CSV row per xrWaitFrame, including warmup. A 240-row loop
therefore covers every key direction in any 300 submitted frames. Resolution is
the test runtime's recommendation, not a game/HMD hardcoded output size.
"""
import argparse
import csv
import math
from pathlib import Path
from pose_sequences import row


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--output',type=Path,default=Path('vrmod/test_env/world-wide-pose.csv'))
    p.add_argument('--vertical-pitch',action='store_true',help='Hold exact +/-90 degree pitch while varying physical roll; writes world-vertical-pose.csv by default')
    a=p.parse_args()
    keys=[(0,0,0),(90,0,0),(180,0,0),(270,0,0),(360,80,0),
          (360,-80,0),(360,0,25),(360,0,-25),(360,0,0)]
    if a.vertical_pitch:
        if a.output==Path('vrmod/test_env/world-wide-pose.csv'):a.output=Path('vrmod/test_env/world-vertical-pose.csv')
        keys=[(0,0,0),(90,0,25),(180,90,25),(180,90,-25),(270,-90,-25),
              (270,-90,25),(360,0,25),(360,0,-25),(360,0,0)]
    a.output.parent.mkdir(parents=True,exist_ok=True)
    with a.output.open('w',newline='') as f:
        f.write('# time,x,y,z,yaw,pitch,roll,ipd,tracked,connected,width,height\n')
        writer=csv.writer(f)
        for frame in range(240):
            segment,t=divmod(frame,30);t/=30
            yaw,pitch,roll=[x+(y-x)*t for x,y in zip(keys[segment],keys[segment+1])]
            phase=frame*math.tau/240
            writer.writerow(row(frame/90,x=.15*math.sin(phase),y=1.65+.08*math.sin(2*phase),
                                 z=.15*(math.cos(phase)-1),yaw=yaw,pitch=pitch,roll=roll))
    print('Generated 240 looping wide-angle/6DoF rows:',a.output)


if __name__=='__main__':main()
