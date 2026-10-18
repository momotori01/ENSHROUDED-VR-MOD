"""Compare consecutive scenery frames after rotation alignment. Residuals are diagnostic, not a quality pass."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--width',type=int,default=600);p.add_argument('--expected-bursts',type=int,default=4);a=p.parse_args();assert 300<=a.width<=1000
root=a.launch/'native-xr';cams={}
for f in a.launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
 for line in f.open(encoding='utf-8'):
  e=json.loads(line)
  if e.get('event')=='captured_game_camera':cams[e['frame'],e['eye']]=e
frames=sorted(int(f.name.split('-')[1]) for f in root.glob('frame-*-eye-0.ppm'));groups=[]
for f in frames:
 if groups and groups[-1][-1]+1==f:groups[-1].append(f)
 else:groups.append([f])
groups=[g for g in groups if len(g)>=16];assert len(groups)==a.expected_bursts and all(len(g)==32 for g in groups),'Requested32-frame bursts incomplete'
w=a.width;size=(w,w);font=ImageFont.truetype('C:/Windows/Fonts/meiryo.ttc',18)
def load(f,e):return Image.open(root/f'frame-{f}-eye-{e}.ppm').convert('RGB').resize(size,Image.Resampling.LANCZOS)
def matrices(f,e):
 c=cams[f,e];v=np.asarray(c['view_matrix']).reshape(4,4,order='F');p=np.asarray(c['projection_matrix']).reshape(4,4,order='F')
 return v[:3,:3],np.array([[w/2*p[0,0],0,w/2],[0,-w/2*p[1,1],w/2],[0,0,1.]])
metrics=[]
for number,g in enumerate(groups):
 selected=[g[0],g[len(g)//2],g[-1]];sheet=Image.new('RGB',(w*2,3*(w+30)),'#181818');d=ImageDraw.Draw(sheet)
 for row,f in enumerate(selected):
  for e in (0,1):sheet.paste(load(f,e),(w*e,row*(w+30)+30));d.text((w*e+6,row*(w+30)+4),f'{"左目" if e==0 else "右目"} {f}',font=font,fill='white')
 sheet.save(root/f'scenery-turn-{number+1}-contact.jpg',quality=94)
 for e in (0,1):
  local=[]
  for before,after in zip(g,g[1:]):
   rb,kb=matrices(before,e);ra,ka=matrices(after,e);h=kb@rb@ra.T@np.linalg.inv(ka);h/=h[2,2];args=tuple(h.flat)[:8]
   im=load(before,e);aligned=im.transform(size,Image.Transform.PERSPECTIVE,args,Image.Resampling.BICUBIC);target=load(after,e)
   valid=np.asarray(Image.new('L',size,255).transform(size,Image.Transform.PERSPECTIVE,args,Image.Resampling.NEAREST))>0
   margin=w//15;valid[:margin]=False;valid[-margin:]=False;valid[:,:margin]=False;valid[:,-margin:]=False
   diff=np.abs(np.asarray(aligned,dtype=float)-np.asarray(target,dtype=float)).mean(axis=2);vals=diff[valid]
   r=dict(burst=number+1,eye=e,before=before,after=after,mean_absolute_rgb=float(vals.mean()),p95=float(np.percentile(vals,95)),fraction_over_32=float((vals>32).mean()))
   metrics.append(r);local.append((r,aligned,target,diff))
  r,aligned,target,diff=max(local,key=lambda x:x[0]['fraction_over_32'])
  sheet=Image.new('RGB',(w*3,w+40),'#181818');d=ImageDraw.Draw(sheet)
  heat=np.zeros((w,w,3),dtype=np.uint8);heat[:,:,0]=np.minimum(diff*4,255).astype(np.uint8);heat[:,:,1]=np.minimum(diff,255).astype(np.uint8)
  for c,(im,label) in enumerate([(aligned,f"回転を合わせた前画像 {r['before']}"),(target,f"次画像 {r['after']}"),(Image.fromarray(heat),'差分（赤を強調）')]):
   sheet.paste(im,(c*w,40));d.text((c*w+6,7),label,font=font,fill='white')
  sheet.save(root/f'scenery-turn-{number+1}-eye-{e}-largest-residual.jpg',quality=94)
r=dict(bursts=[dict(first=g[0],last=g[-1],frames=len(g)) for g in groups],pairs=metrics,
 limitations=['Rotation alignment only: nearby parallax, wind, hands, lighting and temporal filters remain in residuals.','No exposure/color normalization. Numbers do not classify visual correctness.','Saved images wait for GPU completion; ordinary asynchronous rendering is not established.'])
(a.launch/'scenery-turn-analysis.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(bursts=r['bursts'],largest_residuals=sorted(metrics,key=lambda x:x['fraction_over_32'],reverse=True)[:8]),indent=2))
