"""Same-render-boundary native hand/weapon evidence; FK is exploratory, not approved calibration."""
import argparse,copy,json,math,struct
from pathlib import Path
from analyze_native_weapon_grip_affine import model,pose,combine,mv,inv,mat
from check_recorded_xr_native import multiply,normalize

def analyze(probe,events=None,mutate=None):
 events=events if events is not None else [json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 rows=[e for e in events if e['event']=='native_grip_sample'];hands={e['serial']:e for e in events if e['event']=='motion_hand_pose'};weapons={e['serial']:e for e in events if e['event']=='motion_visual_pose'};pairs={e['serial']:e for e in events if e['event']=='live_native_pair_complete'}
 categories=[e for e in events if e['event']=='attachment_weapon_category'];parents=struct.unpack('<102H',(probe/'hand-pose/joint-parents.bin').read_bytes())
 checks=dict(bounded_twenty_samples=len(rows)==20,unique_ordered_frames=len({e['frame'] for e in rows})==len(rows) and all(b['frame']>a['frame'] for a,b in zip(rows,rows[1:])),same_frame_and_source=bool(rows),category_owner=bool(rows),original_model_pose_bytes=bool(rows),read_only_boundary=bool(rows),valid_joint_hierarchy=all(p==65535 or p<i for i,p in enumerate(parents)))
 results=[]
 for e in rows:
  serial=e['serial'];h=hands.get(serial,{});w=weapons.get(serial,{})
  checks['same_frame_and_source'] &= bool(h and w) and h.get('frame')==w.get('frame')==e['frame'] and h.get('time')==w.get('time')==e['time'] and w.get('source_handle')==e['weapon_handle'] and e['hand_handle']!=h.get('handle') and e['pose_handle']!=h.get('pose_handle')
  checks['read_only_boundary'] &= e['read_only'] is True and e['source_unchanged'] is True and e['engine_phase']=='after_cpu_collectors_before_original_render'
  checks['category_owner'] &= any(c['entity']==e['child'] and c['parent']==e['parent'] and c['item']==e['item'] and c['type']==e['weapon_type'] and c['hash']==e['weapon_hash'] and c['status']=='found' for c in categories)
  folder=probe/e['folder'];hb=(folder/'hand-model.bin').read_bytes();wb=(folder/'weapon-model.bin').read_bytes();pb=(folder/'pose-record.bin').read_bytes();jb=(folder/'joints.bin').read_bytes();ab=(folder/'attachment.bin').read_bytes()
  if mutate:hb,wb,pb,jb,ab=mutate(hb,wb,pb,jb,ab)
  checks['original_model_pose_bytes'] &= len(hb)==len(wb)==432 and len(pb)==104 and len(jb)==4896 and len(ab)==48 and struct.unpack_from('<I',hb,0x120)[0]==e['pose_handle'] and struct.unpack_from('<Q',pb,24)[0]==e['pose_version'] and struct.unpack_from('<I',pb,12)[0]==102 and ab[40]==e['attachment_target'] and ab[41]==e['attachment_slot'] and struct.unpack_from('<I',ab,44)[0]==e['parent']
  world,hs=model(hb[0x38:0x70]);wp,ws=model(wb[0x38:0x70]);bones=[];scaleError=normError=0
  for i,parent in enumerate(parents):
   v=struct.unpack_from('<12f',jb,i*48);checks['valid_joint_hierarchy'] &= all(math.isfinite(x) for x in v) and all(.5<x<1.5 for x in v[8:11])
   local=pose(v[:4],v[4:7],v[8:11]);bones.append(local if parent==65535 else combine(bones[parent],local));scaleError=max(scaleError,max(abs(x-1) for x in v[8:11]));normError=max(normError,abs(sum(x*x for x in v[:4])-1))
  left,right=[combine(world,bones[i]) for i in (64,94)];relative=lambda a,b:mv(inv(a[0]),[u-v for u,v in zip(b[1],a[1])]);rq=right[2];iq=[-rq[0],-rq[1],-rq[2],rq[3]]
  results.append(dict(frame=e['frame'],serial=serial,paired=serial in pairs,pose_version=e['pose_version'],weapon_type=e['weapon_type'],slot=e['attachment_slot'],hand_scale=hs,weapon_scale=ws,joint_scale_error=scaleError,joint_quaternion_norm_error=normError,right_grip_to_weapon=dict(p=mv(mat(iq,[1,1,1]),[u-v for u,v in zip(wp[1],right[1])]),q=normalize(multiply(iq,wp[2]))),left_grip_in_weapon=relative(wp,left),right_grip_in_weapon=relative(wp,right),hand_distance=math.dist(left[1],right[1])))
 # Collection starts during native warmup; source copies precede XR projection submission.
 checks['includes_completed_stereo_frames']=any(e['serial'] in pairs for e in rows)
 checks['updated_original_pose_versions']=len({e['pose_version'] for e in rows})>10
 return checks,results

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();checks,records=analyze(a.probe);events=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
 checks['runner_and_survival_PASS']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS'
 checks['normal_exit']=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 checks['Core_Sync_clean']=(a.probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  for key,value in [('frame',0),('time',0),('weapon_handle',0),('weapon_type',255),('source_unchanged',False),('pose_version',0),('attachment_slot',8)]:
   bad=copy.deepcopy(events)
   for e in bad:
    if e['event']=='native_grip_sample':e[key]=value
   checks['reject_'+key]=not all(analyze(a.probe,bad)[0].values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},samples=records,calibration_approved=False,limitations='FK normalizes compressed quaternions. Engine scale/quaternion operation order and pose-to-attachment phase not yet proven; no automatic constants.')
 (a.probe/'native-grip-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 for key in ('left_grip_in_weapon','right_grip_in_weapon'):
  print(key,[[min(r[key][i] for r in records),max(r[key][i] for r in records)] for i in range(3)])
 print('PASS',sum(checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
