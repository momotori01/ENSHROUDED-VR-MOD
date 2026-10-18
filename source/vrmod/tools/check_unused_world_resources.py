"""Check unused world View ownership, continued frames, and later game teardown.

This checks resource creation/disposal, not eye rendering or playable VR.
Only run after normal game exit. Original failures are not reclassified here.
"""
import argparse,json
from pathlib import Path
from check_private_pools import check_pools
from check_water_feedback import check_water
from check_culling_history import check_history
from check_culling_gpu_memory import check_gpu_memory

def rows(path):return [json.loads(s) for s in path.read_text(encoding='utf-8-sig').splitlines() if s.strip()]
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
    if (a.probe.parent/'process-watch.json').exists() and not (a.probe.parent/'process-exit.json').exists():
        p.error('managed process exit is not recorded yet; finish ordinary shutdown before final lifetime checks')
    native=rows(a.probe/'view-resource-probe.jsonl');gpu=rows(a.probe.parent/'vulkan.jsonl')
    post=json.loads((a.probe/'probe-postcheck.json').read_text())
    def events(name):return [e for e in native if e['event']==name]
    stages=events('unused_world_resource_diagnostic');stage=stages[0]['stage'] if len(stages)==1 else None
    views=events('native_view_created');completed=events('native_view_probe_complete');hooks=events('view_resource_hook_complete')
    shared=events('shared_resolution_hook_removed');devices=[e for e in gpu if e['event']=='create_device']
    exit_ok=False
    if len(devices)==1:
        dev=devices[0];ends=[e for e in gpu if e['event']=='destroy_device' and e['device']==dev['device'] and e['us']>dev['us']]
        parents=[e for e in gpu if e['event']=='destroy_instance' and e['instance']==dev['instance'] and e['us']>dev['us']]
        exit_ok=len(ends)==len(parents)==1 and ends[0]['us']<parents[0]['us']
    checks={
        'explicit_unused_world_stage':stage in (1,2) and stages[0]['drawn'] is False,
        'two_distinct_private_views':len(views)==2 and len({e['view'] for e in views})==2 and all(e['view']!=e['original_view'] for e in views),
        'two_feature_groups_initialized':len(events('native_view_features_initialized'))==2,
        'no_eye_render_attempted':not events('calibration_eye_begin') and not events('calibration_pair_recorded'),
        'both_views_disposed':len(completed)==1 and completed[0]['two_views_destroyed'],
        'original_view_unchanged':len(completed)==1 and completed[0]['original_view_unchanged'],
        'scene_time_unchanged':len(completed)==1 and completed[0]['scene_time_unchanged'],
        'original_back_pointer_restored':len(completed)==1 and completed[0]['back_pointer_restored'],
        'render_hook_restored_successfully':len(hooks)==1 and hooks[0]['result']==0 and hooks[0]['original_bytes_restored'],
        'all_shared_hooks_restored':len(shared)==8 and {e['function'] for e in shared}=={'0x1172c0','0x28620','0x1cc620','0x117970','0x2aa30','0x1cdd20','0x152f00','0x153fe0'} and all(e['status']==0 for e in shared),
        'no_failure_event':not events('native_view_probe_failure') and not events('view_resource_hook_failure'),
        'fresh_render_progress_after_probe':post['status']=='PASS' and post['expected_process_alive'] and post['rendering_continued_four_seconds'] and post['first_frame_us']>post['observation_start_frame_us'] and post['last_frame_us']-post['first_frame_us']>=4000000,
        'no_new_crash_during_postcheck':not post['new_crash_directories'],
        'later_game_device_and_parent_exit':exit_ok,
    }
    if stage==2:
        detail=events('private_native_detail_factory_return');arrays=events('detail_array_reserve')
        checks['two_resolution_groups_initialized']=len(events('native_view_resolution_initialized'))==2
        checks['two_native_detail_factories_succeeded']=len(detail)==2 and all(e['object']!='0x0' and e['failed_reserves']==0 for e in detail)
        checks['both_detail_array_capacities']=len(arrays)==8 and all(any(e['eye']==eye and e['requested_capacity']==capacity and e['capacity_after']==capacity and e['result'] for e in arrays) for eye in (0,1) for capacity in (512,2048))
        checks['detail_hooks_removed']=all(len(events(n))==1 and events(n)[0]['status']==0 for n in ('detail_factory_trace_hook_removed','detail_reserve_hook_removed','detail_allocator_trace_hook_removed'))
        if stages[0]['original_features']&0x8000:
            fsr=events('private_fsr3_context')
            checks['fsr3_private_temporal_resources']=len(fsr)==2 and {e['eye'] for e in fsr}=={0,1} and len({e['object'] for e in fsr})==2 and all(e['object']!=e['original'] and e['program_id'] and e['private_image_handles']>0 for e in fsr)
    if events('private_native_memory_pool_created'):checks.update(check_pools(native,stage==2))
    if (a.probe/'world-water-feedback.txt').exists():checks.update(check_water(native,False))
    if (a.probe/'world-culling-history.txt').exists():
        checks.update(check_history(native,False));checks.update(check_gpu_memory(native,gpu,False))
    report=dict(scope=__doc__,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()})
    (a.probe/'unused-world-checks.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    for k,v in report['checks'].items():print(v,k)
    print('RESULT',sum(checks.values()),'PASS',len(checks)-sum(checks.values()),'FAIL')
    return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
