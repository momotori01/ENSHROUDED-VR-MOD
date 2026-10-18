"""Coverage assertions for interrupted input; not a game test."""
import copy,unittest
from check_picking_ray import camera_coverage

class PickingWindow(unittest.TestCase):
 def setUp(self):
  self.a=dict(entity=1,camera_q32=[1,2,3],camera_orientation=[0,0,0,1],us=10)
  self.e=dict(self.a,us=11)
 def covered(self,applied,matched,stop=20,mode='fixed-root-motion'):
  return all(camera_coverage(applied,matched,mode,stop).values())
 def test_fixed_before_stop(self):self.assertTrue(self.covered([self.a],[(self.e,self.a)]))
 def test_normal_motion_still_requires_motion(self):self.assertFalse(self.covered([self.a],[(self.e,self.a)],None))
 def test_missing_picking(self):self.assertFalse(self.covered([self.a],[]))
 def test_missing_source(self):self.assertFalse(self.covered([],[(self.e,self.a)]))
 def test_wrong_camera(self):
  e=dict(self.e,camera_q32=[2,2,3]);self.assertFalse(self.covered([self.a],[(e,self.a)]))
 def test_late_source(self):
  a=dict(self.a,us=21);self.assertFalse(self.covered([a],[(self.e,a)]))
 def test_late_consumption(self):
  e=dict(self.e,us=21);self.assertFalse(self.covered([self.a],[(e,self.a)]))
 def test_source_after_consumer(self):
  a=dict(self.a,us=12);self.assertFalse(self.covered([a],[(self.e,a)]))
 def test_moving_source_stuck_consumer_rejected(self):
  a=dict(self.a,camera_q32=[5,2,3],us=12);self.assertFalse(self.covered([self.a,a],[(self.e,self.a)]))
 def test_moving_source_and_consumer(self):
  a=dict(self.a,camera_q32=[5,2,3],us=12);e=dict(a,us=13);self.assertTrue(self.covered([self.a,a],[(self.e,self.a),(e,a)]))
 def test_varying_orientation_not_fixed_head(self):
  a=dict(self.a,camera_orientation=[0,1,0,0],us=12);e=dict(a,us=13);self.assertFalse(self.covered([self.a,a],[(self.e,self.a),(e,a)]))
 def test_fixed_default_unchanged(self):self.assertTrue(self.covered([self.a],[(self.e,self.a)],None,'fixed'))
if __name__=='__main__':unittest.main()
