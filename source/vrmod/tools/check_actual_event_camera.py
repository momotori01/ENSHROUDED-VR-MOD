"""Actual native Flame Altar camera routing; not every cutscene or physical HMD."""
import argparse,collections,json,pathlib
from check_root_camera import check_root

def check(probe):
 rows=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 events=lambda name:[x for x in rows if x['event']==name]
 roots=events('root_camera_sample');done=events('live_native_pair_complete');submitted={x['frame'] for x in done}
 actual=[x for x in roots if x['frame'] in submitted];native={x['frame'] for x in roots if x['camera_id']==31};first={x['frame'] for x in roots}-native
 sequence=[]
 for x in actual:
  name='altar' if x['camera_id']==31 else 'first_person'
  if not sequence or sequence[-1]!=name:sequence.append(name)
 published=events('input_pose_published');invalid=events('input_pose_native_camera');settling={x['frame'] for x in events('input_pose_height_settling')}
 phases=events('controller_menu_phase');captures=events('controller_menu_capture_requested')
 cameras=events('controller_fixture_camera');begins=events('controller_fixture_connected')
 checks={
  'explicit_altar_fixture':len(begins)==1 and begins[0].get('altar_sequence',False),
  '600_actual_stereo_pairs':len(done)==600,
  'actual_camera_ids_no_synthetic_override':bool(roots) and all(x['camera_modes'] and not x['synthetic_presentation'] and x['camera_id']==x['presentation_id'] for x in roots),
  'actual_first_altar_first_sequence':sequence==['first_person','altar','first_person'],
  'multiple_submitted_event_frames':len(native&submitted)>=2,
  'only_verified_first_person_and_altar_ids':bool(actual) and all(x['camera_id'] in (4,19,31) for x in actual),
  'native_ui_altar_and_return_observed':any(x['phase']==4 and x['screen']==12 for x in phases) and bool(phases) and phases[-1]['phase']==8 and phases[-1]['screen']==1,
  'native_camera_not_just_menu_prediction':any(x['camera_id']==31 and x['screen']==12 for x in cameras),
  'head_input_only_in_first_person':{x['frame'] for x in published}|settling==first and not settling&{x['frame'] for x in published} and (not settling or max(settling)<min((x['frame'] for x in published),default=-1)) and {x['frame'] for x in invalid}==native and all(x['invalidated'] for x in invalid),
  'same_tracking_origin_epoch_after_event':len({x['origin_epoch'] for x in published})==1,
  'same_runtime_session_generation':len({x['session_generation'] for x in published})==1,
  'event_never_applies_head_aim':not any(x['applied'] and x['frame'] in native for x in events('input_head_walking_sample')),
  'no_render_or_fixture_failure':not any(('failure' in x['event']) for x in rows),
 }
 checks.update(check_root(rows,2))
 layer=[json.loads(x) for x in (probe.parent/'native-xr/openxr.jsonl').read_text(encoding='utf-8').splitlines()]
 images={x['tag']:x for x in layer if x['event']=='native_fixture_capture_complete'}
 checks['event_image_captured_while_actual_camera31']=1 in images and images[1]['frame'] in native and images[1]['ui_submitted']
 checks['return_image_in_first_person_after_event']=2 in images and bool(native) and images[2]['frame'] in first and images[2]['frame']>max(native) and images[2]['ui_submitted']
 evidence=dict(presentation_order=sequence,native_camera_counts=dict(collections.Counter(x['camera_id'] for x in actual)),event_frames=sorted(native&submitted),captures=images)
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},evidence=evidence,limitation=__doc__)
 (probe/'actual-event-camera-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(sum(bool(x) for x in checks.values()),'PASS /',sum(not x for x in checks.values()),'FAIL')
 return all(checks.values())
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();raise SystemExit(not check(a.probe))
