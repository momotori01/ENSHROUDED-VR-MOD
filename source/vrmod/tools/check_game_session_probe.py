"""Validate a bounded empty OpenXR session on the actual game device.

Session/ownership/fallback validation only; no game eye pixels were submitted.
"""
import argparse,json
from pathlib import Path
from check_native_series import rows


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
    log=rows(a.probe/'view-resource-probe.jsonl');gpu=rows(a.probe.parent/'vulkan.jsonl');xr=rows(a.probe.parent/'game-session-openxr.jsonl')
    events=lambda entries,name:[e for e in entries if e['event']==name]
    leases=events(log,'game_session_queue_lease');complete=events(log,'game_session_probe_complete')
    hook=events(log,'view_resource_hook_complete');binding=events(xr,'borrowed_vulkan');swaps=events(xr,'swapchain')
    fallback=events(xr,'fallback');states=[e['state'] for e in events(xr,'session_state')]
    outcome=events(gpu,'prepared_game_session_probe')
    checks=dict(native_three_queue_lease=len(leases)==1 and leases[0]['queues']==3 and leases[0]['native_lock_value']==0x80000000,
                three_empty_frames=len(complete)==1 and complete[0]['empty_frames']==3 and complete[0]['game_eye_pixels_submitted'] is False,
                code_restored=len(hook)==1 and hook[0]['result']==0 and hook[0]['original_bytes_restored'],
                game_device_borrowed=len(binding)==1 and binding[0]['owned'] is False,
                preflight_xr_instance_borrowed=any(e['owned'] is False for e in events(xr,'openxr_instance_ownership')),
                runtime_reported=len(events(xr,'runtime'))==1,
                two_runtime_sized_swapchains=len(swaps)==2 and {e['eye'] for e in swaps}=={0,1} and all(
                    e['width']==e['recommended_width'] and e['height']==e['recommended_height'] and e['render_scale']==1 for e in swaps),
                session_ready_and_stopping=2 in states and 6 in states and 8 in states,
                three_explicit_empty_fallbacks=len(fallback)==3,
                layer_reported_success=len(outcome)==1 and outcome[0]['status']=='PASS' and outcome[0]['game_stereo'] is False,
                postcheck_game_rendering=json.loads((a.probe/'probe-postcheck.json').read_text())['status']=='PASS')
    if leases:
        device=leases[0]['device'];creation=[e for e in events(gpu,'create_device') if e['device']==device]
        destroyed=[e for e in events(gpu,'destroy_device') if e['device']==device]
        checks['same_real_prepared_game_device']=len(creation)==1 and creation[0]['openxr_extensions_prepared'] and bool(creation[0]['game_rva_stack'])
        checks['game_device_destroyed_after_probe']=len(destroyed)==1 and len(complete)==1 and destroyed[0]['us']>complete[0]['us']
    if (a.probe.parent/'mock.jsonl').exists():
        mock=rows(a.probe.parent/'mock.jsonl')
        for event,count in [('instance_created',1),('session_created',1),('swapchain_created',2),('wait_frame',3),
                            ('acquire',6),('wait_image',6),('release',6),('end_frame',3),('session_destroyed',1),('instance_destroyed',1)]:
            checks['mock_'+event]=len(events(mock,event))==count
        checks['mock_zero_composition_layers']=all(e['layers']==0 for e in events(mock,'end_frame'))
    result=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},runtime=events(xr,'runtime'),limits=__doc__)
    (a.probe/'game-session-checks.json').write_text(json.dumps(result,indent=2))
    for k,v in result['checks'].items():print(v,k)
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
