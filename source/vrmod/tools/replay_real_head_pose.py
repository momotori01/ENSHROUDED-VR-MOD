"""Generate a Mock 6DoF CSV from recorded HMD poses, preserving measured position/rotation deltas."""
import argparse,json,math,statistics,hashlib
from pathlib import Path

def mul(a,b):
 x,y,z,w=a;X,Y,Z,W=b
 return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def quat(y,p,r):
 return mul(mul([0,math.sin(y/2),0,math.cos(y/2)],[math.sin(p/2),0,0,math.cos(p/2)]),[0,0,math.sin(r/2),math.cos(r/2)])
def angles(q):
 n=math.sqrt(sum(v*v for v in q));x,y,z,w=[v/n for v in q]
 pitch=math.asin(max(-1,min(1,2*(w*x-y*z))))
 yaw=math.atan2(2*(x*z+w*y),1-2*(x*x+y*y))
 roll=math.atan2(2*(x*y+w*z),1-2*(x*x+z*z))
 rebuilt=quat(yaw,pitch,roll);err=min(max(abs(a-b) for a,b in zip(rebuilt,[x,y,z,w])),max(abs(a+b) for a,b in zip(rebuilt,[x,y,z,w])))
 if err>1e-6:raise ValueError('Euler reconstruction failed')
 return [v*180/math.pi for v in (yaw,pitch,roll)],err

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('output',type=Path);p.add_argument('--lead',type=int,default=700);p.add_argument('--frames',type=int,default=1800);p.add_argument('--size',type=int,default=1963);a=p.parse_args()
 if a.output.exists() or a.lead<0 or not 1<=a.frames<=10000 or not 1<=a.size<=4096:raise ValueError('new output and bounded dimensions required')
 raw=a.log.read_bytes();e=[json.loads(s) for s in raw.splitlines()];head=[x for x in e if x['event']=='hmd'][:a.frames]
 eyes={}
 for x in e:
  if x['event']=='runtime_eye_pose':eyes.setdefault(x['frame'],{})[x['eye']]=x
 ipds=[math.dist(v[0]['position'],v[1]['position']) for v in eyes.values() if 0 in v and 1 in v]
 if not head or not ipds:raise ValueError('HMD and binocular evidence required')
 ipd=statistics.median(ipds);origin=head[0]['position'];errors=[];rows=[]
 for n,h in enumerate([head[0]]*a.lead+head,1):
  orientation,error=angles(h['orientation']);errors.append(error)
  position=[h['position'][i]-origin[i]+(1.65 if i==1 else 0) for i in range(3)]
  values=[n,*position,*orientation,ipd,1,1,a.size,a.size,*eyes[min(eyes)][0]["fov"],*eyes[min(eyes)][1]["fov"]];rows.append(','.join(f'{v:.10g}' for v in values))
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text('# Recorded 6DoF head motion. Translated origin only; recorded rotations unchanged.\n'+'\n'.join(rows)+'\n')
 report=dict(source=str(a.log),source_sha256=hashlib.sha256(raw).hexdigest(),head_frames=len(head),lead=a.lead,rows=len(rows),measured_ipd_m=ipd,ipd_range_m=[min(ipds),max(ipds)],max_quaternion_component_error=max(errors),position_span_m=[max(h['position'][i] for h in head)-min(h['position'][i] for h in head) for i in range(3)],recorded_fov=[eyes[min(eyes)][i]['fov'] for i in range(2)],limitations=['Mock replays one recorded sample per frame, not original wall clock pacing.','Recorded per-eye FOV is included as 8 optional CSV columns.','No game/root/controller motion replay. No proof of real compositor behaviour.'])
 a.output.with_suffix('.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
