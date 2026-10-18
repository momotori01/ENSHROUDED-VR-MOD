"""Audit captured coincident-eye geometry and show ungraded stereo differences.
Image statistics are diagnostic: stochastic GI, temporal filters and particles may differ.
"""
import argparse, json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('launch', type=Path)
p.add_argument('--width', type=int, default=640)
a=p.parse_args(); assert 300 <= a.width <= 1000
root=a.launch/'native-xr'; cameras={}
for f in a.launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
 for line in f.open(encoding='utf-8'):
  e=json.loads(line)
  if e.get('event')=='captured_game_camera': cameras[e['frame'],e['eye']]=e
frames=sorted(int(f.name.split('-')[1]) for f in root.glob('frame-*-eye-0.ppm'))
groups=[]
for f in frames:
 if groups and groups[-1][-1]+1==f: groups[-1].append(f)
 else: groups.append([f])
groups=[g for g in groups if len(g)>=16]
assert len(groups)==4 and all(len(g)==32 for g in groups), 'four complete 32-pair bursts required'
rows=[]; geometry=[]; w=a.width
font=ImageFont.truetype('C:/Windows/Fonts/meiryo.ttc',18)
def load(f,e):
 return Image.open(root/f'frame-{f}-eye-{e}.ppm').convert('RGB').resize((w,w),Image.Resampling.LANCZOS)
for n,g in enumerate(groups,1):
 local=[]
 for fr in g:
  c0,c1=[cameras[fr,e] for e in (0,1)]
  distance=float(np.linalg.norm((np.asarray(c1['camera_q32'],dtype=np.int64)-np.asarray(c0['camera_q32'],dtype=np.int64))/2**32))
  ori=min(max(abs(x-y) for x,y in zip(c0['orientation'],c1['orientation'])),max(abs(x+y) for x,y in zip(c0['orientation'],c1['orientation'])))
  proj=max(abs(x-y) for x,y in zip(c0['projection_matrix'],c1['projection_matrix']))
  geometry.append(dict(frame=fr,distance_m=distance,orientation_error=ori,projection_error=proj,same_time=c0['time']==c1['time'],same_serial=c0['serial']==c1['serial']))
  assert distance<1e-6 and ori<1e-7 and proj<1e-7 and c0['time']==c1['time'] and c0['serial']==c1['serial'], 'images do not have coincident camera geometry'
  left,right=[np.asarray(load(fr,e),dtype=np.float32) for e in (0,1)]
  signed=(right-left).mean(axis=2); diff=np.abs(right-left).mean(axis=2)
  blocks=[float(signed[y*w//8:(y+1)*w//8,x*w//8:(x+1)*w//8].mean()) for y in range(8) for x in range(8)]
  r=dict(burst=n,frame=fr,mean_absolute_rgb=float(diff.mean()),p95=float(np.percentile(diff,95)),fraction_over_32=float((diff>32).mean()),right_minus_left_mean=float(signed.mean()),largest_signed_block=max(blocks,key=abs))
  rows.append(r);local.append(r)
 selected=sorted(set([g[0],g[len(g)//2],max(local,key=lambda r:r['fraction_over_32'])['frame']]))
 sheet=Image.new('RGB',(3*w,len(selected)*(w+30)),'#181818');d=ImageDraw.Draw(sheet)
 for row,fr in enumerate(selected):
  ims=[load(fr,e) for e in (0,1)]
  diff=np.abs(np.asarray(ims[0],dtype=float)-np.asarray(ims[1],dtype=float)).mean(axis=2)
  heat=np.zeros((w,w,3),dtype=np.uint8);heat[:,:,0]=np.minimum(diff*4,255).astype('uint8');heat[:,:,1]=np.minimum(diff,255).astype('uint8')
  ims.append(Image.fromarray(heat))
  for col,(im,label) in enumerate(zip(ims,['左目','右目','左右差（赤を強調）'])):
   sheet.paste(im,(col*w,row*(w+30)+30));d.text((col*w+6,row*(w+30)+4),f'{label} {fr}',font=font,fill='white')
 sheet.save(root/f'coincident-eyes-{n}.jpg',quality=95)
report=dict(geometry_status='PASS',image_quality='REVIEW_REQUIRED',width=w,bursts=[dict(first=g[0],last=g[-1],count=len(g)) for g in groups],geometry=geometry,images=rows,limitations=['Identical optics and sub-micrometre separation are a diagnostic only, not a real headset.','RGB statistics use resized display images without exposure or colour adjustment.','Stochastic shading, temporal filters, motion vectors and eye-specific visibility may produce differences.','Capture waits for GPU completion; ordinary asynchronous rendering requires separate verification.'])
(a.launch/'coincident-eye-analysis.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(geometry_status=report['geometry_status'],pairs=len(rows),worst=sorted(rows,key=lambda r:r['fraction_over_32'],reverse=True)[:4]),indent=2))
