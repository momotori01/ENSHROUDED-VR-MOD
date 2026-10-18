"""Send one bounded controller/head command to an opted-in Mock play session."""
import argparse,json,math,os,time
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('launch',type=Path)
p.add_argument('--yaw',type=float)
p.add_argument('--pitch',type=float)
p.add_argument('--left-x',type=float,default=0)
p.add_argument('--left-y',type=float,default=0)
p.add_argument('--right-x',type=float,default=0)
p.add_argument('--jump',action='store_true')
p.add_argument('--hold-ms',type=int,default=0)
a=p.parse_args();launch=a.launch.resolve()
record=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
if record.get('runtime')!='mock' or not record.get('mock_navigation'):p.error('requires opted-in Mock launch')
if (launch/'ui-close-request.json').exists():p.error('session already requested to close')
path=launch/'mock-navigation.txt'
if not path.is_file() or path.is_symlink():p.error('navigation file missing or linked')
old=path.read_text(encoding='ascii').split()
seq=0;yaw=0.;pitch=0.
if old:
 if len(old)!=10 or old[0]!='EVR_MOCK_NAV_1':p.error('invalid prior command; inspect before replacing')
 seq=int(old[1]);yaw=float(old[4]);pitch=float(old[5])
yaw=yaw if a.yaw is None else a.yaw;pitch=pitch if a.pitch is None else a.pitch
values=(yaw,pitch,a.left_x,a.left_y,a.right_x)
if not all(math.isfinite(v) for v in values) or abs(yaw)>360 or abs(pitch)>85 or any(abs(v)>1 for v in values[2:]):p.error('invalid angle/stick range')
if not 0<=a.hold_ms<=2000:p.error('hold must be 0..2000 ms')
if not a.hold_ms and (a.left_x or a.left_y or a.right_x or a.jump):p.error('moving commands need an explicit hold')
issued=time.time_ns()//1000000
line=f'EVR_MOCK_NAV_1 {seq+1} {issued} {a.hold_ms} {yaw:.6f} {pitch:.6f} {a.left_x:.6f} {a.left_y:.6f} {a.right_x:.6f} {int(a.jump)}'
tmp=launch/f'mock-navigation-{issued}.tmp'
with tmp.open('x',encoding='ascii',newline='') as f:f.write(line+'\n')
# The runtime may briefly hold a Windows read handle without delete sharing.
# Retry only the atomic publish, without extending the original input expiry.
deadline=time.monotonic()+min(.25,a.hold_ms/1000 if a.hold_ms else .25)
while True:
 try:
  os.replace(tmp,path)
  break
 except PermissionError as error:
  if getattr(error,'winerror',None) not in (5,32,33) or time.monotonic()>=deadline:raise
  time.sleep(.01)
entry={'issued_ms':issued,'sequence':seq+1,'command':line,'scope':'mock input only; maximum hold 2000ms'}
with (launch/'mock-navigation-sent.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps(entry)+'\n')
print(json.dumps(entry))
