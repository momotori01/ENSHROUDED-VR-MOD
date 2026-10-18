"""Cross-check saved eye pairs against their exact XR and native camera snapshots."""
import argparse,json,math,re,struct
from pathlib import Path

def qmul(a,b):
 x,y,z,w=a;X,Y,Z,W=b
 return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def conjugate(q):return [-q[0],-q[1],-q[2],q[3]]
def rotate(q,p):return qmul(qmul(q,[*p,0]),conjugate(q))[:3]
def read_events(p):return [json.loads(s) for s in p.read_text(encoding='utf-8').splitlines() if s.strip()]
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('launch',type=Path);arg=a.parse_args();root=arg.launch
 launch=json.loads((root/'launch.json').read_text(encoding='utf-8-sig'))
 probes=sorted(root.glob('view-resource-probe-*/world-session.txt'))
 if not probes:raise ValueError('live session probe not found')
 probe=probes[-1].parent;runtime=read_events(root/'native-xr/openxr.jsonl');game=read_events(probe/'view-resource-probe.jsonl')
 poses={x['frame']:x for x in runtime if x['event']=='native_capture_pose'}
 cameras={}
 for x in game:
  if x['event']=='captured_game_camera':
   k=(x['frame'],x['eye'])
   if k in cameras:raise ValueError('duplicate native captured camera')
   cameras[k]=x
 frames={}
 for path in (root/'native-xr').glob('frame-*-eye-*.ppm'):
  m=re.fullmatch(r'frame-(\d+)-eye-([01])\.ppm',path.name)
  if m:frames.setdefault(int(m[1]),set()).add(int(m[2]))
 failures=[];rows=[]
 for frame,eyes in sorted(frames.items()):
  if eyes!={0,1} or frame not in poses or any((frame,i) not in cameras for i in (0,1)):
   failures.append(dict(frame=frame,reason='image/pose/native-camera pair incomplete'));continue
  h=poses[frame];cs=[cameras[frame,i] for i in (0,1)];xs=h['eyes']
  if len(xs)!=2 or [x['eye'] for x in xs]!=[0,1]:raise ValueError('runtime eye identity differs')
  common=[];fov_error=0
  for i in (0,1):
   c=cs[i];x=xs[i];d=bytes.fromhex(c['descriptor'])
   if len(d)!=80 or struct.unpack_from('<I',d)[0]!=3:raise ValueError('asymmetric descriptor missing')
   l,r,b,t,n,f=struct.unpack_from('<6f',d,4)
   error=max(abs(v/n-math.tan(angle)) for v,angle in zip([l,r,t,b],x['fov']));fov_error=max(fov_error,error)
   q=x['orientation'];qr=[-q[0],-q[1],q[2],q[3]]
   common.append(qmul(c['orientation'],conjugate(qr)))
  delta=[xs[1]['position'][i]-xs[0]['position'][i] for i in range(3)];delta[2]*=-1
  expected=rotate(common[0],delta);actual=[(cs[1]['camera_q32'][i]-cs[0]['camera_q32'][i])/2**32 for i in range(3)]
  pos_error=max(abs(v-w) for v,w in zip(expected,actual))
  rotation_error=min(max(abs(v-w) for v,w in zip(*common)),max(abs(v+w) for v,w in zip(*common)))
  ok=(cs[0]['time']==cs[1]['time']==h['time'] and cs[0]['serial']==cs[1]['serial'] and (h['head_flags']&3)==3
      and h['regular_async_policy']==launch['xr_async_completion'] and h['strict_capture'] and pos_error<1e-4 and rotation_error<1e-5 and fov_error<2e-6)
  rows.append(dict(frame=frame,ok=ok,position_error_m=pos_error,rotation_error=rotation_error,fov_tangent_error=fov_error))
  if not ok:failures.append(dict(frame=frame,reason='paired camera geometry or policy mismatch'))
 regular_frames=[x['frame'] for x in runtime if x['event']=='hmd'];last_regular=max(regular_frames,default=0)
 report=dict(status='PASS' if rows and not failures else 'FAIL',pairs=len(rows),failures=failures,regular_hmd_last_frame=last_regular,
             captures_after_regular_pose_log=sum(r['frame']>last_regular for r in rows),rows=rows,
             limitation='Validates camera evidence for saved pairs only. Capture waits for GPU completion; ordinary async image quality is not established.')
 (root/'captured-camera-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print(json.dumps({k:v for k,v in report.items() if k!='rows'},indent=2));return int(report['status']!='PASS')
if __name__=='__main__':raise SystemExit(main())
