"""Two measured right-controller swings with a fast return during native recovery.
Frame placement is diagnostic choreography, never a product cooldown.
"""
import argparse,json,math
from pathlib import Path
from generate_right_hand_speed_fixture import generate as base,arc
from grip_preview_math import calibrated
from check_recorded_xr_native import rotate

def generate(profile):
 seq=base(profile);left=seq[498][1:4];right=seq[498][14:17]
 for frame in range(532,1001):
  angle=.065*max(0,6-(frame-531)) if frame<600 else .065*min(frame-599,6)
  seq[frame-1][14:17]=arc(left,right,angle)
 return seq

def verify(path,profile):
 seq=[[float(x) for x in l.split(',')] for l in path.read_text().splitlines() if l and not l.startswith('#')];expected=generate(profile)
 if len(seq)!=1000 or any(len(r)!=29 for r in seq) or any(abs(a-b)>1e-12 for r,e in zip(seq,expected) for a,b in zip(r,e)):raise ValueError('unapproved measured repeat sequence')
 gaps=[];speeds=[]
 for frame in range(428,1001):
  row=seq[frame-1];p,q,_=calibrated(row,profile);off=[a+b for a,b in zip(p,rotate(q,profile['off_hand_in_scaled_model_XR']))];gaps.append(math.dist(off,row[1:4]))
  if frame in [*range(520,526),*range(532,538),*range(600,606)]:
   speed=math.dist(row[14:17],seq[frame-2][14:17])*90;speeds.append(speed)
   if math.dist(row[1:4],seq[frame-2][1:4])!=0 or not 3<speed<4:raise ValueError('right-controller-only swing required')
 if max(gaps)>=.035:raise ValueError('measured support lost')
 return dict(poses=1000,request_frames=[520,600],recovery_swing_frames=[532,537],max_support_gap=max(gaps),right_speed_range=[min(speeds),max(speeds)])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--output',type=Path);p.add_argument('--verify',type=Path);a=p.parse_args();profile=json.loads(a.profile.read_text())
 if a.output:
  if a.output.exists():raise ValueError('Do not overwrite archived fixture')
  a.output.write_text('# Measured right-only attacks520/600; fast return532..537 during native recovery; no queued attack.\n'+''.join(','.join(map(str,r))+'\n' for r in generate(profile)))
 if a.verify:print('PASS',verify(a.verify,profile))
if __name__=='__main__':main()
