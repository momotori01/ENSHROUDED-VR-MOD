"""Compare consecutive XR triplets after compensating recorded head rotation (translation remains)."""
import argparse,json,math
from pathlib import Path
import numpy as np
from PIL import Image
p=argparse.ArgumentParser();p.add_argument('launch',type=Path);p.add_argument('--pose',type=Path,required=True);a=p.parse_args()
rows=np.loadtxt(a.pose,delimiter=',',comments='#');folder=a.launch/'native-xr';frames=sorted(int(x.name.split('-')[1]) for x in folder.glob('*-eye-0.ppm'));runs=[]
for n in frames:
 if runs and n==runs[-1][-1]+1:runs[-1].append(n)
 else:runs.append([n])
frames=max(runs,key=len)
if len(frames)<3:raise ValueError('no consecutive triplet')
def rotation(frame):
 y,p,r=np.radians(rows[min(frame,len(rows))-1,4:7]);cy,sy=np.cos(y),np.sin(y);cp,sp=np.cos(p),np.sin(p);cr,sr=np.cos(r),np.sin(r)
 return np.array([[cy,0,sy],[0,1,0],[-sy,0,cy]])@np.array([[1,0,0],[0,cp,-sp],[0,sp,cp]])@np.array([[cr,-sr,0],[sr,cr,0],[0,0,1]])
report={'frames':frames,'method':'head rotation compensated; no depth/translation correction; threshold max-channel AB>40 and AC<12','eyes':[]}
for eye in [0,1]:
 ims=[np.array(Image.open(folder/f'frame-{f}-eye-{eye}.ppm'))[::2,::2].astype(np.float32) for f in frames];h,w=ims[0].shape[:2];fov=rows[0,12+eye*4:16+eye*4];left,right,up,down=np.tan(fov)
 yy,xx=np.mgrid[:h,:w];rays=np.stack([left+(xx+.5)/w*(right-left),up+(yy+.5)/h*(down-up),-np.ones_like(xx)],axis=-1).astype(np.float32)
 def warp(im,src,dest):
  v=rays@(rotation(src).T@rotation(dest)).T;z=-v[:,:,2];u=(v[:,:,0]/z-left)/(right-left)*w-.5;t=(v[:,:,1]/z-up)/(down-up)*h-.5
  valid=(z>0)&(u>=0)&(u<w-1)&(t>=0)&(t<h-1);u=np.clip(u,0,w-1.001);t=np.clip(t,0,h-1.001);i=u.astype(np.int32);j=t.astype(np.int32);du=(u-i)[:,:,None];dt=(t-j)[:,:,None]
  val=im[j,i]*(1-du)*(1-dt)+im[j,i+1]*du*(1-dt)+im[j+1,i]*(1-du)*dt+im[j+1,i+1]*du*dt
  return val,valid
 heat=np.zeros((h,w),np.int32);metrics=[]
 for i in range(1,len(frames)-1):
  aa,va=warp(ims[i-1],frames[i-1],frames[i]);cc,vc=warp(ims[i+1],frames[i+1],frames[i]);bb=ims[i];valid=va&vc
  ab=np.abs(aa-bb).max(2);ac=np.abs(aa-cc).max(2);mask=(ab>40)&(ac<12)&valid;heat+=mask
  metrics.append(dict(frame=frames[i],percent=float(mask.sum()/max(1,valid.sum())*100),compensated_mean_ab=float(np.abs(aa-bb)[valid].mean()),raw_mean_ab=float(np.abs(ims[i-1]-bb).mean())))
 preview=(ims[len(ims)//2]/2).astype(np.uint8);preview[heat>0]=[255,100,0];Image.fromarray(preview).save(folder/f'rotation-compensated-toggle-eye-{eye}.png')
 report['eyes'].append(dict(eye=eye,mean_toggle_percent=float(np.mean([m['percent'] for m in metrics])),mean_compensated_ab=float(np.mean([m['compensated_mean_ab'] for m in metrics])),mean_raw_ab=float(np.mean([m['raw_mean_ab'] for m in metrics])),metrics=metrics))
(folder/'rotation-compensated-toggle.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({**report,'eyes':[{k:v for k,v in e.items() if k!='metrics'} for e in report['eyes']]},indent=2))
