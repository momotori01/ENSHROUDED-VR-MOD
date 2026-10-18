"""Offline comparison of recorded Vulkan creation and queried XR requirements.

This checks explicit extension enablement, not GPU support, features, queue
synchronization, handle lifetime, or rendered stereo. No process/registry access.
Legacy traces without handles require an explicit instance selection.
"""
import argparse
import copy
import hashlib
import json
import pathlib
import re
import sys


def names(value):
    if not isinstance(value, list) or any(not isinstance(n, str) or not re.fullmatch(r'VK_[A-Za-z0-9_]+', n) for n in value):
        raise ValueError('invalid Vulkan extension name list')
    return list(dict.fromkeys(value))


def select_creation(events, device_index=0, instance_index=None):
    instances = [e for e in events if e.get('event') == 'create_instance']
    devices = [e for e in events if e.get('event') == 'create_device']
    if not 0 <= device_index < len(devices):
        raise ValueError('selected device creation is absent')
    device = devices[device_index]
    # Find the matching live instance at device creation, even after handle reuse.
    active = {}
    for e in events:
        if e is device:
            break
        if e.get('event') == 'create_instance' and 'instance' in e:
            active[e['instance']] = e
        elif e.get('event') == 'destroy_instance' and 'instance' in e:
            active.pop(e['instance'], None)
    linked = active.get(device.get('instance'))
    if instance_index is not None:
        if not 0 <= instance_index < len(instances):
            raise ValueError('selected instance creation is absent')
        instance = instances[instance_index]
        if 'instance' in device and instance is not linked:
            raise ValueError('explicit selection contradicts recorded handle lifetime')
        if next(i for i,e in enumerate(events) if e is instance) >= next(i for i,e in enumerate(events) if e is device):
            raise ValueError('instance creation follows device creation')
        selection = 'explicit_legacy_selection' if 'instance' not in device else 'recorded_handles'
    else:
        if linked is None:
            raise ValueError('no unambiguous instance association; legacy traces need --instance-index')
        instance = linked
        instance_index = next(i for i,e in enumerate(instances) if e is instance)
        selection = 'recorded_handles'
    return instance, device, instance_index, selection


def compare(events, requirements, device_index=0, instance_index=None):
    instance, device, instance_index, selection = select_creation(events, device_index, instance_index)
    raw = instance['api'] or (1 << 22)  # Vulkan defines apiVersion=0 as 1.0
    if type(raw) is not int or not 0 < raw < (1 << 32) or raw >> 29:
        raise ValueError('invalid/nonstandard Vulkan API variant')
    api = ((raw >> 22) & 127, (raw >> 12) & 1023, raw & 4095)
    xr_api = (api[0] << 48) | (api[1] << 32) | api[2]
    minimum, maximum = (requirements[k] for k in ('min_vulkan_version', 'max_vulkan_version'))
    if type(minimum) is not int or type(maximum) is not int or not 0 < minimum <= maximum < (1 << 64):
        raise ValueError('invalid runtime API range')
    report = dict(runtime_name=requirements['runtime_name'], runtime_version=requirements['runtime_version'],
                  device_index=device_index, instance_index=instance_index, association=selection,
                  game_api='.'.join(map(str, api)), api_in_runtime_range=minimum <= xr_api <= maximum,
                  min_vulkan_version=minimum, max_vulkan_version=maximum,
                  native_stereo_verified=False, binding_ready=False,
                  remaining_checks=['runtime-selected GPU identity', 'required Vulkan features and extension dependencies',
                                    'created graphics queue and external synchronization', 'handle lifetime',
                                    'per-eye game rendering integration'])
    for kind, creation in [('instance', instance), ('device', device)]:
        enabled = names(creation['extensions'])
        required = names(requirements[kind + '_extensions'])
        missing = [n for n in required if n not in enabled]
        report[kind] = dict(enabled=enabled, required=required, missing=missing,
                            proposed_enablement=enabled + missing)
    report['creation_requirements_pass'] = report['api_in_runtime_range'] and not any(report[k]['missing'] for k in ('instance', 'device'))
    report['result'] = 'PASS' if report['creation_requirements_pass'] else 'FAIL'
    return report


