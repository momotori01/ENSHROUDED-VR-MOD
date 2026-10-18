"""Audit opt-in consecutive motion images in the known illuminated test room.

A bounded visual regression, not scene-independent comfort or performance testing.
Readback affects timing. Does not replace normal-exit/resource-lifetime acceptance.
"""
import argparse,json,pathlib
from check_motion_camera import check_motion_images,Image
from PIL import ImageDraw

def check(probe,previews=False):
 def rows(p):return [json.loads(s) for s in p.read_text(encoding='utf-8-sig').splitlines() if s.strip()]
 cfg=(probe/'controller-motion-burst.txt').read_text().split()
 if len(cfg)!=2 or cfg[0]!='EVR_CONTROLLER_MOTION_BURST_1' or not 1<=int(cfg[1])<=32:raise ValueError('bounded burst policy required')
 count=int(cfg[1]);native=rows(probe/'view-resource-probe.jsonl');xr=rows(probe.parent/'native-xr/openxr.jsonl')
 captures=[x for x in xr if x['event']=='native_burst_capture_complete'];requests=[x for x in native if x['event']=='controller_motion_burst_requested']
 roots={x['frame']:x for x in native if x['event']=='root_camera_sample'};pairs={x['frame'] for x in xr if x['event']=='stereo_frame'}
 extents=[(x['width'],x['height']) for x in xr if x['event']=='swapchain']
 checks=dict(explicit_single_request=len(requests)==1 and requests[0]['count']==count,exact_capture_count=len(captures)==count,
  ordered_bounded_indices=[x['index'] for x in captures]==list(range(1,count+1)),
  all_pairs_submitted_with_ui=bool(captures) and all(x['frame'] in pairs and x['ui_submitted'] for x in captures),
  same_total_all_frames=bool(captures) and all(x['total']==count for x in captures),
  no_capture_cancelled=not any(x['event']=='native_burst_capture_cancelled' for x in xr))
 frames=[x['frame'] for x in captures]
 checks['consecutive_xr_frames']=bool(frames) and frames==list(range(frames[0],frames[0]+count))
 checks['capture_after_request']=len(requests)==1 and bool(captures) and captures[0]['us']>=requests[0]['us']
 images=[];evidence=[]
 for capture in captures:
  frame=capture['frame'];paths=[probe.parent/'native-xr'/f'frame-{frame}-eye-{eye}.ppm' for eye in (0,1)]
  pair=[Image.open(p).convert('RGB') for p in paths if p.exists()]
  sample_checks,stats=check_motion_images({1:pair},extents)
  checks.update({f'frame_{frame}_{k}':v for k,v in sample_checks.items() if k.startswith('image_')})
  root=roots.get(frame,{})
  checks[f'frame_{frame}_actual_root_bound']=bool(root) and not root['synthetic_presentation']
  evidence.append(dict(frame=frame,camera=root.get('camera_id'),root_q32=root.get('stable_q32'),stats=stats.get(1)))
  images.append(pair)
 if frames:
  checks['glider_and_return_in_burst']=any(e['camera']==25 for e in evidence) and evidence[-1]['camera'] in (4,19)
 if images and len(images[0])==len(images[-1])==2:
  update,_=check_motion_images({1:images[0],2:images[-1]},extents)
  checks['first_to_last_updates_both_eyes']=update['motion_images_updated_both_eyes']
 if previews and images:
  contact=Image.new('RGB',(1280,202*((len(images)+3)//4)),(20,20,20));draw=ImageDraw.Draw(contact)
  for n,pair in enumerate(images):
   x=(n%4)*320;y=(n//4)*202;draw.text((x+3,y+3),f"f{frames[n]} Camera{evidence[n]['camera']}",fill='white')
   for eye,im in enumerate(pair):contact.paste(im.resize((160,180)),(x+eye*160,y+22))
  contact.save(probe/'motion-burst-contact.png')
 report=dict(checks=checks,passed=sum(checks.values()),failed=sum(not v for v in checks.values()),frames=evidence,limitations=__doc__)
 (probe/'motion-burst-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(report['passed'],'PASS /',report['failed'],'FAIL')
 return all(checks.values())
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--previews',action='store_true');a=p.parse_args();raise SystemExit(not check(a.probe,a.previews))
