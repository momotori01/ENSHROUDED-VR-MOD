"""Capture one submitted XR pair after a bounded navigation command (Pillow needed)."""
import argparse,json,time
from pathlib import Path
from PIL import Image
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('label');a=p.parse_args()
if not a.label.replace('-','').replace('_','').isalnum():p.error('simple label required')
rec=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
if rec.get('runtime')!='mock' or not rec.get('mock_navigation'):p.error('opted-in Mock launch required')
folder=a.launch/'native-xr';old=set(folder.glob('frame-*-eye-0.ppm'))
request=a.launch/'xr-capture.request'
if request.exists():p.error('another request is pending')
request.write_text('navigation observation',encoding='ascii')
end=time.monotonic()+20
while time.monotonic()<end:
 new=set(folder.glob('frame-*-eye-0.ppm'))-old
 if new:
  path=max(new,key=lambda x:int(x.name.split('-')[1]));right=path.with_name(path.name.replace('eye-0','eye-1'))
  try:
   for source in (path,right):
    with Image.open(source) as im:im.load();im.save(folder/(a.label+'-'+source.name.replace('.ppm','.png')))
   frame=int(path.name.split('-')[1]);ui_png=None
   ui_meta=folder/f'ui-frame-{frame}.json';ui_raw=folder/f'ui-frame-{frame}-native.rgba8'
   if ui_meta.exists() and ui_raw.exists():
    meta=json.loads(ui_meta.read_text());w,h=meta['width'],meta['height'];raw=ui_raw.read_bytes()
    if meta.get('ui_readback_captured') and len(raw)==w*h*4:
     ui_png=folder/f'{a.label}-frame-{frame}-ui.png';Image.frombytes('RGBA',(w,h),raw).save(ui_png)
   print(json.dumps({'frame':frame,'left':str(folder/(a.label+'-'+path.name.replace('.ppm','.png'))),'ui':str(ui_png) if ui_png else None}));break
  except (OSError,ValueError):pass
 time.sleep(.25)
else:raise RuntimeError('capture timed out')
