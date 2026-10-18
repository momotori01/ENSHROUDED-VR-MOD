"""Read-only shield anatomy: compare each native bone, preserve hand-relative measurements."""
import argparse,json,math,struct
from pathlib import Path
from emulate_native_grip import Engine,compare,combine,relative,rawmat

def audit(probe):
 events=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 samples=[x for x in events if x['event']=='native_shield_grip_sample']
 parents=struct.unpack('<102H',(probe/'hand-pose/joint-parents.bin').read_bytes())
 names=struct.unpack('<102I',(probe/'hand-pose/joint-names.bin').read_bytes())
 checks=dict(six_samples=len(samples)==6,one_asset=len({(e['item'],tuple(e['shield_guid_words'])) for e in samples})==1,source_read_only=True,source_sizes=True,same_frame=True,native_fk=True,attachment_owner=True,bone_match=True,completed=any(e['event']=='view_resource_hook_complete' and e['result']==0 for e in events),no_failure=not any('failure' in e['event'] for e in events))
 hands={x['serial']:x for x in events if x['event']=='motion_hand_pose'};engine=Engine();rows=[]
 for e in samples:
  p=probe/e['folder'];hand=(p/'hand-model.bin').read_bytes();shield=(p/'shield-model.bin').read_bytes();raw=(p/'joints.bin').read_bytes();rec=(p/'pose-record.bin').read_bytes();at=(p/'attachment.bin').read_bytes();model=(p/'shield-render-model.bin').read_bytes()
  checks['source_read_only'] &= e['read_only'] and e['source_unchanged'] and e['engine_phase']=='after_cpu_collectors_before_original_render'
  checks['source_sizes'] &= len(hand)==len(shield)==432 and len(raw)==4896 and len(rec)==104 and len(at)==48 and len(model)==320 and struct.unpack_from('<I',hand,0x120)[0]==e['pose_handle'] and struct.unpack_from('<Q',rec,24)[0]==e['pose_version']
  h=hands.get(e['serial'],{});checks['same_frame'] &= h.get('frame')==e['frame'] and h.get('time')==e['time'] and h.get('source_unchanged',False)
  checks['attachment_owner'] &= e['item_slot']==18 and at[40]==0 and at[41]==e['attachment_slot'] and struct.unpack_from('<I',at,44)[0]==e['parent']
  bones=engine.bones(raw,parents);err=max(compare(bones,raw,parents));checks['native_fk'] &= err<1e-5
  origin=hand[0x38:0x50];hw=engine.model(hand[0x38:0x70],origin);sw=engine.model(shield[0x38:0x70],origin)
  q=struct.unpack_from('<4f',at,24);pos=[v/2**32 for v in struct.unpack_from('<3q',at)];local=(rawmat(q,[1,1,1]),pos)
  candidates=[]
  for i,b in enumerate(bones):
   actual=combine(combine(hw,b),local);pe=math.dist(actual[1],sw[1]);me=max(abs(a-b) for ar,br in zip(actual[0],sw[0]) for a,b in zip(ar,br));candidates.append(dict(bone=i,name=f'{names[i]:08x}',position_error_m=pe,matrix_error=me,score=pe+.1*me))
  candidates.sort(key=lambda v:v['score']);best=candidates[0];checks['bone_match'] &= best['position_error_m']<.002 and best['matrix_error']<.03
  left=combine(hw,bones[64]);right=combine(hw,bones[94])
  rows.append(dict(frame=e['frame'],item=e['item'],slot=e['attachment_slot'],guid=e['shield_guid_words'],closest_bones=candidates[:5],left_to_shield=relative(left,sw),right_to_shield=relative(right,sw),left_in_shield=relative(sw,left),bounds=[struct.unpack_from('<3f',model,16),struct.unpack_from('<3f',model,32)],scale=struct.unpack_from('<3f',shield,0x60),pose_handle=struct.unpack_from('<I',shield,0x120)[0],native_fk_error=err))
 checks['same_native_bone']=bool(rows) and len({r['closest_bones'][0]['bone'] for r in rows})==1
 checks['no_emulator_substitution']=not engine.calls
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,samples=rows,calibration_approved=False,limitation='Native anatomy only. No tracked shield rendering, guard, comfort or all-asset calibration proof.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();r=audit(a.probe);(a.probe/'native-shield-grip-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
