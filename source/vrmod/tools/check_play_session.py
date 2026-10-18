"""Summarise and check a play-session run (runner --world-session, 2026-10-06).

Reads the live probe log (session_policy / session_heartbeat / session_stop_requested / teardown events),
the native XR log (eye luma samples if enabled) and optional nvidia-smi samples
(<launch>/claude-gpu-samples.csv: timestamp, memory.used, utilization, temperature).

Checks (each PASS/FAIL, all printed):
  - policy present, at least one heartbeat, no heartbeat reports a failed pair
  - the session ended through a stop request (stop file or minute limit) and the render hook completed with result 0
  - pairs/s of the last third of heartbeats is at least 85 % of the first third (no progressive slowdown)
  - no black eye luma sample (if luma was enabled)
  - GPU memory growth between the first and last third of samples is below 256 MiB (if samples exist)
  - live controller input (runner --world-live-input, 2026-10-09), when enabled: the synthetic pad connected,
    no controller failure was logged, and the XInput table was restored at the end
`--self-test` runs synthetic positive/negative cases. A PASS is a stability summary of one run,
not proof of unlimited play or of real-HMD behaviour.
"""
import argparse
import json
import statistics
import sys
import tempfile
from datetime import datetime, timedelta
from pathlib import Path


def rows(path):
    out = []
    if not path.exists():
        return out
    for line in path.read_text(encoding='utf-8-sig', errors='replace').splitlines():
        try:
            out.append(json.loads(line))
        except ValueError:
            pass
    return out


def thirds(values):
    if len(values) < 3:
        return None, None
    k = max(1, len(values) // 3)
    return statistics.median(values[:k]), statistics.median(values[-k:])


def summarize(launch):
    probes = []
    for d in sorted(launch.glob('view-resource-probe-*')):
        log = d / 'view-resource-probe.jsonl'
        if log.exists() and '"session_policy"' in log.read_text(encoding='utf-8', errors='replace'):
            probes.append(d)
    if not probes:
        return dict(status='FAIL', checks={'policy': 'FAIL'}, reason='no session probe directory with session_policy')
    probe = probes[-1]
    ev = rows(probe / 'view-resource-probe.jsonl')
    policy = [e for e in ev if e.get('event') == 'session_policy']
    beats = [e for e in ev if e.get('event') == 'session_heartbeat']
    stops = [e for e in ev if e.get('event') == 'session_stop_requested']
    complete = [e for e in ev if e.get('event') == 'view_resource_hook_complete']
    retained = [e for e in ev if e.get('event') == 'calibration_retained_until_scene_disposal']
    luma = [e for e in rows(launch / 'native-xr/openxr.jsonl') if e.get('event') == 'native_eye_luma']
    black = sum(1 for e in luma if min(e['luma']) < 5)
    rates = [b['pairs_per_s'] for b in beats[1:]]  # the first window includes warm-up
    first, last = thirds(rates)
    # GPU samples start at launch; only samples from 60 s after the live probe started (directory name time)
    # describe the session itself (eye resources are created during that first minute).
    gpu = []
    csv = launch / 'claude-gpu-samples.csv'
    try:
        session_start = datetime.strptime(probe.name[len('view-resource-probe-'):], '%Y%m%d-%H%M%S') + timedelta(seconds=60)
    except ValueError:
        session_start = None
    if csv.exists():
        for line in csv.read_text(encoding='utf-8-sig', errors='replace').splitlines():
            parts = [x.strip() for x in line.split(',')]
            if len(parts) < 2 or not parts[1].endswith('MiB'):
                continue
            try:
                stamp = datetime.strptime(parts[0].split('.')[0], '%Y/%m/%d %H:%M:%S')
            except ValueError:
                stamp = None
            if session_start and stamp and stamp < session_start:
                continue
            gpu.append(int(parts[1].split()[0]))
    g_first, g_last = thirds(gpu)
    checks = {
        'policy': bool(policy),
        'heartbeats': len(beats) >= 1,
        'no_failed_heartbeat': not any(b.get('failed') for b in beats),
        'stop_requested': bool(stops),
        'hook_completed_ok': bool(complete) and complete[-1].get('result') == 0,
        'no_progressive_slowdown': first is None or last >= 0.85 * first,
        'no_black_luma': black == 0,
        'gpu_memory_stable': g_first is None or (g_last - g_first) < 256,
    }
    live = [e for e in ev if e.get('event') == 'controller_live_enabled']
    live_samples = [e for e in ev if e.get('event') == 'controller_live_sample']
    if live:
        restored = [e for e in ev if e.get('event') == 'controller_fixture_restored']
        checks['live_input_connected'] = any(e.get('event') == 'controller_fixture_connected' for e in ev)
        checks['live_input_no_failure'] = not any(e.get('event') == 'controller_fixture_failure' for e in ev)
        checks['live_input_restored'] = bool(restored) and restored[-1].get('pointers_restored') is True
    return dict(probe=probe.name, minutes=policy[0].get('minutes') if policy else None,
                heartbeats=len(beats), total_pairs=retained[-1].get('pairs') if retained else (beats[-1]['pairs'] if beats else 0),
                elapsed_s=beats[-1]['elapsed_s'] if beats else 0, stop_reason=stops[-1].get('reason') if stops else None,
                pairs_per_s_first_third=first, pairs_per_s_last_third=last,
                pairs_per_s_min=min(rates) if rates else None, pairs_per_s_max=max(rates) if rates else None,
                luma_samples=len(luma), black_samples=black,
                gpu_mib_first_third=g_first, gpu_mib_last_third=g_last, gpu_samples=len(gpu),
                suppressed_total=beats[-1].get('suppressed_total') if beats else None,
                live_input=dict(samples_logged=len(live_samples), pressed_samples=sum(1 for e in live_samples if e.get('buttons')),
                                stale_samples=sum(1 for e in live_samples if not e.get('fresh')),
                                failure=next((e.get('reason') for e in ev if e.get('event') == 'controller_fixture_failure'), None)) if live else None,
                checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()},
                status='PASS' if all(checks.values()) else 'FAIL',
                limit='One run of a stability summary; not proof of unlimited play or of real-HMD behaviour.')


