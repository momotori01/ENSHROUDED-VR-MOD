"""Create a draw-only world camera reference from a captured real stereo pair and replay CSV."""
import argparse,json,math,hashlib
from pathlib import Path
from check_capture_cameras import qmul,conjugate,rotate
from replay_real_head_pose import quat

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('csv',type=Path);p.add_argument('output',type=Path);p.add_argument('--frame',type=int,default=1432);p.add_argument('--lead',type=int,default=700);a=p.parse_args()
 if a.output.exists():raise ValueError('new output required')
 cameras={}
 for f in a.launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
  for line in f.open():
   e=json.loads(line)
   if e.get('event')=='captured_game_camera' and e['frame']==a.frame:cameras[e['eye']]=e
 if len(cameras)!=2:raise ValueError('both captured game cameras required')
 rows=[line for line in a.csv.read_text().splitlines() if line and not line.startswith('#')]
 row=list(map(float,rows[a.frame+a.lead-1].split(',')));assert int(row[0])==a.frame+a.lead
 q=quat(*[x*math.pi/180 for x in row[4:7]]);qr=[-q[0],-q[1],q[2],q[3]]
 origins=[];bases=[];errors=[]
 for eye in (0,1):
  c=cameras[eye];base=qmul(c['orientation'],conjugate(qr));norm=math.sqrt(sum(x*x for x in base));base=[x/norm for x in base];bases.append(base)
  delta=rotate(q,[(1 if eye else -1)*row[7]/2,0,0]);pos=[row[1+i]+delta[i] for i in range(3)];pos[2]*=-1
  offset=rotate(base,pos);origin=[c['camera_q32'][i]/2**32-offset[i] for i in range(3)];origins.append(origin)
 error=max(abs(x-y) for x,y in zip(*origins))
 if error>1e-5:raise ValueError(f'eye reference origins disagree: {error}')
 q32=[round(sum(o[i] for o in origins)/2*2**32) for i in range(3)]
 a.output.write_text('EVR_SCENERY_REPLAY_1\n'+' '.join(map(str,q32))+' '+' '.join(f'{x:.10g}' for x in bases[0])+'\n')
 report=dict(source=str(a.launch),real_frame=a.frame,mock_frame=a.frame+a.lead,csv=str(a.csv),csv_sha256=hashlib.sha256(a.csv.read_bytes()).hexdigest(),reference_q32=q32,orientation=bases[0],binocular_origin_error_m=error,scope='Mock draw cameras only; no player, physics or save transform writes')
 a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