def self_test():
    req = dict(runtime_name='fixture', runtime_version=1, min_vulkan_version=1 << 48,
               max_vulkan_version=(1 << 48) | (2 << 32), instance_extensions=['VK_A'], device_extensions=['VK_B'])
    original = [dict(event='create_instance', instance='1', api=4202496, extensions=['VK_A']),
                dict(event='create_device', instance='1', extensions=['VK_B', 'VK_Game'])]
    count = 0
    def check(label, value):
        nonlocal count
        if not value:
            raise AssertionError(label)
        print('PASS', label)
        count += 1
    check('explicit requirements satisfied does not imply binding ready', compare(original, req)['creation_requirements_pass'] and not compare(original, req)['binding_ready'])
    missing = copy.deepcopy(original)
    missing[1]['extensions'] = ['VK_Game']
    result = compare(missing, req)
    check('missing extension rejected and game extension preserved', result['result'] == 'FAIL' and result['device']['proposed_enablement'] == ['VK_Game', 'VK_B'])
    higher = copy.deepcopy(original)
    higher[0]['api'] = (1 << 22) | (3 << 12)
    check('API above runtime range rejected', compare(higher, req)['result'] == 'FAIL')
    legacy = copy.deepcopy(original)
    for e in legacy:
        e.pop('instance')
    cases = [('legacy auto-association refused', legacy, req, {}),
             ('destroyed instance refused', [original[0], dict(event='destroy_instance', instance='1'), original[1]], req, {}),
             ('contradictory explicit instance refused', [original[0], dict(original[0], instance='2'), original[1]], req, dict(instance_index=1)),
             ('invalid extension names refused', original, dict(req, device_extensions=['VK_B bad']), {}),
             ('absent device refused', [], req, {}),
             ('invalid API range refused', original, dict(req, max_vulkan_version=0), {})]
    for label, events, requirements, kwargs in cases:
        try:
            compare(events, requirements, **kwargs)
        except ValueError:
            check(label, True)
        else:
            check(label, False)
    check('explicit legacy association marked', compare(legacy, req, instance_index=0)['association'] == 'explicit_legacy_selection')
    reused = [original[0], dict(event='destroy_instance', instance='1'), dict(original[0], extensions=[]), original[1]]
    check('reused handle resolves new creation', compare(reused, req)['instance_index'] == 1 and compare(reused, req)['result'] == 'FAIL')
    check('inputs unmodified', original[1]['extensions'] == ['VK_B', 'VK_Game'])
    print(f'{count} PASS / 0 FAIL')


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--trace', type=pathlib.Path)
    p.add_argument('--requirements', type=pathlib.Path)
    p.add_argument('--output', type=pathlib.Path)
    p.add_argument('--device-index', type=int, default=0)
    p.add_argument('--instance-index', type=int)
    p.add_argument('--self-test', action='store_true')
    a = p.parse_args()
    if a.self_test:
        self_test()
        return 0
    if not all((a.trace, a.requirements, a.output)):
        p.error('--trace, --requirements and --output required')
    result = compare([json.loads(line) for line in a.trace.read_text(encoding='utf-8-sig').splitlines() if line.strip()],
                     json.loads(a.requirements.read_text(encoding='utf-8-sig')), a.device_index, a.instance_index)
    result['inputs'] = {name: dict(path=str(path.resolve()), sha256=hashlib.sha256(path.read_bytes()).hexdigest())
                        for name, path in [('trace', a.trace), ('requirements', a.requirements)]}
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(result, indent=2) + '\n', encoding='utf8')
    print(result['result'], 'recorded Vulkan creation requirements;', 'API', result['game_api'])
    for kind in ('instance', 'device'):
        print(kind, 'missing:', ', '.join(result[kind]['missing']) or 'none')
    print('Binding and native stereo remain unverified. Report:', a.output)
    return 0 if result['creation_requirements_pass'] else 2


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, KeyError, OSError) as exc:
        print('FAIL', exc, file=sys.stderr)
        sys.exit(1)
