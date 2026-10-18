"""Freeze measured right-hand mounts for the two surveyed local rigid weapon assets."""
import argparse,json,math,hashlib,struct
from pathlib import Path
from prepare_grip_preview import quat,polar,to_xr
from check_recorded_xr_native import rotate

def prepare(probe,out):
 report=json.loads((probe/'live-native-mounts.json').read_text())
 if report['status']!='PASS' or not all(report['checks'].values()):raise ValueError('native survey proof required')
 events=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 samples={x['frame']:x for x in events if x['event']=='native_grip_sample'};result=[]
 for item,type_ in [(1940764111,7),(3936779840,8)]:
  rows=[r for r in report['samples'] if r['item']==item]
  if len(rows)!=6 or any(r['type']!=type_ or r['slot']!=9 or r['source_weapon_pose'] or any(not math.isfinite(s) or not .9<s<1.1 for s in r['scale']) for r in rows):raise ValueError('rigid local asset evidence differs')
  selected=min(rows,key=lambda r:sum(math.dist(r['right_in_weapon'][1],s['right_in_weapon'][1]) for s in rows))
  m,raw_p=selected['right_to_weapon'];q=quat(polar(m));q,raw_p=to_xr(q,raw_p);event=samples[selected['frame']]
  if len({samples[r['frame']]['weapon_guid'] for r in rows})!=1:raise ValueError('asset identity differs')
  native_grip=selected['right_in_weapon'][1];local_grip=[native_grip[0],native_grip[1],-native_grip[2]]
  p=[-x for x in rotate(q,[a*b for a,b in zip(local_grip,selected['scale'])])];residual=math.dist(p,raw_p)
  if residual>.002:raise ValueError('affine-to-rigid pivot residual too large')
  result.append(dict(item=item,type=type_,frame=selected['frame'],weapon_guid=event['weapon_guid'],hand_guid=event['hand_guid'],grip_to_model_XR=dict(p=p,q=q),right_in_model_XR=local_grip,affine_to_rigid_offset_difference_m=residual,source_scale=selected['scale'],native_right_position_span_m=max(math.dist(a['right_in_weapon'][1],b['right_in_weapon'][1]) for a in rows for b in rows),source_folder=event['folder']))
 out.mkdir(exist_ok=False)
 r=dict(source=str(probe),source_sha256=hashlib.sha256((probe/'live-native-mounts.json').read_bytes()).hexdigest(),profiles=result,limitation='Local rigid weapon placement only; animation frozen to a positional medoid. Does not authorize combat, calibrate physical controller comfort or implement grip sliding.')
 (out/'profiles.json').write_text(json.dumps(r,indent=2)+'\n')
 for x in result:print(json.dumps(x))
 return r
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('out',type=Path);a=p.parse_args();prepare(a.probe,a.out)
