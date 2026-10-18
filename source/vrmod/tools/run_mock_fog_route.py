"""Replay the verified southern stairs/glider route with bounded Mock controller input."""
import argparse,json,subprocess,sys,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args();launch=a.launch
rec=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
if rec.get('runtime')!='mock' or not rec.get('mock_navigation'):p.error('requires opted-in Mock')
# Fixed route is valid only for the verified spawn/save, never arbitrary sessions.
save=json.loads((launch/'save-reference-check.json').read_text(encoding='utf-8-sig'))
world=json.loads((launch/'world-selection-check.json').read_text(encoding='utf-8-sig'))
if save.get('status')!='PASS' or Path(save['reference']).name!='20261010-011450-789' or world.get('status')!='PASS' or world.get('expected')!='3bd85c7d':p.error('route requires its recorded world and save reference')
plan=[(['--yaw','180','--pitch','0','--left-x','-1','--hold-ms','350'],.65)]
plan += [(['--left-y','1','--hold-ms','2000'],2.25)]*11
plan += [([],1.0)]
plan += [(['--yaw','135','--left-y','1','--hold-ms','2000'],2.2)]*2
plan += [(['--left-y','1','--jump','--hold-ms','160'],.45),(['--left-y','1','--jump','--hold-ms','1600'],1.8),(['--left-y','1','--hold-ms','1000'],1.3),(['--pitch','-15'],2)]
plan += [(['--yaw','145','--pitch','0','--left-y','1','--jump','--hold-ms','160'],.45),(['--left-y','1','--jump','--hold-ms','1800'],2),(['--left-y','1','--hold-ms','1500'],1.8),(['--pitch','-15'],2)]
plan += [(['--yaw','130','--pitch','0','--left-y','1','--jump','--hold-ms','250'],.35),(['--left-y','1','--hold-ms','1600'],1.85),(['--yaw','95.251684','--pitch','-20.200694'],1)]
(launch/'fog-route-plan.json').write_text(json.dumps(dict(source='game-20261010-022744 successfully landed near real frame2485, no death',commands=plan,scope='Mock ordinary input only. Each command expires in <=2000ms; no player/save transform writes.'),indent=2)+'\n')
log=launch/'native-xr/openxr.jsonl';deadline=time.monotonic()+200
while time.monotonic()<deadline:
 if (launch/'ui-close-request.json').exists():raise RuntimeError('game closing')
 if log.exists():
  with log.open('rb') as f:f.seek(max(0,log.stat().st_size-65536));data=f.read()
  ev=[]
  for line in data.splitlines():
   try:ev.append(json.loads(line))
   except ValueError:pass
  if any(e.get('event')=='native_xr_closed' for e in ev):raise RuntimeError('XR closed')
  if max((e.get('frame',0) for e in ev),default=0)>=650:break
 time.sleep(.5)
else:raise RuntimeError('XR readiness timeout')
start=time.monotonic()
for args,seconds in plan:
 subprocess.run([sys.executable,str(Path(__file__).with_name('mock_navigate.py')),str(launch)]+args,check=True,stdout=subprocess.DEVNULL)
 time.sleep(seconds)
result=dict(issued_all=True,elapsed_s=time.monotonic()-start,commands=len(plan),scope='Command delivery only. Inspect captured position/UI before claiming successful navigation.')
(launch/'fog-route-complete.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result))
