"""Check transient loader churn retries without replaying any injection calls."""
import ctypes,unittest
from unittest.mock import patch
import inject_camera_observer as injector
class SnapshotRetry(unittest.TestCase):
 def run_case(self,errors):
  sequence=iter(errors)
  def create(flags,pid):
   self.assertEqual((flags,pid),(8,123))
   value=next(sequence)
   if value is None:return 100
   ctypes.set_last_error(value);return ctypes.c_void_p(-1).value
  return create
 def test_transient_churn(self):
  with patch.object(injector.k,'CreateToolhelp32Snapshot',side_effect=self.run_case([24,24,None])) as create,patch.object(injector.time,'sleep') as pause:
   self.assertEqual(injector.module_snapshot(123),100);self.assertEqual(create.call_count,3);self.assertEqual(pause.call_count,2)
 def test_no_retry_for_access_denied(self):
  with patch.object(injector.k,'CreateToolhelp32Snapshot',side_effect=self.run_case([5])) as create,patch.object(injector.time,'sleep') as pause:
   with self.assertRaises(OSError) as e:injector.module_snapshot(123)
   self.assertEqual(e.exception.errno,5);self.assertEqual(create.call_count,1);pause.assert_not_called()
 def test_persistent_churn_is_bounded(self):
  with patch.object(injector.k,'CreateToolhelp32Snapshot',side_effect=self.run_case([24]*21)) as create,patch.object(injector.time,'sleep') as pause:
   with self.assertRaises(OSError) as e:injector.module_snapshot(123)
   self.assertEqual(e.exception.errno,24);self.assertEqual(create.call_count,21);self.assertEqual(pause.call_count,20)
 def test_success_no_wait(self):
  with patch.object(injector.k,'CreateToolhelp32Snapshot',side_effect=self.run_case([None])),patch.object(injector.time,'sleep') as pause:
   self.assertEqual(injector.module_snapshot(123),100);pause.assert_not_called()
if __name__=='__main__':unittest.main()
