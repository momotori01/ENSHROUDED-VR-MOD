"""Run a bounded Mock desktop-render A/B/A/B comparison without resetting lighting."""
import argparse,json,os,subprocess,sys,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--observe-only',action='store_true');p.add_argument('--head-sweep',action='store_true');a=p.parse_args();launch=a.launch.resolve()
if a.head_sweep and not a.observe_only:p.error('--head-sweep requires --observe-only')
record=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
if record.get('runtime')!='mock' or not record.get('mock_navigation'):p.error('opted-in Mock required')
def alive():
 if (launch/'ui-close-request.json').exists():raise RuntimeError('game closing')
def events(probe):
 f=probe/'view-resource-probe.jsonl'
 if not f.exists():return []
 with f.open('rb') as h:h.seek(max(0,f.stat().st_size-262144));data=h.read()
 out=[]
 for line in data.splitlines():
  try:out.append(json.loads(line))
  except ValueError:pass
 return out
probe=None;results=[];deadline=time.monotonic()+220
while time.monotonic()<deadline:
 alive();matches=list(launch.glob('view-resource-probe-*/'+('world-session.txt' if a.observe_only else 'world-desktop-compare.txt')))
 if matches and (launch/'world-selection-check.json').exists():probe=matches[-1].parent;break
 time.sleep(.5)
if not probe:raise RuntimeError('comparison policy not ready')
try:
 subprocess.run([sys.executable,str(Path(__file__).with_name('run_mock_fog_route.py')),str(launch)],check=True,timeout=240)
 def capture(label):
  alive();r=subprocess.run([sys.executable,str(Path(__file__).with_name('capture_mock_navigation.py')),str(launch),label],check=True,stdout=subprocess.PIPE,text=True,timeout=30)
  result=json.loads(r.stdout);result['label']=label;results.append(result);print(json.dumps(result),flush=True)
 def mode(sequence,name):
  alive();target=probe/'desktop-mode.txt';tmp=probe/f'desktop-mode-{sequence}.tmp'
  with tmp.open('x',encoding='ascii') as f:f.write(f'EVR_DESKTOP_MODE_1 {sequence} {name}\n')
  for attempt in range(30):
   try:os.replace(tmp,target);break
   except PermissionError:
    if attempt==29:raise
    time.sleep(.01)
  deadline=time.monotonic()+15
  while time.monotonic()<deadline:
   alive()
   found=[e for e in events(probe) if e.get('event')=='desktop_compare_mode' and e.get('sequence')==sequence]
   if found:
    if found[-1].get('render')!=(name=='render'):raise RuntimeError('mode mismatch')
    results.append(found[-1]);print(json.dumps(found[-1]),flush=True);return
   time.sleep(.2)
  raise RuntimeError('mode acceptance timeout')
 capture('compare-initial-a' if a.observe_only else 'compare-skip-a');time.sleep(2);capture('compare-initial-a2' if a.observe_only else 'compare-skip-a2')
 for sequence,name in ([(1,'observe'),(2,'observe'),(3,'observe')] if a.observe_only else [(1,'render'),(2,'skip'),(3,'render')]):
  if not a.observe_only:mode(sequence,name)
  if a.head_sweep:
   pitch={1:-40,2:0,3:-20.200694}[sequence]
   subprocess.run([sys.executable,str(Path(__file__).with_name('mock_navigate.py')),str(launch),'--yaw','95.251684','--pitch',str(pitch)],check=True,stdout=subprocess.DEVNULL)
   results.append(dict(head_sweep_sequence=sequence,pitch=pitch,yaw=95.251684))
  for delay,label in [(2,'early'),(8,'settled'),(10,'late')]:
   time.sleep(delay);capture(f'compare-{sequence}-{name}-{label}')
 (launch/'desktop-compare-result.json').write_text(json.dumps(dict(status='CAPTURED',results=results,limitation='Inspect images and camera equality before claiming a visual result.'),indent=2)+'\n',encoding='utf-8')
finally:
 (probe/'stop.request').write_text('desktop comparison finished',encoding='ascii')
