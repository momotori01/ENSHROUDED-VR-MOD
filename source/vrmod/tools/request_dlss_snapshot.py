"""Request one bounded, same-pair DLSS color/depth/motion snapshot from a Mock run."""
import argparse,json,time
from pathlib import Path

def events(path):
 if not path.exists():return []
 result=[]
 for line in path.read_bytes().splitlines():
  try:result.append(json.loads(line))
  except ValueError:pass
 return result

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('--frame',type=int,default=1100);p.add_argument('--timeout',type=int,default=300);p.add_argument('--count',type=int,default=1,choices=[1,2,3,4]);a=p.parse_args()
 config=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'));assert config['runtime']=='mock' and config['mock_navigation'] and config['eye_image_sweep'] and not config['renderdoc']
 request=a.launch/'eye-dump.request';record=a.launch/'dlss-snapshot-request.json';assert not record.exists() and not request.exists()
 deadline=time.monotonic()+a.timeout;selection=None
 while time.monotonic()<deadline:
  runtime=events(a.launch/'native-xr/openxr.jsonl');frame=max((e.get('frame',0) for e in runtime),default=0)
  closed=any(e['event']=='native_xr_closed' for e in runtime)
  inventory=[e for e in runtime if e['event']=='native_eye_sweep_inventory']
  probes=sorted(a.launch.glob('view-resource-probe-*/world-session.txt'))
  if selection is None and inventory and probes and frame>=a.frame:
   es=events(probes[-1].with_name('view-resource-probe.jsonl'));eyes={}
   for e in es:
    if e['event']=='dlss_eval' and e['output']['w']==1963:eyes[e['output']['image']]=e
   assert len(eyes)==2,'expected two independent eye outputs'
   images={e['image']:e for e in inventory[-1]['images']};selection=[]
   for eye,e in enumerate(eyes.values()):
    for role in ['color','output','depth','motion']:
     im=images[e[role]['image']];assert im['readable'] and im['format']==e[role]['format'] and [im['w'],im['h']]==[e[role]['w'],e[role]['h']]
     selection.append(dict(eye_by_eval_order=eye,role=role,**im))
   assert len(set(x['i'] for x in selection))==8,'per-eye resources alias'
   temp=request.with_suffix('.tmp');temp.write_text(('EVR_EYE_IMAGE_DUMP_1 ' if a.count==1 else f'EVR_EYE_IMAGE_DUMP_2 {a.count} ')+' '.join(str(x['i']) for x in selection),encoding='ascii');temp.rename(request)
   record.write_text(json.dumps(dict(requested_frame=a.frame,observed_frame=frame,dump_count=a.count,images=selection,evals=list(eyes.values()),scope='API handles mapped to eye inventory. Post-pair dedicated image snapshot; contents must be checked for subsequent overwrite.'),indent=2)+'\n')
   print('requested',frame,[(x['role'],x['i']) for x in selection],flush=True)
  dumped=[e for e in runtime if e['event']=='native_eye_dump']
  if dumped:
   assert all('error' not in d and d['read']==8 and d['skipped']==0 for d in dumped)
   if len(dumped)==a.count:
    frames=[d['frame'] for d in dumped];assert frames==list(range(frames[0],frames[0]+a.count))
    print('complete',frames,flush=True);return
  if closed:raise RuntimeError('session closed before snapshot')
  time.sleep(.5)
 raise RuntimeError('snapshot timeout')
if __name__=='__main__':main()
