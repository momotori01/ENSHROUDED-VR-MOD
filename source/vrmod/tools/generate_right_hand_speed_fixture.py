"""Calibrated left-only swings rejected; right-only swings qualify. No game launch."""
import argparse,json,math
from pathlib import Path
from generate_controller_speed_fixture import generate as base
from check_recorded_xr_native import rotate
from grip_preview_math import calibrated

def arc(pivot,point,angle):
 q=[0,math.sin(angle/2),0,math.cos(angle/2)]
 return [a+b for a,b in zip(pivot,rotate(q,[a-b for a,b in zip(point,pivot)]))]
def generate(profile):
 seq=base(profile);left=seq[498][1:4];right=seq[498][14:17]
 for f in range(500,1001):
  row=seq[f-1];row[1:4]=left;row[14:17]=right
  if f<=511:row[1:4]=arc(right,left,.065*min(f-499,512-f))
  if f>=520:row[14:17]=arc(left,right,.065*min(f-519,6))
 return seq

def verify(path,profile):
 seq=[[float(x) for x in l.split(',')] for l in path.read_text().splitlines() if l and not l.startswith('#')];expected=generate(profile)
 if len(seq)!=1000 or any(len(r)!=29 for r in seq) or any(abs(a-b)>1e-12 for r,e in zip(seq,expected) for a,b in zip(r,e)):raise ValueError('unapproved right-hand fixture')
 gaps=[];left_speed=[];right_speed=[]
 for f in list(range(500,513))+list(range(520,527)):
  row=seq[f-1];p,q,_=calibrated(row,profile);off=[a+b for a,b in zip(p,rotate(q,profile['off_hand_in_scaled_model_XR']))];gaps.append(math.dist(off,row[1:4]))
  vl=math.dist(row[1:4],seq[f-2][1:4])*90;vr=math.dist(row[14:17],seq[f-2][14:17])*90
  if f<=512:
   if vr!=0:raise ValueError('right moved during left-only phase')
   left_speed.append(vl)
  else:
   if vl!=0:raise ValueError('left moved during right-only phase')
   right_speed.append(vr)
 if max(gaps)>=.035 or min(v for i,v in enumerate(left_speed) if i!=6)<2 or min(right_speed[:6])<2 or max(left_speed+right_speed)>12:raise ValueError('invalid held single-controller swing')
 return dict(poses=1000,max_support_gap=max(gaps),left_only_frames=[500,512],left_speed_range=[min(left_speed),max(left_speed)],right_only_attack_frame=520,right_speed_range=[min(right_speed[:6]),max(right_speed)])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--output',type=Path);p.add_argument('--verify',type=Path);a=p.parse_args();profile=json.loads(a.profile.read_text())
 if a.output:
  if a.output.exists():raise ValueError('Do not overwrite archived fixture')
  a.output.write_text('# Held twists430..490; LEFT ONLY swing500..512 rejected; RIGHT ONLY swing520 qualifies.\n'+''.join(','.join(map(str,r))+'\n' for r in generate(profile)))
 if a.verify:print('PASS',verify(a.verify,profile))
if __name__=='__main__':main()
