"""Check render-only camera-mode transitions. Does not certify actual cutscenes."""
import argparse,json,pathlib,collections

def inspect(rows):
 def events(name):return [r for r in rows if r['event']==name]
 roots=events('root_camera_sample');done=events('live_native_pair_complete');anchors={r['recorded_xr_frame']:r for r in events('recorded_xr_anchor')}
 submitted={r['frame'] for r in done};actual=[r for r in roots if r['frame'] in submitted]
 modes=[r['presentation_id'] for r in actual];order=[x for i,x in enumerate(modes) if i==0 or modes[i-1]!=x]
 third={r['frame'] for r in roots if r['presentation_id'] in (30,36,255)}
 first={r['frame'] for r in roots}-third
 published={r['frame'] for r in events('input_pose_published')};invalid={r['frame'] for r in events('input_pose_native_camera')};settling={r['frame'] for r in events('input_pose_height_settling')}
 calibration=events('root_heading_calibrated');native=events('camera_control_observer_removed')
 checks={
  'fixture_at_least_600_completed_pairs':len(done)>=600,
  'fixture_explicitly_marked_synthetic':bool(roots) and all(r.get('synthetic_presentation') and r.get('camera_modes') for r in roots),
  'all_five_presentation_phases':len(order)==5 and order[0]==order[-1] and order[1:4]==[30,36,255],
  'real_native_camera_id_not_rewritten':bool(roots) and len({r['camera_id'] for r in roots})==1 and all(r['camera_id'] not in (30,36,255) for r in roots),
  'event_is_native_anchor':bool(third) and all(anchors[f]['anchor_kind']=='native_event_or_special_camera_diagnostic' for f in third),
  'first_person_restored':bool(first) and order[-1]==order[0] and all(anchors[f]['anchor_kind']=='stable_root_first_person_diagnostic' for f in first),
  'same_tracking_origin_epoch':len({r['origin_epoch'] for r in events('input_pose_published')})==1,
  'same_runtime_session_generation':len({r['session_generation'] for r in events('input_pose_published')})==1,
  'heading_calibrated_once_across_event':len(calibration)==1,
  'first_person_head_input_published':published|settling==first and not (published&settling) and (not settling or max(settling)<min(published,default=-1)),
  'native_modes_invalidate_head_input':invalid==third and not (published&invalid),
  'no_native_frame_has_published_head_aim':not any(r['frame'] in third for r in events('input_head_walking_sample') if r['applied']),
  'native_control_observer_restored':len(native)==1 and native[0]['restored'] and native[0]['failures']==0 and native[0]['callbacks']==0,
  'no_stereo_series_failure':not events('calibration_series_failure') and not events('native_view_probe_failure'),
 }
 return checks,dict(completed_pairs=len(done),presentation_order=order,presentation_counts=dict(collections.Counter(modes)),first_person_frames=len(first),native_frames=len(third))
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args()
 rows=[json.loads(l) for l in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
 checks,evidence=inspect(rows)
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},evidence=evidence,limits=__doc__)
 (a.probe/'camera-mode-checks.json').write_text(json.dumps(report,indent=2))
 for name,ok in checks.items():print('PASS' if ok else 'FAIL',name)
 print('RESULT',sum(checks.values()),'PASS',len(checks)-sum(checks.values()),'FAIL')
 return not all(checks.values())
if __name__=='__main__':raise SystemExit(main())
