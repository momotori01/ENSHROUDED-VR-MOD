"""Pair exact camera frames from the bounded in-session fog A/B (second motion starts600 later)."""
import argparse,json,re
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import numpy as np
p=argparse.ArgumentParser();p.add_argument('launch',type=Path);a=p.parse_args();root=a.launch/'native-xr';cs={}
for f in a.launch.glob('view-resource-probe-*/world-session.txt'):
 for line in f.with_name('view-resource-probe.jsonl').open('rb'):
  e=json.loads(line)
  if e.get('event')=='captured_game_camera':cs[e['frame'],e['eye']]=e
frames={int(re.search(r'frame-(\d+)',p.name)[1]) for p in root.glob('frame-*-eye-0.ppm')};pairs=[(f,f+600) for f in sorted(frames) if 700<=f<1300 and f+600 in frames];assert len(pairs)>=30
errors=[]
for before,after in pairs:
 for eye in (0,1):
  ca=cs[before,eye];cb=cs[after,eye];errors.append(dict(before=before,after=after,eye=eye,position_error_m=max(abs(x-y) for x,y in zip(ca['camera_q32'],cb['camera_q32']))/2**32,orientation_error=max(abs(x-y) for x,y in zip(ca['orientation'],cb['orientation'])),projection_equal=ca['descriptor']==cb['descriptor']))
assert all(e['position_error_m']<1e-6 and e['orientation_error']<1e-6 and e['projection_equal'] for e in errors)
selected=[pairs[0],pairs[len(pairs)//2-1],pairs[len(pairs)//2],pairs[-1]];font=ImageFont.truetype('C:/Windows/Fonts/meiryo.ttc',21)
for eye in (0,1):
 c=Image.new('RGB',(1400,len(selected)*725),(20,20,20));d=ImageDraw.Draw(c)
 for j,(b,e) in enumerate(selected):
  for k,f in enumerate([b,e]):
   with Image.open(root/f'frame-{f}-eye-{eye}.ppm') as im:c.paste(im.resize((700,700)),(k*700,j*725+25))
   d.text((k*700+4,j*725),f'{"修正前" if k==0 else "投影一致後"} {"左目" if eye==0 else "右目"} {f}',font=font,fill='white')
 c.save(root/f'fog-ab-eye-{eye}.jpg',quality=94)
r=dict(status='PASS',matched_pairs=len(pairs),rows=errors,limitation='Camera correspondence only. Same session but time/weather/wind/hands still vary. No numerical visual-quality pass.')
(a.launch/'fog-ab-camera-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print('PASS',len(pairs),'same-camera pairs')
