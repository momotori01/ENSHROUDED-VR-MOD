"""Rebuild bounded two-hand mock poses from the measured grip profile; never launch a game."""
import argparse,json,math
from pathlib import Path
from check_recorded_xr_native import rotate,normalize

HEADER='# visual calibration + one native attack gate: one-hand swipe390, too-far support415, qualified swipe440; no trigger/sticks\n'
def generate(profile):
 q=normalize(profile['right_to_left_orientation_XR']);axes=[rotate(q,v) for v in ([1,0,0],[0,1,0],[0,0,1])]
 angles=[math.degrees(math.atan2(axes[2][0],axes[2][2])),math.degrees(math.asin(max(-1,min(1,-axes[2][1])))),math.degrees(math.atan2(axes[0][1],axes[1][1]))]
 local=[x+y for x,y in zip(profile['grip_to_model_XR']['p'],rotate(profile['grip_to_model_XR']['q'],profile['off_hand_in_scaled_model_XR']))];span=math.sqrt(sum(x*x for x in local));axis=[x/span for x in local];rows=[]
 for f in range(1,1001):
  x=.22
  if 390<=f<=400:x=.22+.045*min(f-389,6)
  elif 401<=f<=414:x=.49+(.22-.49)*(f-400)/14
  elif 415<=f<=420:x=.22+.045*(f-414)
  elif 421<=f<=433:x=.49+(.22-.49)*(f-420)/13
  elif f>=440:x=.22+.045*min(f-439,6)
  distance=.2 if 415<=f<=423 else .2*max(0,1-(f-423)/4) if 424<=f<=427 else 0
  right=[x,1.28,-.88];left=[r+d+distance*a for r,d,a in zip(right,local,axis)]
  rows.append([f,*left,*angles,0,0,0,0,int(f>=415),1,1,*right,0,0,0,0,0,0,0,1,1,1,1,0])
 return rows

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--output',type=Path);p.add_argument('--verify',type=Path);a=p.parse_args();profile=json.loads(a.profile.read_text());data=generate(profile);text=HEADER+''.join(','.join(map(str,row))+'\n' for row in data)
 if a.output:
  if a.output.exists():raise ValueError('Refusing to overwrite an existing fixture; use a new output path')
  a.output.write_text(text)
 if a.verify:
  src=[[float(v) for v in l.split(',')] for l in a.verify.read_text().splitlines() if l and not l.startswith('#')]
  assert len(src)==len(data)==1000 and all(len(r)==29 for r in src+data)
  error=max(abs(x-y) for r,s in zip(src,data) for x,y in zip(r,s));assert error<1e-12,error
  print('PASS 1000 poses numerically reproduced; max error=',error,'byte_identical=',a.verify.read_text()==text)
 if not a.output and not a.verify:p.error('Choose --output or --verify')
if __name__=='__main__':main()
