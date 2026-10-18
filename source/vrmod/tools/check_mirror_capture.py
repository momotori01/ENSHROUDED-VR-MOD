"""Check the actual game present readback regression, after normal game exit."""
import argparse
import json
import pathlib
import statistics
from check_capture import ppm


def normal_game_teardown(events):
    """Match capture lifetime to the game device, not a startup probe instance.

    Capture records do not identify their device. Until they do, reject logs
    with multiple device creations instead of guessing which one rendered.
    Instance handles can be reused after an earlier startup probe is destroyed.
    """
    devices = [e for e in events if e['event'] == 'create_device']
    captures = [e for e in events if e['event'] == 'game_capture']
    if len(devices) != 1 or not captures:
        return False
    created = devices[0]
    parent_history = [e for e in events
                      if e['event'] in ('create_instance', 'destroy_instance')
                      and e.get('instance') == created['instance']
                      and e['us'] < created['us']]
    if not parent_history or max(parent_history, key=lambda e: e['us'])['event'] != 'create_instance':
        return False
    device_end = [e for e in events if e['event'] == 'destroy_device'
                  and e['device'] == created['device'] and e['us'] > created['us']]
    instance_end = [e for e in events if e['event'] == 'destroy_instance'
                    and e['instance'] == created['instance'] and e['us'] > created['us']]
    if len(device_end) != 1 or len(instance_end) != 1:
        return False
    render_times = [e['us'] for e in events if e['event'] in ('game_capture', 'game_frame')]
    return (created['us'] < min(render_times)
            and max(render_times) < device_end[0]['us'] < instance_end[0]['us'])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('directory', type=pathlib.Path)
    parser.add_argument('--minimum', type=int, default=2)
    args = parser.parse_args()
    events = [json.loads(line) for line in (args.directory/'vulkan.jsonl').read_text().splitlines()]
    checks = []
    def check(name, ok):
        checks.append({'test': name, 'status': 'PASS' if ok else 'FAIL'})
    captures = [e for e in events if e['event'] == 'game_capture']
    check('requested_captures_completed', len(captures) >= args.minimum)
    check('no_capture_errors', not any(e['event'] in ('capture_failed', 'capture_exception') for e in events))
    check('game_device_and_parent_instance_teardown', normal_game_teardown(events))
    check('request_consumed', not (args.directory/'capture.request').exists())
    for capture in captures:
        path = args.directory / pathlib.Path(capture['path']).name
        w, h, pixels = ppm(path)
        sampled = pixels[::17]
        check(path.name+':valid_image', w > 0 and h > 0 and statistics.mean(sampled) > 3 and statistics.pvariance(sampled) > 4)
        check(path.name+':game_continued', any(e['event'] == 'game_frame' and e['us'] > capture['us'] + 2_000_000 and e['result'] == 0 for e in events))
    (args.directory/'mirror-capture-results.json').write_text(json.dumps(checks, indent=2))
    for c in checks:
        print(c['status'], c['test'])
    failed = sum(c['status'] == 'FAIL' for c in checks)
    print('RESULT', len(checks)-failed, 'PASS', failed, 'FAIL')
    return bool(failed)


if __name__ == '__main__':
    raise SystemExit(main())
