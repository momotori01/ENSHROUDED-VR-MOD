"""Verify deliberate Mock disconnect stops native VR while the game survives.

Run only after normal game shutdown. A nonzero diagnostic result is expected;
it is accepted only together with the intended runtime loss and complete cleanup.
"""
import argparse
import collections
import json
from pathlib import Path
from check_live_native_xr import rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe', type=Path)
    parser.add_argument('--expected-pairs', type=int, required=True)
    args = parser.parse_args()
    if args.expected_pairs < 1:
        parser.error('disconnect scenario must first render at least one pair')
    launch = args.probe.parent
    game, xr, mock = (rows(p) for p in (launch/'vulkan.jsonl', launch/'native-xr/openxr.jsonl', launch/'mock.jsonl'))
    native = rows(args.probe/'view-resource-probe.jsonl')

    def events(data, name):
        return [e for e in data if e['event'] == name]

    submitted = events(xr, 'native_game_pair_submitted')
    pair_count = args.expected_pairs
    closed = events(xr, 'native_xr_closed')
    lost = [e for e in events(xr, 'session_state') if e['state'] == 7]
    failure = events(game, 'native_xr_begin_failed')
    devices = [e for e in events(game, 'create_device') if e.get('openxr_extensions_prepared')]
    destroyed = events(game, 'destroy_device')
    instances = events(game, 'destroy_instance')
    disposal = events(native, 'calibration_scene_disposal_complete')
    histogram = collections.Counter(e['event'] for e in mock)
    ended = events(mock, 'end_frame')
    checks = {
        'intended_runtime_loss_observed': len(lost) == 1,
        'expected_pairs_before_disconnect': [e['frame'] for e in submitted] == list(range(1, pair_count+1)),
        'no_pair_submitted_after_loss': bool(lost) and all(e['us'] < lost[0]['us'] for e in submitted),
        'only_runtime_stop_begin_failure': len(failure) == 1 and failure[0]['reason'] == 'native XR session stopped',
        'native_two_eye_calls_per_pair': collections.Counter(e['eye'] for e in events(native, 'calibration_eye_recorded')) == {0: pair_count, 1: pair_count},
        'xr_copy_completed_before_release': all(e['gpu_fence_complete'] and e['eyes'] == 2 for e in submitted),
        'mock_images_balanced': histogram['acquire'] == histogram['wait_image'] == histogram['release'] == 2*pair_count,
        'valid_pairs_have_projection': [e['frame'] for e in ended if e['layers'] == 1] == list(range(1, pair_count+1)),
        'loss_frame_has_no_projection': [e['frame'] for e in ended if e['layers'] == 0] == [pair_count+1] and len(ended) == pair_count+1,
        'xr_session_closed_once': len(closed) == histogram['session_destroyed'] == 1,
        'borrowed_xr_instance_eventually_destroyed': histogram['instance_destroyed'] == 1,
        'expected_failure_restores_original_render_hook': any(e['result'] == 1 and e['original_bytes_restored'] for e in events(native, 'view_resource_hook_complete')),
        'projection_hook_restored': len(events(native, 'recorded_xr_projection_hook_removed')) == 1 and events(native, 'recorded_xr_projection_hook_removed')[0]['status'] == 0,
        'normal_game_continues_after_vr_failure': json.loads((args.probe/'probe-postcheck.json').read_text())['status'] == 'PASS',
        'private_resources_disposed_on_scene_exit': len(disposal) == 1 and disposal[0]['views_destroyed'] == 2 and disposal[0]['owned_groups_cleared'],
        'scene_hook_removed': any(e['status'] == 0 for e in events(native, 'calibration_scene_hook_removed')),
        'no_forced_native_retirement': not events(native, 'calibration_gpu_retired'),
        'game_device_and_parent_instance_closed': len(devices) == len(destroyed) == 1 and destroyed[0]['device'] == devices[0]['device'] and any(e['instance'] == devices[0]['instance'] and e['us'] > destroyed[0]['us'] for e in instances),
        'xr_closed_before_native_scene_disposal': len(closed) == len(disposal) == 1 and closed[0]['us'] < disposal[0]['us'],
    }
    registered = events(game, 'native_eye_image_registered')
    for eye in (0, 1):
        selected = [e for e in registered if e['eye'] == eye]
        removed = [e for e in events(game, 'render_image_destroyed') if selected and e['image'] == selected[0]['image'] and e['generation'] == selected[0]['generation']]
        checks[f'eye{eye}_image_lifetime'] = len(selected) == len(removed) == len(disposal) == len(destroyed) == 1 and disposal[0]['us'] < removed[0]['us'] < destroyed[0]['us']
    report = {'checks': {k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, 'limits': __doc__}
    (args.probe/'live-fallback-checks.json').write_text(json.dumps(report, indent=2) + '\n')
    for k, v in report['checks'].items():
        print(v, k)
    print('PASS', sum(checks.values()), 'FAIL', len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__ == '__main__':
    raise SystemExit(main())
