"""Verify private bow pose isolation, source copies, native FK and retirement."""
import argparse,json,collections,math,struct
from pathlib import Path
from emulate_native_grip import Engine,compare

def audit(probe):
 e=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text().splitlines()]
 ev=lambda name:[x for x in e if x['event']==name]
 created=ev('motion_bow_created');poses=ev('motion_bow_pose');retired=ev('motion_bow_pose_disposed');models={x['handle']:x for x in ev('motion_visual_disposed')}
 displays={x['serial']:x for x in ev('motion_visual_pose') if x['mount_kind']=='native_left_bow_animated'};per_serial={x['serial']:x for x in poses};ids={x['handle']:x for x in created}
 checks=dict(created=bool(created) and len(ids)==len(created),distinct_private_pose=all(x['handle']!=x['source_handle'] and x['pose_handle']!=x['source_pose'] and x['private_pose'] for x in created),known_counts=all(x['joints']==6 and x['skin_joints']==4 for x in created),reference_acquired=all(x['references_after']==x['references_before']+1 for x in created),every_display_has_pose=bool(poses) and len(per_serial)==len(poses) and set(per_serial)==set(displays),same_frame=True,source_and_palette=True,original_samples_match=True,native_fk=True,model_then_pose_retirement=bool(retired) and {x['handle'] for x in retired}==set(ids) and len(retired)==len(created),completed=any(x['result']==0 for x in ev('view_resource_hook_complete')),no_failure=not any('failure' in x['event'] for x in e))
 for x in poses:
  d=displays.get(x['serial'],{});c=ids.get(x['handle'],{})
  checks['same_frame'] &= x['frame']==d.get('frame') and x['time']==d.get('time') and x['source_handle']==d.get('source_handle') and x['us']>d.get('us',0)
  checks['source_and_palette'] &= x['pose_handle']==c.get('pose_handle') and x['pose_handle']!=x['source_pose'] and x['palette_count']==4 and x['mask']==x['sdf']==0 and x['source_unchanged'] and x['owned_equals_source']
 for x in retired:
  m=models.get(x['handle'],{});checks['model_then_pose_retirement'] &= m.get('us',10**20)<x['us'] and x['generation_invalidated'] and x['reference_released'] and x['updates']==sum(p['handle']==x['handle'] for p in poses)
 samples=[x for x in ev('native_grip_sample') if x['weapon_type']==4];engine=Engine();error=0.;matches=0
 for x in samples:
  v=per_serial.get(x['serial'])
  if not v:checks['original_samples_match']=False;continue
  raw=(probe/x['folder']/'weapon-current-joints.bin').read_bytes();parents=struct.unpack('<6H',(probe/x['folder']/'weapon-joint-parents.bin').read_bytes())
  checks['original_samples_match'] &= raw==bytes.fromhex(v['joints_bytes']) and v['source_version']==x['weapon_pose_version'];matches+=1
  actual=engine.bones(raw,parents);error=max(error,*compare(actual,raw,parents))
 checks['original_samples_match'] &= matches>=4
 checks['native_fk'] &= error<1e-5 and not engine.calls
 points=[struct.unpack_from('<3f',bytes.fromhex(x['joints_bytes']),5*48+16) for x in poses];span=max((p[2] for p in points),default=0)-min((p[2] for p in points),default=0)
 checks['string_animates']=span>.4
 checks['reused_after_switch']=bool(poses) and any(max(x['frame'] for x in poses if x['handle']==h)-min(x['frame'] for x in poses if x['handle']==h)>1000 for h in ids)
 dedicated=ev('motion_bow_eye_payload')
 eyes=dedicated or [x for x in ev('head_visibility_payload') if x['serial'] in per_serial]
 pairs=collections.Counter((x['serial'],x['eye']) for x in eyes)
 checks['both_eyes_use_private_bow']=pairs==collections.Counter((s,eye) for s in per_serial for eye in (0,1)) and all(x['serial'] in per_serial and x['hand_instance']==(per_serial[x['serial']]['handle']&0xffffff) and (per_serial[x['serial']]['source_handle']&0xffffff) in x['indices'] for x in eyes)
 checks['eye_pose_binding']=bool(dedicated) and all(x['serial'] in per_serial and x['frame']==per_serial[x['serial']]['frame'] and x['pose_handle']==per_serial[x['serial']]['pose_handle'] for x in dedicated)
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,pose_count=len(poses),source_snapshots=matches,max_native_fk_error=error,string_depth_span_m=span,created=created,retired=retired,limitation='Native local animation copied to a private VR bow. This is not physical hand-distance control or tracked projectile aiming; inspect stereo images separately.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();r=audit(a.probe);(a.probe/'private-bow-pose-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
