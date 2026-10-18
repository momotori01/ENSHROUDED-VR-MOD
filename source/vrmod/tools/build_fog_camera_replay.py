"""Build a bounded draw-camera Mock replay from a stationary-player real HMD interval."""
import argparse,json,hashlib,math,sys,statistics
from pathlib import Path
from check_capture_cameras import qmul,conjugate,rotate
from replay_real_head_pose import angles,quat
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('launch',type=Path);p.add_argument('output',type=Path);p.add_argument('--start',type=int,required=True);p.add_argument('--end',type=int,required=True);p.add_argument('--lead',type=int,default=700);p.add_argument('--rows',type=int,default=6000)
a=p.parse_args();assert 0<a.start<=a.end and 0<a.lead and a.rows>=a.lead+a.end-a.start+1
pose_path=a.output.with_suffix('.csv');ref_path=a.output.with_suffix('.txt');report_path=a.output.with_suffix('.json')
assert not any(x.exists() for x in [pose_path,ref_path,report_path])
heads={};captures={};cameras={}
source=a.launch/'native-xr/openxr.jsonl'
for line in source.open('rb'):
 e=json.loads(line)
 if e.get('event')=='hmd':heads[e['frame']]=e
 if e.get('event')=='native_capture_pose':
  captures[e['frame']]=e
  assert max(abs(x-y) for x,y in zip(e['eyes'][0]['orientation'],e['eyes'][1]['orientation']))<1e-7, 'Canted views require per-eye fixture support'
  heads[e['frame']]=dict(e,position=[sum(v['position'][i] for v in e['eyes'])/2 for i in range(3)],orientation=e['eyes'][0]['orientation'])
for sess in a.launch.glob('view-resource-probe-*/world-session.txt'):
 for line in sess.with_name('view-resource-probe.jsonl').open('rb'):
  e=json.loads(line)
  if e.get('event')=='captured_game_camera':cameras[e['frame'],e['eye']]=e
assert a.start in captures and all(f in heads for f in range(a.start,a.end+1))
first=captures[a.start];ipd=math.dist(first['eyes'][0]['position'],first['eyes'][1]['position']);origin=heads[a.start]['position'];fovs=[e['fov'] for e in first['eyes']]
rows=[];max_error=0
source_frames=[a.start]*a.lead+list(range(a.start,a.end+1))+[a.end]*(a.rows-a.lead-(a.end-a.start+1))
for n,f in enumerate(source_frames,1):
 h=heads[f];deg,error=angles(h['orientation']);max_error=max(max_error,error)
 pos=[h['position'][i]-origin[i]+(1.65 if i==1 else 0) for i in range(3)]
 rows.append([n,*pos,*deg,ipd,1,1,first['eyes'][0]['width'],first['eyes'][0]['height'],*fovs[0],*fovs[1]])
q=heads[a.start]['orientation'];qr=[-q[0],-q[1],q[2],q[3]];bases=[];roots=[]
for eye in (0,1):
 c=cameras[a.start,eye];base=qmul(c['orientation'],conjugate(qr));norm=math.sqrt(sum(v*v for v in base));base=[v/norm for v in base];bases.append(base)
 off=rotate(q,[(1 if eye else -1)*ipd/2,0,0]);pos=[off[0],off[1]+1.65,-off[2]];offset=rotate(base,pos);roots.append([c['camera_q32'][i]/2**32-offset[i] for i in range(3)])
root=[round(sum(v[i] for v in roots)/2*2**32) for i in range(3)];base=bases[0];checks=[]
for f,h in sorted(captures.items()):
 if not a.start<=f<=a.end:continue
 row=rows[a.lead+f-a.start];q=quat(*[x*math.pi/180 for x in row[4:7]])
 for eye in (0,1):
  c=cameras[f,eye];off=rotate(q,[(1 if eye else -1)*ipd/2,0,0]);pos=[row[1+i]+off[i] for i in range(3)];pos[2]*=-1;delta=rotate(base,pos)
  perr=max(abs(c['camera_q32'][i]/2**32-root[i]/2**32-delta[i]) for i in range(3));expected=qmul(base,[-q[0],-q[1],q[2],q[3]]);qerr=min(max(abs(x-y) for x,y in zip(expected,c['orientation'])),max(abs(x+y) for x,y in zip(expected,c['orientation'])))
  checks.append(dict(real_frame=f,mock_frame=int(row[0]),eye=eye,position_error_m=perr,rotation_error=qerr))
print('geometry maxima',max(x['position_error_m'] for x in checks),max(x['rotation_error'] for x in checks), checks[:2],flush=True)
assert checks and max(x['position_error_m'] for x in checks)<1e-5 and max(x['rotation_error'] for x in checks)<2e-6, 'Player motion or unsupported optical geometry in selected interval'
pose_path.write_text('# Real stationary-player HMD interval; head translation normalized, rotation retained.\n'+'\n'.join(','.join(f'{v:.11g}' for v in row) for row in rows)+'\n',encoding='ascii')
ref_path.write_text('EVR_SCENERY_REPLAY_1\n'+' '.join(map(str,root))+' '+' '.join(f'{v:.11g}' for v in base)+'\n',encoding='ascii')
report=dict(source=str(a.launch),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),start=a.start,end=a.end,lead=a.lead,rows=len(rows),ipd=ipd,reference_q32=root,orientation=base,checks=checks,max_position_error_m=max(x['position_error_m'] for x in checks),max_rotation_error=max(x['rotation_error'] for x in checks),limitation='Draw cameras only. Capture frames use the actual submitted-eye midpoint/orientation; between captures uses HMD samples. One sample per Mock frame, not wall-clock pacing. Player/physics/save transforms unmodified; native weather and streaming not frozen.')
report_path.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in report.items() if k!='checks'},indent=2))
