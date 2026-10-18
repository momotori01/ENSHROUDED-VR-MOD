"""Verify intended OpenXR Runtime loss safely ends native world VR diagnostics.

Requires ordinary game shutdown. Partial pairs are measured, never relabelled as
the requested complete run. Real runtime reconnection/session recreation and
latency are outside this check.
"""
import argparse,collections,csv,json,pathlib
from check_private_pools import check_pools
from check_water_feedback import check_water,check_packet_evidence
from check_culling_history import check_history
from check_culling_gpu_memory import check_gpu_memory
from check_head_visibility import check_head
from check_vfx_transparent import check_vfx_transparent
from check_input_direction import check_input_direction
from check_input_pose import check_input_pose
from check_idle_head_camera import check_idle_head

def load(path):return [json.loads(s) for s in path.read_text(encoding='utf-8-sig').splitlines() if s.strip()]

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();launch=a.probe.parent
    if not (launch/'process-exit.json').exists():p.error('ordinary game shutdown must be recorded first')
    native=load(a.probe/'view-resource-probe.jsonl');gpu=load(launch/'vulkan.jsonl');all_mock=load(launch/'mock.jsonl')
    lo=min(e['us'] for e in native);hi=max(e['us'] for e in native)
    xr=[e for e in load(launch/'native-xr/openxr.jsonl') if lo<=e['us']<=hi];mock=[e for e in all_mock if lo<=e['us']<=hi]
    def events(rows,name):return [e for e in rows if e['event']==name]
    with (launch/'mock-pose.csv').open() as f:poses=list(csv.reader(line for line in f if not line.startswith('#')))
    ended=events(mock,'end_frame');waits=events(mock,'wait_frame');counts=collections.Counter(e['event'] for e in mock)
    submitted=events(xr,'native_game_pair_submitted');pairs=events(native,'calibration_pair_recorded');warmups=events(native,'world_warmup_frame_ended')
    valid={e['frame'] for e in events(xr,'native_frame_begin')};warmup_ids={e['frame'] for e in warmups};pair_ids={e['frame'] for e in submitted}
    lost=events(xr,'session_state');lost=[e for e in lost if e['state']==7]
    failure=events(gpu,'native_xr_begin_failed');failure=[e for e in failure if lo<=e['us']<=hi]
    deferred=events(xr,'native_frame_deferred');closed=events(xr,'native_xr_closed');disposed=events(native,'calibration_scene_disposal_complete')
    complete=events(native,'view_resource_hook_complete');restore=events(native,'recorded_xr_projection_hook_removed')
    reasons=events(native,'calibration_series_failure');requested=int((a.probe/'live-xr-config.txt').read_text().split()[1])
    checks={
        'loss_explicit_disconnected_csv_frame':bool(waits) and len([e for e in waits if int(poses[(e['frame']-1)%len(poses)][9])==0])==1 and int(poses[(waits[-1]['frame']-1)%len(poses)][9])==0,
        'loss_single_loss_pending_event':len(lost)==1,
        'loss_expected_native_failure_only':len(failure)==len(reasons)==1 and failure[0]['reason'] in ('native XR session stopped','native XR session not ready') and reasons[0]['reason']=='live native XR begin failed',
        'loss_stops_before_requested_pair_count':0<len(submitted)==len(pairs)<requested,
        'loss_completed_pairs_have_both_eyes':bool(pairs) and collections.Counter((e['serial'],e['eye']) for e in events(native,'calibration_eye_recorded') if e['serial'] in {x['serial'] for x in pairs})==collections.Counter((e['serial'],i) for e in pairs for i in (0,1)),
        'loss_no_submission_after_runtime_stop':len(lost)==1 and all(e['us']<lost[0]['us'] and e['gpu_fence_complete'] and e['eyes']==2 for e in submitted),
        'loss_valid_frames_accounted':valid==warmup_ids|pair_ids and not warmup_ids&pair_ids,
        'loss_only_final_frame_deferred':len(deferred)==1 and deferred[0]['frame']==max(valid,default=0)+1 and deferred[0]['empty_frame_ended'],
        'loss_frame_protocol_balanced':len(waits)==len(ended)==len(valid)+1 and [e['frame'] for e in waits]==[e['frame'] for e in ended] and len({e['frame'] for e in waits})==len(waits),
        'loss_image_acquire_wait_release_balanced':counts['acquire']==counts['wait_image']==counts['release']==2*len(valid),
        'loss_projection_layers_only_for_completed_pairs':sum(e['layers']==1 for e in ended)==len(submitted) and bool(ended) and ended[-1]['layers']==0,
        'loss_restores_native_render_hook':len(complete)==1 and complete[0]['result']==1 and complete[0]['original_bytes_restored'],
        'loss_restores_projection_hook':len(restore)==1 and restore[0]['status']==0,
        'loss_closes_xr_session_once':len(closed)==counts['session_destroyed']==1,
        'loss_stale_input_never_published':not {e['frame'] for e in deferred}.intersection(e['frame'] for e in events(native,'input_pose_published')),
        'loss_no_hmd_input_after_xr_close':len(closed)==1 and all(e['us']<closed[0]['us'] for e in events(native,'input_head_walking_sample') if e['applied']),
        'loss_original_game_survives':json.loads((a.probe/'probe-postcheck.json').read_text())['status']=='PASS',
        'loss_private_views_disposed_normally':len(disposed)==1 and disposed[0]['views_destroyed']==2 and disposed[0]['owned_groups_cleared'] and len(closed)==1 and closed[0]['us']<disposed[0]['us'],
        'loss_scene_hook_removed':len(events(native,'calibration_scene_hook_removed'))==1 and events(native,'calibration_scene_hook_removed')[0]['status']==0,
        'loss_no_forced_gpu_retirement':not events(native,'calibration_gpu_retired'),
        'loss_normal_process_exit':json.loads((launch/'process-exit.json').read_text())['exit_code']==0,
        'loss_borrowed_instance_released':sum(e['event']=='instance_destroyed' for e in all_mock)==1,
        'loss_core_validation_clean':(launch/'validation.log').exists() and not (launch/'validation.log').read_text().strip(),
    }
    devices=[e for e in events(gpu,'create_device') if e.get('openxr_extensions_prepared')];deaths=events(gpu,'destroy_device');instances=events(gpu,'destroy_instance')
    checks['loss_game_device_and_parent_exit']=len(devices)==len(deaths)==1 and devices[0]['device']==deaths[0]['device'] and any(e['instance']==devices[0]['instance'] and e['us']>deaths[0]['us'] for e in instances)
    for eye in (0,1):
        images=[e for e in events(gpu,'native_eye_image_registered') if e['eye']==eye and lo<=e['us']<=hi]
        removed=[e for e in events(gpu,'render_image_destroyed') if len(images)==1 and e['image']==images[0]['image'] and e['generation']==images[0]['generation']]
        checks[f'loss_eye{eye}_image_released_once']=len(images)==len(removed)==len(disposed)==1 and disposed[0]['us']<removed[0]['us']<deaths[0]['us'] if len(deaths)==1 else False
    for values in (check_pools(native),check_water(native,True),check_history(native,True),check_gpu_memory(native,gpu,True),check_head(native,gpu)):
        checks.update(values)
    vfx_checks,vfx_metrics=check_vfx_transparent(native);checks.update(vfx_checks)
    input_checks,input_metrics=check_input_direction(native);checks.update(input_checks)
    input_checks,pose_metrics=check_input_pose(native,xr);checks.update(input_checks)
    input_checks,head_metrics=check_idle_head(native,xr,'walking');checks.update(input_checks)
    water_checks,water_hashes=check_packet_evidence(a.probe,native);checks.update(water_checks)
    report=dict(scope=__doc__,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},requested_pairs=requested,submitted_pairs=len(submitted),warmup_frames=len(warmups),
        input_metrics=input_metrics,pose_metrics=pose_metrics,head_metrics=head_metrics,vfx_metrics=vfx_metrics,water_sha256=water_hashes)
    (a.probe/'world-runtime-loss-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print(sum(checks.values()),'PASS /',sum(not v for v in checks.values()),'FAIL');return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
