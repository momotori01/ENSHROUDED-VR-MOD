"""Display native post-pair raw diagnostics; speculative formats are labeled as candidates."""
import argparse,json,math
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
p=argparse.ArgumentParser(description=__doc__);p.add_argument('dump',type=Path);a=p.parse_args()
meta=json.loads((a.dump/'manifest.json').read_text());assert meta.get('read')==len(meta['images']) and not meta.get('error')
out=a.dump/'preview';out.mkdir(exist_ok=True);tiles=[];stats=[]
font=ImageFont.truetype('C:/Windows/Fonts/meiryo.ttc',14)
def add(name,arr):
 arr=np.nan_to_num(arr,nan=0,posinf=1,neginf=0);im=Image.fromarray(np.uint8(np.clip(arr,0,1)*255));im.save(out/(name+'.png'));tiles.append((name,im))
for x in meta['images']:
 i=x['i'];w,h=x['w'],x['h'];f=x['format'];raw=(a.dump/f'image-{i}.bin').read_bytes();n=w*h
 row=dict(index=i,format=f,bytes=len(raw))
 if f==98:
  assert len(raw)==n*4;word=np.frombuffer(raw,dtype='<u4').reshape(h,w);scale=np.exp2((word>>27).astype(np.int32)-24);rgb=np.stack([(word>>b)&511 for b in (0,9,18)],-1)*scale[:,:,None];lum=rgb@np.array([.2126,.7152,.0722])
  add(f'{i}-rgb9e5-candidate',np.clip((np.log2(np.maximum(rgb,1e-6))+6)/8,0,1));row.update(rgb9e5_candidate_percentiles=np.percentile(lum,[0,10,50,90,100]).tolist(),zero_fraction=float((word==0).mean()))
 elif f==97:
  assert len(raw)==n*8;rgb=np.frombuffer(raw,dtype='<f2').reshape(h,w,4)[:,:,:3].astype(float);add(f'{i}-hdr-log',np.clip((np.log2(np.maximum(rgb,1e-6))+12)/12,0,1));row['rgb_percentiles']=np.nanpercentile(rgb,[0,10,50,90,100]).tolist()
 elif f==37:
  assert len(raw)==n*4;rgb=np.frombuffer(raw,dtype=np.uint8).reshape(h,w,4);add(f'{i}-rgba8',rgb[:,:,:3]/255)
 elif f==9:
  assert len(raw)==n;v=np.frombuffer(raw,dtype=np.uint8).reshape(h,w);add(f'{i}-r8',v/255);row['percentiles']=np.percentile(v,[0,10,50,90,100]).tolist()
 elif f==64:
  assert len(raw)==n*4;v=np.frombuffer(raw,dtype='<u4').reshape(h,w);add(f'{i}-packed-normal',np.stack([(v>>b)&1023 for b in (0,10,20)],-1)/1023)
 elif f==126:
  assert len(raw)==n*4;v=np.frombuffer(raw,dtype='<f4').reshape(h,w);add(f'{i}-depth-log',np.clip((np.log2(np.maximum(v,1e-8))+16)/16,0,1))
 stats.append(row)
canvas=Image.new('RGB',(400*4,425*math.ceil(len(tiles)/4)))
for i,(name,im) in enumerate(tiles):
 x=i%4*400;y=i//4*425;canvas.paste(im.resize((400,400)),(x,y+25));ImageDraw.Draw(canvas).text((x+3,y+3),name,fill='white',font=font)
canvas.save(out/'contact.png');(out/'statistics.json').write_text(json.dumps(dict(scope='Post-pair private image dump. RGB9E5 interpretations require sampled-view/binding validation; log mapped previews do not represent game exposure.',images=stats),indent=2)+'\n',encoding='utf-8');print(out/'contact.png')
