"""Replay captured Vulkan feature values/queues in the independent XR GPU fixture.
Process-scoped runtime only. No game process access and no system setting changes.
"""
import argparse
import copy
import hashlib
import json
import math
import os
import pathlib
import re
import subprocess
from check_vulkan_binding import select_creation, names


def features(device):
    if device.get('feature_metadata_complete') is not True or device.get('feature_chain_truncated') or device.get('feature_chain_cycle'):
        raise ValueError('complete, acyclic feature metadata required')
    result = []
    if device.get('core_features') is not None:
        result.append({'name': 'VkPhysicalDeviceFeatures2', 'values': device['core_features']})
    chain = device.get('feature_chain')
    if not isinstance(chain, list) or len(chain) > 64:
        raise ValueError('invalid feature chain')
    for node in chain:
        if node.get('name') == 'loader_device_create_info' and node.get('scope') == 'loader_private_not_features':
            continue
        result.append({'name': node.get('name'), 'values': node.get('values')})
    seen = set()
    for node in result:
        name, values = node['name'], node['values']
        if not isinstance(name, str) or not re.fullmatch(r'VkPhysicalDevice[A-Za-z0-9]+', name) or name in seen:
            raise ValueError('unknown/duplicate feature structure')
        seen.add(name)
        if not isinstance(values, dict) or not values or len(values) > 256:
            raise ValueError('feature values absent')
        for field, value in values.items():
            if not re.fullmatch(r'[A-Za-z][A-Za-z0-9_]*', field) or type(value) is not int or value not in (0, 1):
                raise ValueError('invalid feature field/value')
    return result


def queues(device):
    result = device.get('queues')
    if not isinstance(result, list) or not 0 < len(result) <= 64:
        raise ValueError('queue metadata absent')
    seen = set()
    for q in result:
        family, count, flags, priorities = (q.get(k) for k in ('family', 'count', 'flags', 'priorities'))
        if type(family) is not int or not 0 <= family < 16384 or family in seen or type(count) is not int or not 0 < count <= 64:
            raise ValueError('invalid queue family/count')
        seen.add(family)
        if type(flags) is not int or flags != 0:
            raise ValueError('nonzero recorded queue flags not supported')
        if not isinstance(priorities, list) or len(priorities) != count or any(type(p) not in (int, float) or not math.isfinite(p) or not 0 <= p <= 1 for p in priorities):
            raise ValueError('invalid queue priorities')
    return result


def arguments(events):
    if sum(e.get('event') == 'create_device' for e in events) != 1:
        raise ValueError('trace requires exactly one device creation')
    instance, device, _, _ = select_creation(events)
    api = instance.get('api') or (1 << 22)
    if type(api) is not int or not 0 < api < (1 << 29):
        raise ValueError('invalid Vulkan API')
    args = ['--host-api', str(api)]
    for kind, record in [('instance', instance), ('device', device)]:
        for name in names(record['extensions']):
            args += ['--host-' + kind + '-extension', name]
    for node in features(device):
        for field, value in node['values'].items():
            args += ['--host-feature', f'{node["name"]}.{field}={value}']
    for queue in queues(device):
        args += ['--host-queue', ':'.join(map(str, [queue['family'], queue['flags'], *queue['priorities']]))]
    properties = [e for e in events if e.get('event') == 'physical_device_properties' and e.get('device') == device.get('device')]
    if len(properties) != 1:
        raise ValueError('matching GPU properties required')
    for key in ('vendor_id', 'device_id'):
        value = properties[0].get(key)
        if type(value) is not int or not 0 < value <= 0xffffffff:
            raise ValueError('invalid GPU model identity')
        args += ['--host-' + key.replace('_', '-'), str(value)]
    return args


def verify(events, actual_path):
    _, expected, _, _ = select_creation(events)
    actual = [json.loads(line) for line in actual_path.read_text().splitlines()]
    _, created, _, _ = select_creation(actual)
    checks = {'feature_values_and_order': features(expected) == features(created),
              'queue_families_counts_flags_priorities': queues(expected) == queues(created)}
    properties = next(e for e in events if e.get('event') == 'physical_device_properties' and e.get('device') == expected.get('device'))
    checks['GPU_model'] = all(properties[k] == created[k] for k in ('vendor_id', 'device_id'))
    report = {'checks': checks, 'native_stereo_verified': False,
              'scope': 'independent device: recorded API, extension names plus runtime requirements, feature values and queue parameters; excludes game resources, queue pNext, instance/device creation flags and allocator callbacks'}
    (actual_path.parent / 'device-replay-results.json').write_text(json.dumps(report, indent=2), encoding='utf8')
    for name, passed in checks.items():
        print('PASS' if passed else 'FAIL', name)
    if not all(checks.values()):
        raise ValueError('recorded device configuration was not preserved')


def self_test():
    sample = {'feature_metadata_complete': True, 'core_features': None,
              'feature_chain': [{'name': 'VkPhysicalDeviceVulkan12Features', 'values': {'timelineSemaphore': 1}}],
              'queues': [{'family': 0, 'count': 1, 'flags': 0, 'priorities': [1]}]}
    assert features(sample)[0]['values']['timelineSemaphore'] == 1
    assert queues(sample)[0]['priorities'] == [1]
    underscore = copy.deepcopy(sample)
    underscore['feature_chain'][0]['values']['textureCompressionASTC_LDR'] = 0
    assert features(underscore)[0]['values']['textureCompressionASTC_LDR'] == 0
    mutations = [lambda d: d.update(feature_metadata_complete=False),
                 lambda d: d['feature_chain'][0].update(name=None),
                 lambda d: d['feature_chain'][0]['values'].update(timelineSemaphore=2),
                 lambda d: d['queues'][0].update(priorities=[]),
                 lambda d: d['queues'][0].update(flags=1),
                 lambda d: d['queues'].append(copy.deepcopy(d['queues'][0]))]
    for mutation in mutations:
        invalid = copy.deepcopy(sample)
        mutation(invalid)
        try:
            features(invalid); queues(invalid)
        except ValueError:
            continue
        raise AssertionError('invalid capture accepted')
    print('PASS 9 recorded feature/queue input checks')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--trace', type=pathlib.Path)
    p.add_argument('--runtime', type=pathlib.Path)
    p.add_argument('--output', type=pathlib.Path)
    p.add_argument('--self-test', action='store_true')
    a = p.parse_args()
    if a.self_test:
        self_test(); return
    if not all((a.trace, a.runtime, a.output)):
        p.error('--trace, --runtime and --output required')
    events = [json.loads(line) for line in a.trace.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
    args = arguments(events)
    out = a.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.update(XR_RUNTIME_JSON=str(a.runtime.resolve(strict=True)), EVR_MOCK_LOG=str(out / 'mock.jsonl'), EVR_POSE_SEQUENCE='')
    binary = pathlib.Path(__file__).resolve().parents[1] / 'build/bin/Release/evr_borrowed_vulkan_tests.exe'
    (out / 'creation-input.json').write_text(json.dumps({'trace': str(a.trace.resolve()), 'sha256': hashlib.sha256(a.trace.read_bytes()).hexdigest(), 'arguments': args}, indent=2), encoding='utf8')
    result = subprocess.run([str(binary), str(out), *args], env=env, capture_output=True, text=True, timeout=90)
    (out / 'probe.txt').write_text(result.stdout + '\n' + result.stderr, encoding='utf8')
    print(result.stdout, result.stderr, end='')
    result.check_returncode()
    verify(events, out / 'host-vulkan.jsonl')


if __name__ == '__main__':
    main()
