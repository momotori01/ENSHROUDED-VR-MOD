"""Independent known-axis cases for the melee camera consumer check."""
import math,unittest,struct
from check_motion_attack import native_look_error,native_look_branch

class LookTests(unittest.TestCase):
 def row(self,q,v):return dict(camera_orientation=q,world_look=v)
 def test_axes(self):
  s=math.sqrt(.5)
  for q,v in [([0,0,0,1],[0,0,1]),([0,s,0,s],[1,0,0]),([0,-s,0,s],[-1,0,0]),([0,1,0,0],[0,0,-1]),([s,0,0,s],[0,-1,0]),([-s,0,0,s],[0,1,0]),([0,0,s,s],[0,0,1])]:
   with self.subTest(q=q):
    self.assertLess(native_look_error(self.row(q,v)),2e-6)
    self.assertLess(native_look_error(self.row([-a for a in q],v)),2e-6)
 def test_arbitrary_yaw_and_wrong_axis(self):
  for degrees in (-179,-90,-39.6,-1,0,1,40,90,179):
   a=math.radians(degrees);q=[0,math.sin(a/2),0,math.cos(a/2)];v=[math.sin(a),0,math.cos(a)]
   self.assertLess(native_look_error(self.row(q,v)),2e-6)
   if degrees:self.assertGreater(native_look_error(self.row(q,[0,0,1])),2e-6)
 def test_bad_evidence(self):
  for q,v in [([],[]),([0,0,0,1],[0,1]),([0,0,0,0],[0,0,1]),([0,0,0,2],[0,0,1]),([0,math.nan,0,1],[0,0,1]),([0,0,0,1],[0,math.inf,1]),([0,0,0,1],[0,0,.9])]:
   self.assertGreater(native_look_error(self.row(q,v)),2e-6)
 def test_rounding_and_tolerance(self):
  # Historical body-look observation remains a mismatch; do not widen tolerance.
  self.assertGreater(native_look_error(self.row([0,-.3387377262,0,.9408808351],[-.6373659968,0,.7705612183])),2e-6)
  self.assertGreater(native_look_error(self.row([0,0,0,1],[3e-6,0,1])),2e-6)
 def test_native_branch_contract(self):
  s=math.sqrt(.5);actor=bytearray(3104);body=struct.pack('<3q4f',0,0,0,0,s,0,s)
  c=dict(body_transform_bytes=body.hex(),mode=1,settings=1,post_look=0,world_move=[0,0,0],query_slots=[1,7,15],read_only=True)
  r=dict(camera_orientation=[0,0,0,1],world_look=[1,0,0],base_actor_bytes=actor.hex(),native_look_context=c)
  branch,error=native_look_branch(r);self.assertEqual(branch,'body_337490');self.assertLess(error,2e-6)
  for bit in (25,53):
   a=bytearray(actor);struct.pack_into('<Q',a,0xbf8,1<<bit);r['base_actor_bytes']=a.hex()
   self.assertGreater(native_look_branch(r)[1],2e-6)
  r['base_actor_bytes']=actor.hex();c['mode']=3;r['world_look']=[0,0,1]
  self.assertEqual(native_look_branch(r),('camera_289172',0))
  for key,value in [('post_look',1),('mode',0),('body_transform_bytes','00'),('read_only',False),('query_slots',[1,2,3])]:
   original=c[key];c[key]=value;self.assertEqual(native_look_branch(r)[0],'unproven');c[key]=original
if __name__=='__main__':unittest.main()
