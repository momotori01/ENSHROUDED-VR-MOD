"""Exercise opt-in layer preparation and real Vulkan failure/fallback in child fixtures."""
import json
import argparse
import os
import pathlib
import subprocess
import time

mod = pathlib.Path(__file__).resolve().parents[1]
binary = mod / 'build/bin/Release'
output = mod / 'output' / time.strftime('graphics-preflight-%Y%m%d-%H%M%S')
output.mkdir(parents=True, exist_ok=True)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--steamvr', action='store_true')
parser.add_argument('--renderdoc', action='store_true',help='Exercise the public pre-instance ABI and wrapped handles with EVR above portable RenderDoc')
options = parser.parse_args()
isolated_env = {}
runtime_path = binary / 'evr_mock_runtime.json'
if options.steamvr:
    isolated = mod / 'test_env/steamvr'
    if not (isolated / 'active-session.json').is_file():
        parser.error('owned isolated SteamVR session required')
    registration = json.loads((isolated / 'openvrpaths.vrpath').read_text(encoding='utf-8-sig'))
    runtime_path = pathlib.Path(registration['runtime'][0]) / 'steamxr_win64.json'
    isolated_env = dict(VR_PATHREG_OVERRIDE=str(isolated / 'openvrpaths.vrpath'),
                        VR_CONFIG_PATH=str(isolated / 'config'), VR_LOG_PATH=str(isolated / 'logs'))
checks = []
for name, enable, fail, missing in [('off', False, False, False), ('prepared', True, False, False),
                                    ('device_fallback', True, True, False), ('runtime_fallback', True, False, True)]:
    directory = output / name
    directory.mkdir()
    env = os.environ.copy()
    env.update(isolated_env)
    env.update(VK_LAYER_PATH=str(binary), VK_INSTANCE_LAYERS='VK_LAYER_EVR_diagnostic',
               EVR_LAYER_OUTPUT=str(directory), EVR_PREPARE_OPENXR='1' if enable else '0',
               EVR_PREPARE_FIXTURE='1', EVR_PREPARE_FAIL_DEVICE='1' if fail else '0',
               EVR_PREPARE_LOADER=str(binary / 'openxr_loader.dll'),
               XR_RUNTIME_JSON=str(directory / 'absent-runtime.json' if missing else runtime_path),
               EVR_MOCK_LOG=str(directory / 'mock.jsonl'), EVR_POSE_SEQUENCE='')
    env['EVR_QUEUE_TRACE_BUDGET'] = '16'
    env['EVR_PREPARE_SESSION_FIXTURE'] = '1'
    env['EVR_PREPARE_SESSION_EXPECTED'] = '0' if name == 'prepared' else '2'
    if options.renderdoc:
        rd=mod.parent/'references/renderdoc/1.46/RenderDoc_1.46_64'
        if not (rd/'renderdoc.json').is_file():raise RuntimeError('portable RenderDoc missing')
        env.update(VK_LAYER_PATH=str(binary)+';'+str(rd),VK_INSTANCE_LAYERS='VK_LAYER_EVR_diagnostic;VK_LAYER_RENDERDOC_Capture',ENABLE_VULKAN_RENDERDOC_CAPTURE='1')
        env.pop('DISABLE_VULKAN_RENDERDOC_CAPTURE_1_46',None)
    result = subprocess.run([str(binary / 'evr_graphics_preflight_tests.exe')], env=env,
                            capture_output=True, text=True, timeout=30,creationflags=subprocess.CREATE_NO_WINDOW)
    (directory / 'process.txt').write_text(result.stdout + '\n' + result.stderr, encoding='utf8')
    events = [json.loads(line) for line in (directory / 'vulkan.jsonl').read_text().splitlines()]
    devices = [e for e in events if e.get('event') == 'create_device']
    fallback = [e for e in events if e.get('event') == 'preflight_fallback']
    passed = result.returncode == 0 and len(devices) == 1
    passed &= sum(e.get('event') == 'destroy_device' for e in events) == 1
    passed &= sum(e.get('event') == 'destroy_instance' for e in events) == 1
    if passed:
        passed &= devices[0]['openxr_extensions_prepared'] == (enable and not fail and not missing)
        if name == 'device_fallback':
            passed &= any(e.get('phase') == 'device_create' and e.get('result') == -8 for e in fallback)
        elif name == 'runtime_fallback':
            passed &= any(e.get('phase') == 'instance_plan' for e in fallback)
        else:
            passed &= not fallback
        passed &= devices[0]['queues'][0]['priorities'] == [0.75]
        passed &= devices[0]['feature_chain'][-1]['values']['timelineSemaphore'] == 1
        submissions = [e for e in events if e.get('event') == 'queue_submit' and e.get('call_context') != 'openxr_fixture']
        passed &= len(submissions) == 2 and all(e['result'] == 0 and not e['truncated'] for e in submissions)
        if len(submissions) == 2:
            signals = submissions[0]['batches'][0]['signals']
            waits = submissions[1]['batches'][0]['waits']
            passed &= [s['type'] for s in signals] == ['timeline', 'binary']
            passed &= [(s['semaphore'], s['value'], s['type']) for s in signals] == [(s['semaphore'], s['value'], s['type']) for s in waits]
            passed &= [s['value'] for s in waits] == [5, 0]
        passed &= any(e.get('event') == 'queue_wait_idle' and e['result'] == 0 for e in events)
        passed &= sum(e.get('event') == 'prepared_session_fixture' for e in events) == int(name == 'prepared')
        if name == 'prepared' and not options.steamvr:
            xr_events = [json.loads(line) for line in (directory / 'mock.jsonl').read_text().splitlines()]
            for event, count in [('instance_created', 1), ('session_created', 1), ('swapchain_created', 2),
                                 ('wait_frame', 3), ('acquire', 6), ('wait_image', 6), ('release', 6),
                                 ('end_frame', 3), ('session_destroyed', 1), ('instance_destroyed', 1)]:
                passed &= sum(e['event'] == event for e in xr_events) == count
            passed &= all(e['layers'] == 0 for e in xr_events if e['event'] == 'end_frame')
    checks.append({'case': name, 'status': 'PASS' if passed else 'FAIL', 'returncode': result.returncode})
    print(checks[-1]['status'], name, result.stdout.strip(), flush=True)
(output / 'results.json').write_text(json.dumps(checks, indent=2), encoding='utf8')
print('Results:', output)
raise SystemExit(0 if all(row['status'] == 'PASS' for row in checks) else 1)
