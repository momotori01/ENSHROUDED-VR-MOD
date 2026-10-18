"""Make an ungraded eye-image contact sheet for visual scenery review."""
import argparse,json
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--frames',required=True);p.add_argument('--name',default='scenery-contact');p.add_argument('--eye',type=int,choices=[0,1],default=1);a=p.parse_args()
frames=[int(x) for x in a.frames.split(',')];assert a.name.replace('-','').replace('_','').isalnum()
w=650;h=690;out=Image.new('RGB',(2*w,((len(frames)+1)//2)*h),(20,20,20));d=ImageDraw.Draw(out);font=ImageFont.truetype('C:/Windows/Fonts/meiryo.ttc',23)
for n,fr in enumerate(frames):
 im=Image.open(a.launch/'native-xr'/f'frame-{fr}-eye-{a.eye}.ppm');x=(n%2)*w;y=(n//2)*h
 out.paste(im.resize((w,w),Image.Resampling.LANCZOS),(x,y+40));d.text((x+10,y+5),f'フレーム {fr}  '+('左目' if a.eye==0 else '右目'),font=font,fill='white')
path=a.launch/'native-xr'/(a.name+'.jpg');out.save(path,quality=95);print(path)
