"""Bounded head gesture/combat interlock; no menu buttons or native gameplay writes."""
import argparse,json,math
from pathlib import Path
from generate_controller_speed_fixture import angles
from check_recorded_xr_native import rotate

def generate(profile):
 q=profile['right_to_left_orientation_XR'];a=angles(q);grip=profile['grip_to_model_XR']
 offset=[x+y for x,y in zip(grip['p'],rotate(grip['q'],profile['off_hand_in_scaled_model_XR']))];seq=[]
 for f in range(1,1001):
  x=.5
  if 380<=f<=420:x=.5-(.5-.285)*(f-380)/40
  elif 421<=f<440:x=.285
  elif 440<=f<=479:x=.22+(.02 if f%2 else -.02) if 455<=f<=475 else .24
  elif f==480:x=.30
  elif 481<=f<=489:x=.36+.045*(f-481)
  elif 490<=f<520:x=.72
  elif f>=520:x=.72+.045*min(f-519,6)
  right=[x,1.65,0];left=[p+d for p,d in zip(right,offset)]
  seq.append([f,*left,*a,0,0,0,0,1,1,1,*right,0,0,0,0,0,0,0,1,1,1,1,0])
 return seq

def verify(path,profile):
 seq=[[float(v) for v in l.split(',')] for l in path.read_text().splitlines() if l and not l.startswith('#')];expected=generate(profile)
 if len(seq)!=1000 or any(len(r)!=29 for r in seq):raise ValueError('head combat fixture shape')
 if any(abs(a-b)>1e-12 for r,s in zip(seq,expected) for a,b in zip(r,s)):raise ValueError('head combat fixture mutated')
 speeds=[math.dist(seq[f-1][14:17],seq[f-2][14:17])*90 for f in range(2,1001)]
 if max(speeds)>12 or speeds[438]<2 or speeds[518]<2:raise ValueError('expected entry and post-neutral swing speed')
 return dict(poses=1000,entry_candidate=440,active_from=448,leaves=481,post_neutral_swing=520,max_controller_speed=max(speeds))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--output',type=Path);p.add_argument('--verify',type=Path);a=p.parse_args();profile=json.loads(a.profile.read_text())
 if a.output:
  if a.output.exists():raise ValueError('refuse archived fixture replacement')
  a.output.write_text('# Slow approach, fast entry440, fast motions while reserved, fast exit481, neutral then one swing520.\n'+''.join(','.join(map(str,r))+'\n' for r in generate(profile)))
 if a.verify:print('PASS',verify(a.verify,profile))
 if not a.output and not a.verify:p.error('Choose output or verify')
if __name__=='__main__':main()
