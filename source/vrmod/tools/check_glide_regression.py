"""Check the glide look/jump/roll fixture, including actual Root displacement."""
import argparse,json,math
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--without-glide',action='store_true');a=p.parse_args()
play=json.loads((a.launch/'play-session-check.json').read_text(encoding='utf-8-sig'));rows=[]
for l in (a.launch/play['probe']/'view-resource-probe.jsonl').open(encoding='utf-8-sig'):
 try:rows.append(json.loads(l))
 except ValueError:pass
head=[e for e in rows if e.get('event')=='input_head_walking_sample'];basis=[e for e in rows if e.get('event')=='glide_native_basis'];roots=[e for e in rows if e.get('event')=='root_camera_sample']
def yaw(q):
 x,y,z,w=q;return math.degrees(math.atan2(2*(x*z+w*y),1-2*(x*x+y*y)))
def error(a,b):return abs((a-b+180)%360-180)
first=next((e for e in basis if e.get('applied')),None)
later=[e for e in basis if 100000000<e.get('native_played_time',0)<350000000 and e.get('caller')=='0x22aaf2' and first and error(yaw(e['camera_orientation']),yaw(first['camera_orientation']))>25]
checks=dict(play=play['status']=='PASS',look_during_glide=bool(later) and all(not e['applied'] and error(yaw(e['body_orientation']),yaw(first['camera_orientation']))<5 for e in later))
if a.without_glide:checks.pop('look_during_glide')
metrics={}
for name,lo,hi,bit in [('jump',1600,1700,1),('roll',1800,1870,3)]:
 events=[e for e in head if lo<=e.get('frame',0)<hi and e.get('actor_state_effective',0)&(1<<bit)]
 checks[name+'_state_seen']=bool(events);checks[name+'_head_camera_preserved']=bool(events) and all(e.get('applied') and e.get('only_camera_bytes_changed') for e in events)
 if events:
  e=events[0];r=min(roots,key=lambda r:abs(r['us']-e['us']));s=min(roots,key=lambda r:abs(r['us']-e['us']-200000));d=[(y-x)/2**32 for x,y in zip(r['stable_q32'],s['stable_q32'])];heading=math.degrees(math.atan2(d[0],d[2]));headYaw=yaw(e['camera_orientation'])
  metrics[name]=dict(state_records=len(events),start_frame=r['frame'],end_frame=s['frame'],delta_m=d,head_deg=headYaw,travel_deg=heading,error_deg=error(heading,headYaw))
  checks[name+'_actual_forward']=math.hypot(d[0],d[2])>.1 and error(heading,headYaw)<5
  if name=='jump':checks['jump_actual_rise']=d[1]>.05
 else:checks[name+'_actual_forward']=False
if later:metrics['look']=dict(records=len(later),max_head_turn_deg=max(error(yaw(e['camera_orientation']),yaw(first['camera_orientation'])) for e in later),max_body_turn_deg=max(error(yaw(e['body_orientation']),yaw(first['camera_orientation'])) for e in later))
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,metrics=metrics,limit='Specific short Mock fixture; contacts, long flight and real-device feel are separate.')
(a.launch/('movement-regression-check.json' if a.without_glide else 'glide-regression-check.json')).write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
