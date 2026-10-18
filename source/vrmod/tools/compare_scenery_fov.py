"""Align two captured FOVs with the same world eye pose and make ungraded diagnostic crops."""
import argparse,json,math
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont

def cameras(root):
 result={}
 for f in root.glob('view-resource-probe-*/view-resource-probe.jsonl'):
  for l in f.open():
   e=json.loads(l)
   if e.get('event')=='captured_game_camera':result[e['frame'],e['eye']]=e
 return result

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('before',type=Path);p.add_argument('after',type=Path);p.add_argument('--frames',default='2136,2152,2168');p.add_argument('--eye',type=int,choices=[0,1],default=1);p.add_argument('--roi',default='780,410,1880,820');p.add_argument('--cell-width',type=int,default=1100);p.add_argument('--name',default='fixed-fov-terrain-comparison');p.add_argument('--before-label',default='元の視野');p.add_argument('--after-label',default='対称な包含視野（同じ範囲を切り出し）');a=p.parse_args()
 import struct
 cs=[cameras(x) for x in [a.before,a.after]];frames=[tuple(map(int,x.split(':'))) if ':' in x else (int(x),int(x)) for x in a.frames.split(',')];font=ImageFont.truetype('C:/Windows/Fonts/meiryo.ttc',21)
 roi=tuple(map(int,a.roi.split(',')));assert len(roi)==4 and 0<=roi[0]<roi[2] and 0<=roi[1]<roi[3] and 200<=a.cell_width<=1963
 assert a.name.replace('-','').replace('_','').isalnum()
 cw=a.cell_width;ch=round((roi[3]-roi[1])*cw/(roi[2]-roi[0]));rh=ch+40
 out=Image.new('RGB',(2*cw,len(frames)*rh),(20,20,20));d=ImageDraw.Draw(out);checks=[]
 for row,(fr,after_fr) in enumerate(frames):
  cams=[cs[0][fr,a.eye],cs[1][after_fr,a.eye]]
  pe=max(abs(x-y)/2**32 for x,y in zip(cams[0]['camera_q32'],cams[1]['camera_q32']));qe=min(max(abs(x-y) for x,y in zip(cams[0]['orientation'],cams[1]['orientation'])),max(abs(x+y) for x,y in zip(cams[0]['orientation'],cams[1]['orientation'])))
  if pe>1e-5 or qe>2e-6:raise ValueError('camera poses differ; refusing aligned comparison')
  rects=[]
  for c in cams:
   l,r,b,t,n,f=struct.unpack_from('<6f',bytes.fromhex(c['descriptor']),4);rects.append((l/n,r/n,b/n,t/n))
  ims=[Image.open(root/'native-xr'/f'frame-{f}-eye-{a.eye}.ppm') for root,f in [(a.before,fr),(a.after,after_fr)]]
  l,r,b,t=rects[0];L,R,B,T=rects[1];w,h=ims[1].size
  box=((l-L)/(R-L)*w,(T-t)/(T-B)*h,(r-L)/(R-L)*w,(T-b)/(T-B)*h)
  if min(box)<-1e-3 or box[2]>w+1e-3 or box[3]>h+1e-3:raise ValueError('after FOV does not contain original')
  ims[1]=ims[1].transform(ims[0].size,Image.Transform.EXTENT,box,Image.Resampling.BILINEAR)
  for col,im in enumerate(ims):
   out.paste(im.crop(roi).resize((cw,ch),Image.Resampling.LANCZOS),(col*cw,row*rh+40));d.text((col*cw+10,row*rh+6),(a.before_label if col==0 else a.after_label)+f'  フレーム {fr if col==0 else after_fr}',font=font,fill='white')
  checks.append(dict(frame=fr,after_frame=after_fr,position_difference_m=pe,quaternion_difference=qe,after_sampling_box=box))
 path=a.after/'native-xr'/(a.name+'.jpg');out.save(path,quality=96)
 report=dict(before=str(a.before),after=str(a.after),eye=a.eye,rows=checks,image=str(path),roi=roi,cell_width=cw,labels=[a.before_label,a.after_label],limitations=['Only FOV geometry aligned; no color or luminance adjustment.','World time and weather were not frozen.'])
 (a.after/(a.name+'.json')).write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
if __name__=='__main__':main()
