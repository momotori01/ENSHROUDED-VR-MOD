"""Compare actual game Vulkan creation before/after optional OpenXR preparation."""
import argparse
import hashlib
import json
import pathlib
from check_vulkan_binding import select_creation
from host_device_replay import features, queues

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--original', required=True, type=pathlib.Path)
parser.add_argument('--prepared', required=True, type=pathlib.Path)
parser.add_argument('--requirements', required=True, type=pathlib.Path)
args = parser.parse_args()
def read(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
before, after = read(args.original), read(args.prepared)
old_instance, old_device, _, _ = select_creation(before)
new_instance, new_device, _, _ = select_creation(after)
required = json.loads(args.requirements.read_text())
checks = {}
for kind, old, new in [('instance', old_instance, new_instance), ('device', old_device, new_device)]:
    expected = list(dict.fromkeys(old['extensions'] + required[kind + '_extensions']))
    checks[kind + '_extension_union'] = new['extensions'] == expected
    checks[kind + '_preparation_enabled'] = new.get('openxr_extensions_prepared') is True
checks['original_API_preserved'] = old_instance['api'] == new_instance['api']
checks['original_feature_values_and_order_preserved'] = features(old_device) == features(new_device)
checks['original_queues_preserved'] = queues(old_device) == queues(new_device)
checks['same_queried_runtime'] = any(e.get('event') == 'preflight_runtime' and e.get('name') == required['runtime_name'] and e.get('version') == required['runtime_version'] for e in after)
checks['no_preparation_fallback'] = not any(e.get('event') == 'preflight_fallback' for e in after)
frames = [e for e in after if e.get('event') == 'game_frame']
checks['game_present_progress'] = len(frames) >= 2 and frames[-1]['frame'] > frames[0]['frame'] and all(e['result'] == 0 for e in frames)
report = {'scope': 'actual game graphics creation only; no game stereo or XR session verified',
          'native_stereo_verified': False,
          'inputs': {name: {'path': str(path.resolve()), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()} for name, path in vars(args).items()},
          'checks': checks}
destination = args.prepared.parent / 'preparation-audit.json'
destination.write_text(json.dumps(report, indent=2), encoding='utf8')
for name, passed in checks.items():
    print('PASS' if passed else 'FAIL', name)
raise SystemExit(0 if all(checks.values()) else 1)
