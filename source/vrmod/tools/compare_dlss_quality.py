"""Compare unmodified saved eye crops for matched fixed-camera quality runs (Pillow/NumPy)."""
import argparse,json,re
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('launches',type=Path,nargs='+');a=p.parse_args();a.output.mkdir(exist_ok=True,parents=True)
 rois={'far_cliff':(1000,770,1384,1026),'tree':(625,650,881,970),'stone_floor':(840,1310,1224,1566)}
 reports=[];crops={}
 for launch in a.launches:
  audit=json.loads((launch/'dlss-quality-audit.json').read_text());label=audit['quality']+'-sharp'+str(json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))['vr_sharpness'])
  burst=json.loads((launch/'scenery-burst-requests.json').read_text());assert burst['status']=='PASS'
  captured=[]
  for line in (launch/'native-xr/openxr.jsonl').open():
   e=json.loads(line)
   if e.get('event')=='native_burst_capture_complete':captured.append(e['frame'])
  assert len(captured)==32
  for eye in (0,1):
   stats={}
   for name,box in rois.items():
    images=[Image.open(launch/f'native-xr/frame-{f}-eye-{eye}.ppm').crop(box) for f in captured]
    pixels=np.stack([np.asarray(im,dtype=np.float32) for im in images]);delta=np.abs(np.diff(pixels,axis=0))
    stats[name]={'mean_adjacent_rgb_difference_255':float(delta.mean()),'p95_adjacent_rgb_difference_255':float(np.percentile(delta,95)),'pixel_fraction_any_channel_difference_over_10':float((delta.max(axis=3)>10).mean())}
    crops[label,eye,name]=images[16];images[16].save(a.output/f'{label}-eye-{eye}-{name}.png')
   reports.append(dict(launch=str(launch),quality=label,eye=eye,frame=captured[16],samples=len(captured),regions=stats))
 for eye in (0,1):
  for name,box in rois.items():
   w,h=box[2]-box[0],box[3]-box[1];canvas=Image.new('RGB',(w*len(a.launches),h+32),(24,24,24));draw=ImageDraw.Draw(canvas)
   for i,launch in enumerate(a.launches):
    label=json.loads((launch/'dlss-quality-audit.json').read_text())['quality']+'-sharp'+str(json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))['vr_sharpness']);draw.text((i*w+8,8),label,fill='white');canvas.paste(crops[label,eye,name],(i*w,32))
   canvas.save(a.output/f'comparison-eye-{eye}-{name}.png')
 report=dict(regions=rois,comparisons=reports,limitations='Unscaled uncorrected 1:1 crops. Time/weather/wind may differ. Temporal differences include wind, shadows, lighting and jitter, not an image-quality score; static camera only. Capture waits for GPU.')
 (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(dict(output=str(a.output),comparisons=len(reports),frames_per_eye=32)))
if __name__=='__main__':main()