def self_test():
    def make(root, beats, stop=True, result=0, luma=(), gpu=(), live=None):
        d = root / 'view-resource-probe-20261006-000000'
        d.mkdir(parents=True)
        (root / 'native-xr').mkdir()
        ev = [dict(event='session_policy', minutes=5)]
        ev += [dict(event='session_heartbeat', elapsed_s=30 * (i + 1), pairs=1400 * (i + 1), pairs_per_s=r, failed=f) for i, (r, f) in enumerate(beats)]
        if stop:
            ev.append(dict(event='session_stop_requested', reason='stop_file'))
        ev.append(dict(event='calibration_retained_until_scene_disposal', pairs=1400 * len(beats)))
        if live is not None:
            ev.append(dict(event='controller_live_enabled', phase=1))
            ev.append(dict(event='controller_fixture_connected', synthetic=True))
            ev.append(dict(event='controller_live_sample', fresh=True, buttons=4096))
            if live == 'failure':
                ev.append(dict(event='controller_fixture_failure', reason='synthetic'))
            ev.append(dict(event='controller_fixture_restored', pointers_restored=live != 'unrestored'))
        ev.append(dict(event='view_resource_hook_complete', result=result))
        (d / 'view-resource-probe.jsonl').write_text('\n'.join(json.dumps(e) for e in ev), encoding='utf-8')
        (root / 'native-xr/openxr.jsonl').write_text('\n'.join(json.dumps(dict(event='native_eye_luma', luma=list(l))) for l in luma), encoding='utf-8')
        if gpu:
            (root / 'claude-gpu-samples.csv').write_text('\n'.join(f'2026/10/06 00:{2 + i // 60:02d}:{i % 60:02d}, {m} MiB, 90 %, 60' for i, m in enumerate(gpu)), encoding='utf-8')
        return root
    good = [(46.0, False)] * 7
    cases = {
        'healthy': (dict(beats=good, luma=[(60, 58)] * 5, gpu=[9000] * 9), 'PASS', None),
        'failed_pair': (dict(beats=good[:3] + [(46.0, True)] + good[3:]), 'FAIL', 'no_failed_heartbeat'),
        'no_stop': (dict(beats=good, stop=False), 'FAIL', 'stop_requested'),
        'hook_error': (dict(beats=good, result=4), 'FAIL', 'hook_completed_ok'),
        'slowdown': (dict(beats=[(46.0, False)] * 4 + [(30.0, False)] * 3), 'FAIL', 'no_progressive_slowdown'),
        'black': (dict(beats=good, luma=[(60, 0.4)]), 'FAIL', 'no_black_luma'),
        'vram_growth': (dict(beats=good, gpu=[9000] * 3 + [9200] * 3 + [9600] * 3), 'FAIL', 'gpu_memory_stable'),
        'live_input': (dict(beats=good, live='ok'), 'PASS', None),
        'live_failure': (dict(beats=good, live='failure'), 'FAIL', 'live_input_no_failure'),
        'live_unrestored': (dict(beats=good, live='unrestored'), 'FAIL', 'live_input_restored'),
    }
    ok = True
    with tempfile.TemporaryDirectory() as tmp:
        for name, (kw, expected, check) in cases.items():
            summary = summarize(make(Path(tmp) / name, **kw))
            passed = summary['status'] == expected and (check is None or summary['checks'][check] == 'FAIL')
            print('PASS' if passed else 'FAIL', name, summary['status'], check or '')
            ok &= passed
    print('self-test', 'PASS' if ok else 'FAIL', len(cases), 'cases')
    return 0 if ok else 1


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('launch', type=Path, nargs='?', help='game-* launch directory')
    p.add_argument('--report', type=Path, help='write the JSON summary here')
    p.add_argument('--self-test', action='store_true', help='run synthetic positive/negative cases and exit')
    a = p.parse_args()
    if a.self_test:
        return self_test()
    if not a.launch:
        p.error('launch directory required')
    summary = summarize(a.launch)
    text = json.dumps(summary, indent=2, ensure_ascii=False)
    print(text)
    if a.report:
        a.report.write_text(text + '\n', encoding='utf-8')
    return 0 if summary['status'] == 'PASS' else 1


if __name__ == '__main__':
    sys.exit(main())
