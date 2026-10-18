"""Inspect post-pair DLSS snapshots; preserve raw values and use one common display conversion."""
import argparse,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw

def srgb(a):
 c=np.clip(a,0,1);return np.where(c<=.0031308,12.92*c,1.055*np.power(c,1/2.4)-.055)
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--frame',type=int);a=p.parse_args()
 selection=json.loads((a.launch/'dlss-snapshot-request.json').read_text());dirs=list((a.launch/'native-xr').glob(f'eye-dump-{a.frame}' if a.frame else 'eye-dump-*'));assert len(dirs)==1
 directory=dirs[0];manifest=json.loads((directory/'manifest.json').read_text());byid={x['i']:x for x in manifest['images']};out=directory/'review';out.mkdir(exist_ok=True)
 arrays={};report=[]
 for item in selection['images']:
  i=item['i'];m=byid[i];assert all(m[k]==item[k] for k in ('image','w','h','format'))
  role=item['role'];eye=item['eye_by_eval_order'];w,h=m['w'],m['h'];fmt=m['format'];data=(directory/f'image-{i}.bin').read_bytes()
  dtype,channels={97:('<f2',4),126:('<f4',1),100:('<f4',1),78:('<i2',2)}[fmt]
  raw=np.frombuffer(data,dtype=dtype).reshape(h,w,channels).astype(np.float32)
  arr=np.maximum(-1,raw/32767.) if fmt==78 else raw;arrays[eye,role]=arr
  report.append(dict(eye_by_eval_order=eye,role=role,index=i,format=fmt,extent=[w,h],finite=bool(np.isfinite(arr).all()),min=float(arr.min()),max=float(arr.max()),percentiles=np.percentile(arr,[1,50,95,99],axis=(0,1)).tolist()))
  if role in ('color','output'):
   Image.fromarray(np.round(srgb(arr[:,:,:3])*255).astype(np.uint8)).save(out/f'eval-eye{eye}-{role}-srgb.png')
  elif role=='motion':
   pixels=arr*np.array([w,h]);heat=np.zeros((h,w,3),np.uint8);heat[:,:,:2]=np.round(np.clip(.5+pixels/8,0,1)*255).astype(np.uint8);heat[:,:,2]=128
   Image.fromarray(heat).save(out/f'eval-eye{eye}-motion-plusminus4px.png');np.save(out/f'eval-eye{eye}-motion-pixels.npy',pixels)
 for eye in [0,1]:
  # Common output-pixel crop. Input is enlarged with nearest sampling to expose its actual support.
  c=Image.open(out/f'eval-eye{eye}-color-srgb.png');o=Image.open(out/f'eval-eye{eye}-output-srgb.png');f=Image.open(a.launch/f'native-xr/frame-{manifest["frame"]}-eye-{eye}.ppm')
  for name,box in {'grass':(500,1450,1100,1950),'wall':(800,300,1400,800)}.items():
   w,h=box[2]-box[0],box[3]-box[1];sheet=Image.new('RGB',(w*3,h+28),(24,24,24));d=ImageDraw.Draw(sheet)
   for n,(im,label) in enumerate([(c.resize(o.size,Image.Resampling.NEAREST),'DLSS input (nearest scale, sRGB)'),(o,'DLSS output (sRGB)'),(f,'submitted eye (native)')]):
    sheet.paste(im.crop(box),(n*w,28));d.text((n*w+8,8),label,fill='white')
   sheet.save(out/f'eval-eye{eye}-{name}-stages.png')
 r=dict(frame=manifest['frame'],images=report,scope='Raw half-float/SNORM values retained. RGB display uses identical linear-to-sRGB clipping 0..1, no exposure fitting. Assumed eval order must be checked against final-eye geometry. Post-pair contents may be overwritten by later passes; one frame does not prove temporal correctness.')
 (out/'inspection.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(dict(frame=r['frame'],output=str(out),images=len(report))))
if __name__=='__main__':main()
