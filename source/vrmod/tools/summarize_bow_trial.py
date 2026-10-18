"""Read a running or completed bow diagnostic without changing its evidence."""
import argparse,json,struct,collections
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args()
for d in sorted(a.launch.glob('view-resource-probe-*')):
 phases=[];worlds={};last=0;counts=collections.Counter()
 for line in (d/'view-resource-probe.jsonl').open():
  try:e=json.loads(line)
  except ValueError:continue
  event=e['event']
  if event=='motion_visual_pose':last=max(last,e['frame'])
  if event=='bow_input_trial_phase' or 'failure' in event:phases.append(e)
  if event=='native_grip_sample':counts[e['weapon_type']]+=1
  if event!='input_bow_sample':continue
  b=bytes.fromhex(e['weapon_bytes'])
  if struct.unpack_from('<I',b,0x4c)[0]!=327157552:continue
  w=worlds.setdefault(e['world'],[])
  if not w or w[-1]['state']!=b[0]:w.append(dict(frame=e['frame'],state=b[0],digital=e.get('digital'),queued=b[0x39],trigger=[b[0x40],b[0x41]]))
 if last:print(json.dumps(dict(probe=d.name,frame=last,phases=phases,worlds=worlds,anatomy_samples=counts),indent=2))
