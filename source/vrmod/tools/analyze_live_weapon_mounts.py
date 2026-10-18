"""Audit a bounded live-item mount survey using isolated native FK and untouched source copies."""
import argparse,collections,json,math,struct
from pathlib import Path
from emulate_native_grip import Engine,combine,relative,compare,rawmat

EXPECTED={327157552:(4,9),3474096535:(3,8),3851016100:(6,8),3936779840:(8,9),1940764111:(7,9)}
def audit(probe,expected_profile=None):
 if expected_profile is None:expected_profile=EXPECTED
 events=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 rows=[e for e in events if e['event']=='native_grip_sample']
 parents=struct.unpack('<102H',(probe/'hand-pose/joint-parents.bin').read_bytes())
 engine=Engine();results=[];counts=collections.Counter(e['item'] for e in rows)
 categories=[e for e in events if e['event']=='attachment_weapon_category']
 hands={e['serial']:e for e in events if e['event']=='motion_hand_pose'}
 weapons={e['serial']:e for e in events if e['event']=='motion_visual_pose'}
 checks=dict(all_expected_six_samples=set(counts)==set(expected_profile) and set(counts.values())=={6},sample_budget=0<len(rows)<=60,
  unique_frames=len({e['frame'] for e in rows})==len(rows),same_frame_owner=True,category_owner=True,source_bytes=True,
  source_read_only=True,native_fk_matches=True,attachment_slot_matches=True)
 for e in rows:
  h=hands.get(e['serial'],{});w=weapons.get(e['serial'],{})
  checks['same_frame_owner'] &= h.get('frame')==w.get('frame')==e['frame'] and h.get('time')==w.get('time')==e['time'] and w.get('source_handle')==e['weapon_handle'] and w.get('item')==e['item']
  checks['category_owner'] &= (e['weapon_type'],e['attachment_slot'])==expected_profile.get(e['item']) and any(c['parent']==e['parent'] and c['entity']==e['child'] and c['item']==e['item'] and c['type']==e['weapon_type'] and c['status']=='found' for c in categories)
  checks['source_read_only'] &= e['source_unchanged'] and e['read_only'] and e['engine_phase']=='after_cpu_collectors_before_original_render'
  folder=probe/e['folder'];hb=(folder/'hand-model.bin').read_bytes();wb=(folder/'weapon-model.bin').read_bytes();pb=(folder/'pose-record.bin').read_bytes();jb=(folder/'joints.bin').read_bytes();ab=(folder/'attachment.bin').read_bytes();mb=(folder/'weapon-render-model.bin').read_bytes()
  checks['source_bytes'] &= len(hb)==len(wb)==432 and len(pb)==104 and len(jb)==4896 and len(ab)==48 and len(mb)==320 and struct.unpack_from('<I',hb,0x120)[0]==e['pose_handle'] and struct.unpack_from('<Q',pb,24)[0]==e['pose_version'] and ab[40]==e['attachment_target'] and ab[41]==e['attachment_slot'] and struct.unpack_from('<I',ab,44)[0]==e['parent']
  bones=engine.bones(jb,parents);em,ep=compare(bones,jb,parents);checks['native_fk_matches'] &= max(em,ep)<1e-5
  origin=hb[0x38:0x50];hw=engine.model(hb[0x38:0x70],origin);ww=engine.model(wb[0x38:0x70],origin)
  grips=[combine(hw,bones[i]) for i in (64,94)];mounts=[relative(g,ww) for g in grips];in_weapon=[relative(ww,g) for g in grips]
  offset=[v/2**32 for v in struct.unpack_from('<3q',ab)];q=list(struct.unpack_from('<4f',ab,24));expected=combine(grips[0 if e['attachment_slot']==9 else 1],(rawmat(q,[1,1,1]),offset))
  pos_error=math.dist(expected[1],ww[1]);matrix_error=max(abs(a-b) for ar,br in zip(expected[0],ww[0]) for a,b in zip(ar,br))
  # Quantized native animation rotations and asset scale are reported separately.
  checks['attachment_slot_matches'] &= pos_error<.01 and matrix_error<.03
  results.append(dict(item=e['item'],type=e['weapon_type'],slot=e['attachment_slot'],frame=e['frame'],left_to_weapon=mounts[0],right_to_weapon=mounts[1],left_in_weapon=in_weapon[0],right_in_weapon=in_weapon[1],attachment_q32=struct.unpack_from('<3q',ab),attachment_orientation=q,slot_position_error_m=pos_error,slot_matrix_error=matrix_error,native_fk_error=max(em,ep),model_bounds=[struct.unpack_from('<3f',mb,16),struct.unpack_from('<3f',mb,32)],source_weapon_pose=struct.unpack_from('<I',wb,0x120)[0],scale=struct.unpack_from('<3f',wb,0x60)))
 checks['no_emulator_substitution']=not engine.calls
 checks['completed_hook']=any(e['event']=='view_resource_hook_complete' and e['result']==0 for e in events)
 checks['no_runtime_failure']=not any('failure' in e['event'] for e in events)
 groups=[]
 for item in sorted(counts):
  rs=[r for r in results if r['item']==item]
  groups.append(dict(item=item,type=rs[0]['type'],slot=rs[0]['slot'],frames=[r['frame'] for r in rs],left_position_range=[[min(r['left_in_weapon'][1][i] for r in rs),max(r['left_in_weapon'][1][i] for r in rs)] for i in range(3)],right_position_range=[[min(r['right_in_weapon'][1][i] for r in rs),max(r['right_in_weapon'][1][i] for r in rs)] for i in range(3)],slot_position_error_max=max(r['slot_position_error_m'] for r in rs),slot_matrix_error_max=max(r['slot_matrix_error'] for r in rs),model_bounds=rs[0]['model_bounds'],source_weapon_pose=rs[0]['source_weapon_pose']))
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,expected=expected_profile,groups=groups,samples=results,calibration_approved=False,limitation='Native relative attachment evidence only. Pose-dependent opposite-hand grips need anatomy/geometry review; no automatic runtime calibration or combat authorization.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--expected',help='Explicit item:type:attachment-slot triples separated by commas; defaults to the original five-item survey.')
 a=p.parse_args();expected=None
 if a.expected:
  triples=[tuple(int(v) for v in part.split(':')) for part in a.expected.split(',')]
  if any(len(t)!=3 or not 0<t[0]<2**32 or not 0<=t[1]<=15 or t[2] not in (8,9) for t in triples) or len({t[0] for t in triples})!=len(triples):p.error('invalid or duplicate expected profile')
  expected={item:(kind,slot) for item,kind,slot in triples}
 r=audit(a.probe,expected);(a.probe/'live-native-mounts.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:v for k,v in r.items() if k!='samples'},indent=2));raise SystemExit(r['status']!='PASS')
