"""Diagnostic reprojection residuals on stationary home grass, using captured NGX motion and jitter."""
import argparse,json
from pathlib import Path
import numpy as np

def sample(a,x,y):
 h,w=a.shape[:2];x=np.clip(x,0,w-1.001);y=np.clip(y,0,h-1.001);ix=x.astype(int);iy=y.astype(int);fx=x-ix;fy=y-iy
 if a.ndim==3:fx=fx[:,:,None];fy=fy[:,:,None]
 return (a[iy,ix]*(1-fx)+a[iy,ix+1]*fx)*(1-fy)+(a[iy+1,ix]*(1-fx)+a[iy+1,ix+1]*fx)*fy

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args();sel=json.loads((a.launch/'dlss-snapshot-request.json').read_text());items={(x['eye_by_eval_order'],x['role']):x for x in sel['images']}
 probe=sorted(a.launch.glob('view-resource-probe-*/world-session.txt'))[-1].parent;es=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 dirs=sorted((a.launch/'native-xr').glob('eye-dump-*'));assert len(dirs)==4;frames=[json.loads((d/'manifest.json').read_text())['frame'] for d in dirs];assert frames==list(range(frames[0],frames[0]+4))
 begins={e['frame']:e['us'] for e in es if e['event']=='live_native_frame_begin'};ends={e['frame']:e['us'] for e in es if e['event']=='live_native_pair_complete'}
 jitter={}
 for f in frames:
  ev=[e for e in es if e['event']=='dlss_eval' and begins[f]<=e['us']<=ends[f] and e['output']['w']==1963];assert len(ev)==2
  for eye in [0,1]:
   exact=[e for e in ev if e['output']['image']==items[eye,'output']['image']];assert len(exact)==1;jitter[f,eye]=np.array(exact[0]['jitter'])
 regions={'grass':(560,1770,685,1940),'floor':(900,1350,1200,1580)};results=[]
 for eye in [0,1]:
  m=items[eye,'motion'];w,h=m['w'],m['h'];arrays=[]
  for folder in dirs:
   arrays.append({role:np.fromfile(folder/f'image-{items[eye,role]["i"]}.bin',dtype=dt).reshape(h,w,channels).astype(np.float32) for role,dt,channels in [('color','<f2',4),('depth','<f4',1),('motion','<i2',2)]})
  for i in range(1,4):
   before,now=arrays[i-1],arrays[i];delta=jitter[frames[i-1],eye]-jitter[frames[i],eye];row=dict(eye=eye,before=frames[i-1],frame=frames[i],elapsed_between_begins_s=(begins[frames[i]]-begins[frames[i-1]])/1e6,jitter_delta=delta.tolist(),candidates=[])
   for sx,sy in [(0,0),(-1,-1),(-1,1),(1,-1),(1,1)]:
    candidate=dict(jitter_sign=[sx,sy],regions={})
    for name,box in regions.items():
     x0,y0,x1,y1=[round(v*w/1963) for v in box];yy,xx=np.mgrid[y0:y1,x0:x1];mv=now['motion'][y0:y1,x0:x1]/32767*w;mag=np.linalg.norm(mv,axis=-1)
     mask=(mag>.1)&(mag<15) if name=='grass' else np.ones(mag.shape,bool);values={'pixels':int(mask.sum())}
     for motion in [False,True]:
      x=xx+delta[0]*sx+(mv[:,:,0] if motion else 0);y=yy+delta[1]*sy+(mv[:,:,1] if motion else 0)
      rgb=sample(before['color'][:,:,:3],x,y)-now['color'][y0:y1,x0:x1,:3];z=sample(before['depth'][:,:,0],x,y)-now['depth'][y0:y1,x0:x1,0]
      tag='native_motion' if motion else 'zero_motion';values[tag]={'mean_abs_rgb':float(np.abs(rgb[mask]).mean()),'mean_abs_depth':float(np.abs(z[mask]).mean()),'p90_abs_depth':float(np.percentile(np.abs(z[mask]),90))}
     candidate['regions'][name]=values
    row['candidates'].append(candidate)
   results.append(row)
 out=a.launch/'native-xr/raw-sequence-review';out.mkdir(exist_ok=True);report=dict(results=results,scope='ROI excludes most bow pixels but is not semantic segmentation. Four explicitly slow diagnostic frames; disocclusion, animation in depth, light and input shading noise remain. Jitter signs shown as candidates. Residual reduction supports correspondence, not an image-quality verdict.')
 (out/'reprojection.json').write_text(json.dumps(report,indent=2)+'\n')
 for row in results:
  best=min(row['candidates'],key=lambda x:x['regions']['floor']['zero_motion']['mean_abs_depth']);r=best['regions']['grass'];print(row['eye'],row['frame'],best['jitter_sign'],r['pixels'],{k:r[k] for k in ['zero_motion','native_motion']})
if __name__=='__main__':main()
