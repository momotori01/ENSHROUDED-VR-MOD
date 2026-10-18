"""CPU negative tests for native-source role policy; not native gameplay proof."""
import copy,json,pathlib,unittest
from combat_epoch_policy import published
class PolicyTests(unittest.TestCase):
 def setUp(self):
  path=pathlib.Path(__file__).resolve().parents[1]/'output/game-20261005-054723/world-lifetimes-20261005-054724/view-resource-probe.jsonl'
  self.rows=[json.loads(x) for x in path.read_text().splitlines()]
  next(r for r in self.rows if r['event']=='world_lifecycle_observation_begin').update(xr_trial=True,seconds=420)
 def test_archived_native_roles(self):
  c,s=published(self.rows,False);self.assertEqual((c['kind'],s['kind']),(2,0))
 def test_ended_not_live(self):
  with self.assertRaises(ValueError):published(self.rows,True)
 def test_missing_or_corrupt_identity(self):
  cases=[('world_lifecycle_observation_begin',{'xr_trial':False}),('world_lifecycle_observation_begin',{'seconds':421}),('world_broker_created',{'serial':2}),('world_broker_created',{'provider':2}),('world_broker_created',{'pins':1}),('world_broker_created',{'phase':2}),('world_broker_created',{'world':'0x1'}),('world_lifecycle_end',{'header_hex':'00'*16}),('world_lifecycle_prediction',{'registry':'0x1','header_hex':'00'*16}),('world_broker_created',{'owner_root':'0x1'})]
  for event,change in cases:
   with self.subTest(change=change):
    rows=copy.deepcopy(self.rows);next(r for r in rows if r['event']==event).update(**change)
    with self.assertRaises(ValueError):published(rows,False)
if __name__=='__main__':unittest.main()
