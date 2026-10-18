"""Verify reviewed staff/wand private poses, source copies, native FK and retirement."""
import argparse,json,collections,math,struct
from pathlib import Path
from emulate_native_grip import Engine,compare

def audit(probe):
 e=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 ev=lambda name:[x for x in e if x['event']==name]
 created=ev('motion_ranged_created');poses=ev('motion_ranged_pose');retired=ev('motion_ranged_pose_disposed');models={x['handle']:x for x in ev('motion_visual_disposed')}
 displays={x['serial']:x for x in ev('motion_visual_pose') if x['item'] in {1255239784,3851016100}};per_serial={x['serial']:x for x in poses};ids={x['handle']:x for x in created}
 checks=dict(created=bool(created) and len(ids)==len(created),distinct_private_pose=all(x['handle']!=x['source_handle'] and x['pose_handle']!=x['source_pose'] and x['private_pose'] for x in created),known_counts={x['item'] for x in created}=={1255239784,3851016100} and all(x['joints']==3 and x['skin_joints']==(2 if x['item']==1255239784 else 1) and x['type']==(5 if x['item']==1255239784 else 6) for x in created),reference_acquired=all(x['references_after']==x['references_before']+1 for x in created),every_display_has_pose=bool(poses) and len(per_serial)==len(poses) and set(per_serial)==set(displays),same_frame=True,source_and_palette=True,original_samples_match=True,native_fk=True,model_then_pose_retirement=bool(retired) and {x['handle'] for x in retired}==set(ids) and len(retired)==len(created),completed=any(x['result']==0 for x in ev('view_resource_hook_complete')),no_failure=not any('failure' in x['event'] for x in e))
 for x in poses:
  d=displays.get(x['serial'],{});c=ids.get(x['handle'],{})
  checks['same_frame'] &= x['frame']==d.get('frame') and x['time']==d.get('time') and x['source_handle']==d.get('source_handle') and x['us']>d.get('us',0)
  checks['source_and_palette'] &= x['pose_handle']==c.get('pose_handle') and x['pose_handle']!=x['source_pose'] and x['palette_count']==c.get('skin_joints') and len(bytes.fromhex(x['joints_bytes']))==144 and x['item']==c.get('item') and x['mask']==x['sdf']==0 and x['source_unchanged'] and x['owned_equals_source']
 for x in retired:
  m=models.get(x['handle'],{});checks['model_then_pose_retirement'] &= m.get('us',10**20)<x['us'] and x['generation_invalidated'] and x['reference_released'] and x['updates']==sum(p['handle']==x['handle'] for p in poses)
 samples=[x for x in ev('native_grip_sample') if x['weapon_type'] in (5,6)];engine=Engine();error=0.;matches=0
 for x in samples:
  v=per_serial.get(x['serial'])
  if not v:checks['original_samples_match']=False;continue
  raw=(probe/x['folder']/'weapon-current-joints.bin').read_bytes();parents=struct.unpack('<3H',(probe/x['folder']/'weapon-joint-parents.bin').read_bytes())
  checks['original_samples_match'] &= raw==bytes.fromhex(v['joints_bytes']) and v['source_version']==x['weapon_pose_version'];matches+=1
  actual=engine.bones(raw,parents);error=max(error,*compare(actual,raw,parents))
 checks['original_samples_match'] &= matches==12 and collections.Counter(x['weapon_type'] for x in samples)=={5:6,6:6}
 checks['native_fk'] &= error<1e-5 and not engine.calls
 checks['both_assets_update']=all(sum(x['item']==item for x in poses)>100 for item in (1255239784,3851016100))
 dedicated=ev('motion_ranged_eye_payload')
 eyes=dedicated or [x for x in ev('head_visibility_payload') if x['serial'] in per_serial]
 pairs=collections.Counter((x['serial'],x['eye']) for x in eyes)
 checks['both_eyes_use_private_ranged']=pairs==collections.Counter((s,eye) for s in per_serial for eye in (0,1)) and all(x['serial'] in per_serial and x['hand_instance']==(per_serial[x['serial']]['handle']&0xffffff) and (per_serial[x['serial']]['source_handle']&0xffffff) in x['indices'] for x in eyes)
 checks['eye_pose_binding']=bool(dedicated) and all(x['serial'] in per_serial and x['frame']==per_serial[x['serial']]['frame'] and x['pose_handle']==per_serial[x['serial']]['pose_handle'] for x in dedicated)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,pose_count=len(poses),source_snapshots=matches,max_native_fk_error=error,created=created,retired=retired,limitation='Native local animation copied to two reviewed staff/wand models. Projectile aim, casting gameplay, all assets and hardware comfort are not established; inspect stereo images separately.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();r=audit(a.probe);(a.probe/'private-ranged-pose-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
