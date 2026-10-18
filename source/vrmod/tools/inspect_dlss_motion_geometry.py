"""Compare captured DLSS motion with static-world reprojection from exact Mock head poses."""
import argparse,json,math,sys
from pathlib import Path
import numpy as np
from PIL import Image
from check_capture_cameras import qmul,rotate
from replay_real_head_pose import quat

def rotation(q):return np.column_stack([rotate(q,b) for b in np.eye(3)])
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--frame',type=int);a=p.parse_args()
 selection=json.loads((a.launch/'dlss-snapshot-request.json').read_text());directories=list((a.launch/'native-xr').glob(f'eye-dump-{a.frame}' if a.frame else 'eye-dump-*'));assert len(directories)==1;directory=directories[0];manifest=json.loads((directory/'manifest.json').read_text());frame=manifest['frame']
 probe=sorted(a.launch.glob('view-resource-probe-*/world-session.txt'))[-1].parent
 ref=(probe/'world-scenery-replay.txt').read_text().split();origin=np.array(list(map(int,ref[1:4])),dtype=np.float64)/2**32;base=list(map(float,ref[4:]));base=np.array(base)/np.linalg.norm(base)
 rows=[list(map(float,l.split(','))) for l in (a.launch/'mock-pose.csv').read_text().splitlines() if l and not l.startswith('#')]
 cameras={e['eye']:e for l in (probe/'view-resource-probe.jsonl').read_text().splitlines() if (e:=json.loads(l)).get('event')=='captured_game_camera' and e['frame']==frame}
 def pose(n,eye):
  r=rows[(n-1)%len(rows)];q=quat(*[math.radians(v) for v in r[4:7]]);offset=rotate(q,[(1 if eye else -1)*r[7]/2,0,0]);pos=[r[i+1]+offset[i] for i in range(3)];pos[2]*=-1
  return origin+rotate(base,pos),rotation(qmul(base,[-q[0],-q[1],q[2],q[3]]))
 images={(x['eye_by_eval_order'],x['role']):x for x in selection['images']};out=directory/'motion-geometry';out.mkdir(exist_ok=True);results=[]
 for eye in [0,1]:
  m=images[eye,'motion'];d=images[eye,'depth'];w,h=m['w'],m['h'];assert d['w']==w and d['h']==h and m['format']==78 and d['format'] in (100,126)
  motion=np.maximum(-1,np.fromfile(directory/f'image-{m["i"]}.bin','<i2').reshape(h,w,2).astype(float)/32767)*[w,h]
  depth=np.fromfile(directory/f'image-{d["i"]}.bin','<f4').reshape(h,w).astype(float)
  cam=cameras[eye];position=np.array(cam['camera_q32'],dtype=np.float64)/2**32;rot=rotation(cam['orientation']);p=np.array(cam['projection_matrix']).reshape(4,4,order='F')
  expected_position,expected_rot=pose(frame,eye);assert abs(position-expected_position).max()<1e-5 and abs(rot-expected_rot).max()<2e-6
  previous,previous_rot=pose(frame-1,eye);yy,xx=np.indices((h,w));u=(xx+.5)/w;v=(yy+.5)/h;z=p[2,3]/(depth-p[2,2]);valid=np.isfinite(z)&(z>.05)&(z<100)
  candidates=[]
  for ysign in [-1,1]:
   ndcx=2*u-1;ndcy=ysign*(2*v-1);cv=np.stack([(ndcx-p[0,2])*z/p[0,0],(ndcy-p[1,2])*z/p[1,1],z],axis=-1)
   world=cv@rot.T+position;pv=(world-previous)@previous_rot
   pu=(pv[:,:,0]/pv[:,:,2]*p[0,0]+p[0,2]+1)/2;py=(pv[:,:,1]/pv[:,:,2]*p[1,1]+p[1,2])/ysign/2+.5
   predicted=np.stack([(pu-u)*w,(py-v)*h],axis=-1)
   for sign in [-1,1]:
    residual=motion-sign*predicted;error=np.linalg.norm(residual,axis=-1);good=valid&(pv[:,:,2]>.05)
    item=dict(screen_y_sign=ysign,motion_direction_sign=sign,median_error_px=float(np.median(error[good])),p90_error_px=float(np.percentile(error[good],90)),median_bias_px=np.median(residual[good],axis=0).tolist(),median_actual_length_px=float(np.median(np.linalg.norm(motion[good],axis=-1))),median_expected_length_px=float(np.median(np.linalg.norm(predicted[good],axis=-1))))
    candidates.append(item)
    if ysign==-1 and sign==1:
     np.save(out/f'eye{eye}-predicted-static-motion.npy',predicted.astype(np.float32));np.save(out/f'eye{eye}-residual-static-motion.npy',residual.astype(np.float32))
     heat=np.zeros((h,w,3),np.uint8);heat[:,:,0]=np.minimum(error*64,255).astype(np.uint8);heat[~good]=0;Image.fromarray(heat).save(out/f'eye{eye}-static-motion-error-4px-red.png')
  results.append(dict(eye_by_eval_order=eye,frame=frame,previous_frame=frame-1,valid_pixels=int(valid.sum()),candidates=candidates))
 report=dict(results=results,scope='Static-world hypothesis only; grass animation, disocclusions and jitter ray offsets are not modeled. Candidate screen/sign conventions are listed, not silently fitted. Native SNORM quantization is about 0.04 input pixels. Not a temporal-quality PASS.')
 (out/'geometry.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
