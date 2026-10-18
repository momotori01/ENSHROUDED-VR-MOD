"""Held-controller axial twists, then existing single calibrated native swing. No game launch."""
import argparse,json,math
from pathlib import Path
from generate_two_hand_fixture import generate as base
from check_recorded_xr_native import rotate,multiply,normalize
from grip_preview_math import calibrated

def angles(q):
 x,y,z=[rotate(q,v) for v in ([1,0,0],[0,1,0],[0,0,1])]
 return [math.degrees(math.atan2(z[0],z[2])),math.degrees(math.asin(max(-1,min(1,-z[1])))),math.degrees(math.atan2(x[1],y[1]))]

def generate(profile):
 seq=base(profile);right=seq[0][14:17];left=seq[0][1:4]
 axis=[a-b for a,b in zip(left,right)];n=math.sqrt(sum(x*x for x in axis));axis=[x/n for x in axis]
 for frame in range(428,1001):
  row=seq[frame-1];row[11]=1
  dx=.045*min(frame-519,6) if frame>=520 else 0
  row[14:17]=[right[0]+dx,*right[1:]];row[1:4]=[left[0]+dx,*left[1:]]
  theta=math.radians(30)*math.sin((frame-430)*math.pi/10) if 430<=frame<=490 else 0
  q=[x*math.sin(theta/2) for x in axis]+[math.cos(theta/2)]
  row[17:20]=angles(q);row[4:7]=angles(multiply(q,profile['right_to_left_orientation_XR']))
 return seq

def verify(path,profile):
 seq=[[float(x) for x in l.split(',')] for l in path.read_text().splitlines() if l and not l.startswith('#')]
 expected=generate(profile)
 if len(seq)!=1000 or any(len(a)!=29 for a in seq):raise ValueError('bounded controller fixture shape')
 if any(abs(a-b)>1e-12 for row,exp in zip(seq,expected) for a,b in zip(row,exp)):raise ValueError('unapproved controller speed fixture mutation')
 gaps=[]
 for f in range(430,491):
  row=seq[f-1];prev=seq[f-2];p,q,_=calibrated(row,profile);off=[a+b for a,b in zip(p,rotate(q,profile['off_hand_in_scaled_model_XR']))]
  if row[1:4]!=prev[1:4] or row[14:17]!=prev[14:17] or row[11]!=1 or row[24]!=1:raise ValueError('twist contains translation or unheld controller')
  gaps.append(math.dist(off,row[1:4]))
 if max(gaps)>=.035 or len({tuple(seq[f-1][17:20]) for f in range(430,491)})<15:raise ValueError('no valid held wrist twist')
 return dict(poses=1000,twist_frames=61,max_support_gap=max(gaps),translation_attack_frame=520)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('profile',type=Path);p.add_argument('--output',type=Path);p.add_argument('--verify',type=Path);a=p.parse_args();profile=json.loads(a.profile.read_text())
 if a.output:
  if a.output.exists():raise ValueError('Refusing to overwrite archived fixture')
  a.output.write_text('# Both grips held; positions fixed and wrists twist430..490 after warmup; native swing only520.\n'+''.join(','.join(map(str,r))+'\n' for r in generate(profile)))
 if a.verify:print('PASS',verify(a.verify,profile))
 if not a.output and not a.verify:p.error('Choose output or verify')
if __name__=='__main__':main()
