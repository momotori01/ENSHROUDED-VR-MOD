"""Check fail-closed world rejection after normal managed game shutdown.

This checks fallback, not world stereo. A clean process exit alone does not
prove hooks were removed at the intended point; both are checked separately.
"""
import argparse
import collections
import json
from pathlib import Path


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe', type=Path)
    a = p.parse_args()
    launch = a.probe.parent
    native = rows(a.probe/'view-resource-probe.jsonl')
    xr = rows(launch/'native-xr/openxr.jsonl')
    mock = rows(launch/'mock.jsonl')
    gpu = rows(launch/'vulkan.jsonl')
    def events(data, name):
        return [e for e in data if e['event'] == name]
    complete = events(native, 'view_resource_hook_complete')
    failure = events(native, 'native_view_probe_failure')
    closed = events(xr, 'native_xr_closed')
    removed = events(native, 'shared_resolution_hook_removed')
    scene = events(native, 'calibration_scene_hook_removed')
    histogram = collections.Counter(e['event'] for e in mock)
    ended = events(mock, 'end_frame')
    post = json.loads((a.probe/'probe-postcheck.json').read_text())
    audit = json.loads((a.probe/'loader-audit.json').read_text())
    call = [c for c in audit['calls'] if c['phase'] == 'bounded_diagnostic']
    device = [e for e in events(gpu, 'create_device') if e.get('openxr_extensions_prepared')]
    game_exit = False
    if len(device) == 1:
        dev = device[0]
        destroyed = [e for e in events(gpu, 'destroy_device') if e['device'] == dev['device'] and e['us'] > dev['us']]
        instances = [e for e in events(gpu, 'destroy_instance') if e['instance'] == dev['instance'] and e['us'] > dev['us']]
        game_exit = len(destroyed) == len(instances) == 1 and destroyed[0]['us'] < instances[0]['us']
    checks = {
        'intended_world_guard': len(failure) == 1 and failure[0]['reason'] == 'world feature ownership unresolved; allocation refused',
        'guard_result_is_expected_failure': len(call) == 1 and call[0]['result'] == 1 and not call[0]['pending'],
        'no_private_view_created': not events(native, 'native_view_created'),
        'no_eye_rendered_or_submitted': not events(native, 'calibration_eye_recorded') and not events(xr, 'native_game_pair_submitted'),
        'one_frame_aborted_without_projection': len(ended) == 1 and ended[0]['frame'] == 1 and ended[0]['layers'] == 0,
        'both_acquired_images_released': histogram['acquire'] == histogram['wait_image'] == histogram['release'] == 2,
        'session_closed_once': len(closed) == histogram['session_destroyed'] == 1,
        'borrowed_instance_destroyed_after_session': histogram['instance_destroyed'] == 1 and bool(closed) and events(mock, 'instance_destroyed')[0]['us'] > closed[0]['us'],
        'render_hook_restored_before_return': len(complete) == 1 and complete[0]['result'] == 1 and complete[0]['original_bytes_restored'],
        'projection_hook_restored': len(events(native, 'recorded_xr_projection_hook_removed')) == 1 and events(native, 'recorded_xr_projection_hook_removed')[0]['status'] == 0,
        'all_eight_shared_hooks_restored': len(removed) == 8 and {e['function'] for e in removed} == {'0x1172c0','0x28620','0x1cc620','0x117970','0x2aa30','0x1cdd20','0x152f00','0x153fe0'} and all(e['status'] == 0 for e in removed),
        'unused_scene_hook_removed_before_return': len(scene) == len(complete) == 1 and scene[0]['status'] == 0 and scene[0]['us'] < complete[0]['us'],
        'xr_closed_before_probe_return': len(closed) == len(complete) == 1 and closed[0]['us'] < complete[0]['us'],
        'game_survived_with_frames_after_guard': post['expected_process_alive'] and post['rendering_continued_four_seconds'] and not post['new_crash_directories'] and post['status'] == 'PASS',
        'normal_game_device_and_instance_exit': game_exit,
        'no_forced_native_retirement': not events(native, 'calibration_gpu_retired'),
    }
    report = dict(checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, limitation=__doc__)
    (a.probe/'world-guard-checks.json').write_text(json.dumps(report, indent=2)+'\n', encoding='utf8')
    for k, v in report['checks'].items():
        print(v, k)
    print('PASS', sum(checks.values()), 'FAIL', len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__ == '__main__':
    raise SystemExit(main())
