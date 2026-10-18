"""Independent scalar reference for calibrated rigid weapon pose and CSV fixture."""
import math
from check_recorded_xr_native import rotate,multiply,normalize
from prepare_grip_preview import unit,cross

def euler_q(v):
 y,p,r=[math.radians(x)/2 for x in v]
 return multiply(multiply([0,math.sin(y),0,math.cos(y)],[math.sin(p),0,0,math.cos(p)]),[0,0,math.sin(r),math.cos(r)])
def calibrated(row,profile):
 main=row[14:27];other=row[1:14];rq=euler_q(main[3:6]);q=normalize(multiply(rq,profile['grip_to_model_XR']['q']));p=[a+b for a,b in zip(main[:3],rotate(rq,profile['grip_to_model_XR']['p']))];both=bool(main[10] and other[10] and main[11] and other[11] and main[12] and other[12])
 if both:
  a=unit([u+v-w for u,v,w in zip(p,rotate(q,profile['off_hand_in_scaled_model_XR']),main[:3])]);b=unit([u-v for u,v in zip(other[:3],main[:3])]);d=max(-1.,min(1.,sum(x*y for x,y in zip(a,b))));axis=cross(a,b);sine=math.sqrt(sum(v*v for v in axis))
  # atan2 remains stable when acos(dot) rounds away a tiny rotation.
  # Anatomical unit() rejects tiny vectors, which a parallel grip legitimately produces.
  if sine<1e-12 and d>=0:c=[0,0,0,1]
  elif d<-.99999:
   axis=unit(cross(a,[1,0,0] if abs(a[0])<.8 else [0,1,0]));c=axis+[0]
  else:
   theta=math.atan2(sine,d);c=[v/sine*math.sin(theta/2) for v in axis]+[math.cos(theta/2)]
  p=[u+v for u,v in zip(main[:3],rotate(c,[u-v for u,v in zip(p,main[:3])]))];q=normalize(multiply(c,q))
 return p,q,both
