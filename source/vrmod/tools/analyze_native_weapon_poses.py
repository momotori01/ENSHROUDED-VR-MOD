"""Audit read-only animated-weapon copies; describe joints and vanilla ranged states."""
import argparse,collections,json,math,struct,hashlib
from pathlib import Path
from emulate_native_grip import Engine,compare

def audit(probe):
 events=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 samples=[e for e in events if e['event']=='native_grip_sample' and e.get('weapon_pose_copied')]
 checks=dict(animated_samples=bool(samples),bounded=len(samples)<=60,source_read_only=True,metadata_identity=True,parent_order=True,native_fk=True,finite_joints=True,completed=any(e['event']=='view_resource_hook_complete' and e['result']==0 for e in events),no_failure=not any('failure' in e['event'] for e in events))
 engine=Engine();rows=[];max_error=0.
 for e in samples:
  folder=probe/e['folder'];n=e['weapon_joint_count'];skin=e['weapon_skin_count']
  files={f.name:f.read_bytes() for f in folder.glob('weapon-*.bin')}
  names=struct.unpack('<'+'I'*n,files['weapon-joint-names.bin']);parents=struct.unpack('<'+'H'*n,files['weapon-joint-parents.bin']);mapping=struct.unpack('<'+'H'*skin,files['weapon-skinning-map.bin'])
  rec=files['weapon-pose-record.bin'];hier=files['weapon-hierarchy.bin'];owner=files['weapon-hierarchy-owner.bin'];model=files['weapon-render-model.bin'];raw=files['weapon-current-joints.bin'];default=files['weapon-default-joints.bin'][:n*48]
  checks['source_read_only'] &= e['read_only'] and e['source_unchanged'] and e['engine_phase']=='after_cpu_collectors_before_original_render'
  checks['metadata_identity'] &= hier[:16]==owner[:16]==model[148:164] and struct.unpack_from('<I',hier,48)[0]==struct.unpack_from('<I',rec,12)[0]==struct.unpack_from('<H',model,314)[0]==n and struct.unpack_from('<Q',rec,24)[0]==e['weapon_pose_version'] and len(raw)==n*48 and len(files['weapon-default-joints.bin'])==((n+3)&~3)*48 and len(files['weapon-skin-to-joint.bin'])==n*64 and all(k<n for k in mapping) and struct.unpack_from('<I',files['weapon-model.bin'],0x120)[0]==e['weapon_pose_handle']
  ordered=all(p==65535 or p<i for i,p in enumerate(parents));checks['parent_order'] &= ordered
  if not ordered:continue
  for b in [raw,default]:
   checks['finite_joints'] &= all(math.isfinite(x) for x in struct.unpack('<'+'f'*(len(b)//4),b))
  actual=engine.bones(raw,parents);base=engine.bones(default,parents)
  em,ep=compare(actual,raw,parents);max_error=max(max_error,em,ep);checks['native_fk'] &= max(em,ep)<1e-5
  joints=[]
  for i,(a,b) in enumerate(zip(actual,base)):
   vals=struct.unpack_from('<12f',raw,i*48)
   joints.append(dict(index=i,parent=parents[i],name_hash=f'{names[i]:08x}',skin=i in mapping,position=a[1],default_position=b[1],distance_from_default_m=math.dist(a[1],b[1]),local_orientation=vals[:4],local_position=vals[4:7]))
  rows.append(dict(frame=e['frame'],us=e['us'],item=e['item'],type=e['weapon_type'],weapon_guid=e['weapon_guid'],hierarchy_guid=hier[:16].hex(),count=n,skin_count=skin,joints=joints,source_hashes={k:hashlib.sha256(v).hexdigest() for k,v in files.items()}))
 checks['no_native_substitution']=not engine.calls
 ranged=[e for e in events if e['event']=='input_bow_sample'];worlds={}
 for e in ranged:
  b=bytes.fromhex(e['weapon_bytes']);key=e['world']+' / '+e['query_state'];w=worlds.setdefault(key,dict(samples=0,states={},transitions=[]))
  state=b[0];w['samples']+=1;w['states'][state]=w['states'].get(state,0)+1
  if not w['transitions'] or w['transitions'][-1]['state']!=state:
   w['transitions'].append(dict(frame=e['frame'],us=e['us'],state=state,weapon=struct.unpack_from('<I',b,0x4c)[0],ammo=struct.unpack_from('<I',b,0x60)[0],missing_resources=struct.unpack_from('<H',b,0x6c)[0],fire_queued=b[0x39],trigger=[b[0x40],b[0x41]]))
 checks['ranged_read_only']=bool(ranged) and len(ranged)<=2048 and all(e['read_only'] and e['source_unchanged'] and len(bytes.fromhex(e['weapon_bytes']))==133 and len(bytes.fromhex(e['camera_bytes']))==44 for e in ranged)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,max_native_fk_error=max_error,samples=rows,ranged_worlds=worlds,limitation='Read-only anatomy/state evidence. No projectile, physical draw, VR deformation or combat completion proof.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();r=audit(a.probe);(a.probe/'native-weapon-poses.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:v for k,v in r.items() if k!='samples'},indent=2));raise SystemExit(r['status']!='PASS')
