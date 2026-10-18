"""Long finite live-series stability: pair cadence drift, serial continuity, process memory/handles.

This is a bounded endurance diagnostic (configured pairs, normally 3000). It does not
certify unlimited play, comfort, or hardware frame pacing. The process samples come from an
external read-only Get-Process poll (claude-process-samples.csv) when present.
"""
import argparse, csv, datetime, json, statistics
from pathlib import Path

BUCKET = 300


def rows(path):
    out = []
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            pass
    return out


def quantile(values, q):
    values = sorted(values)
    return values[min(len(values) - 1, int(q * (len(values) - 1) + 0.5))] if values else 0.0


def cadence(pairs):
    """Per-bucket median/p95/max interval in ms between consecutive completed pairs."""
    gaps = [(b['us'] - a['us']) / 1000.0 for a, b in zip(pairs, pairs[1:])]
    buckets = []
    for start in range(0, len(gaps), BUCKET):
        chunk = gaps[start:start + BUCKET]
        if len(chunk) >= BUCKET // 2:
            buckets.append(dict(first_pair=start + 1, gaps=len(chunk), median_ms=statistics.median(chunk), p95_ms=quantile(chunk, .95), max_ms=max(chunk)))
    return gaps, buckets


def samples(path, end_clock):
    """Process samples up to the probe log's last write (HH:MM:SS, same day), excluding teardown/close."""
    if not path.is_file():
        return None
    data = list(csv.DictReader(path.read_text(encoding='utf-8-sig').splitlines()))
    return [dict(time=r['time'], private_mb=float(r['private_mb']), ws_mb=float(r['ws_mb']), handles=int(r['handles']), threads=int(r['threads'])) for r in data if r['time'] <= end_clock]


def check(native, expected, process=None, drift_limit=1.25, memory_limit_mb=256.0, handle_limit=64):
    pairs = [x for x in native if x['event'] == 'live_native_pair_complete']
    c = {}
    c['configured_pairs_completed'] = len(pairs) == expected
    c['frames_and_serials_contiguous'] = len(pairs) > 1 and all(b['frame'] == a['frame'] + 1 and b['serial'] == a['serial'] + 1 for a, b in zip(pairs, pairs[1:]))
    c['no_budget_exhaustion'] = not any(x['event'] == 'view_resource_budget_exhausted' for x in native)
    c['no_native_failures'] = not any('failure' in x['event'] or 'failed' in x['event'] for x in native)
    gaps, buckets = cadence(pairs)
    c['cadence_buckets_measured'] = len(buckets) >= 2
    first, last = (buckets[0], buckets[-1]) if buckets else ({}, {})
    c['no_cadence_drift'] = bool(buckets) and last['median_ms'] <= first['median_ms'] * drift_limit
    c['no_multi_second_stall'] = bool(gaps) and max(gaps) < 2000.0
    stats = dict(pairs=len(pairs), seconds=(pairs[-1]['us'] - pairs[0]['us']) / 1e6 if len(pairs) > 1 else 0, buckets=buckets,
                 overall_median_ms=statistics.median(gaps) if gaps else 0, overall_p95_ms=quantile(gaps, .95), max_ms=max(gaps) if gaps else 0)
    if process:
        live = process
        c['process_samples_present'] = len(live) >= 4
        if len(live) >= 4:
            growth = live[-1]['private_mb'] - min(s['private_mb'] for s in live[:2])
            handles = live[-1]['handles'] - min(s['handles'] for s in live[:2])
            c['private_memory_bounded'] = growth <= memory_limit_mb
            c['handles_bounded'] = handles <= handle_limit
            stats.update(private_growth_mb=growth, handle_growth=handles, private_first_mb=live[0]['private_mb'], private_last_mb=live[-1]['private_mb'], samples=len(live))
    return c, stats


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe', type=Path)
    p.add_argument('--samples', type=Path, help='external process samples CSV (default: <launch>/claude-process-samples.csv)')
    p.add_argument('--negative-tests', action='store_true')
    p.add_argument('--report', type=Path, help='report path (default: <probe>/endurance-stability-checks.json); use for regression on old evidence')
    a = p.parse_args()
    native = rows(a.probe / 'view-resource-probe.jsonl')
    expected = int((a.probe / 'live-xr-config.txt').read_text().split()[1])
    sample_path = a.samples or a.probe.parent / 'claude-process-samples.csv'
    end_clock = datetime.datetime.fromtimestamp((a.probe / 'view-resource-probe.jsonl').stat().st_mtime).strftime('%H:%M:%S')
    process = samples(sample_path, end_clock)
    checks, stats = check(native, expected, process)
    log_bytes = (a.probe / 'view-resource-probe.jsonl').stat().st_size
    stats['log_mb'] = log_bytes / 1e6
    stats['log_kb_per_pair'] = log_bytes / 1e3 / max(1, stats['pairs'])
    if a.negative_tests:
        base = all(checks.values())
        pairs = [x for x in native if x['event'] == 'live_native_pair_complete']
        # Every gap in the final bucket becomes 100 ms longer.
        slow = [dict(x) for x in native]
        tail = {x['serial'] for x in pairs[-BUCKET:]}
        shift = 0
        for y in slow:
            if y['event'] == 'live_native_pair_complete':
                if y['serial'] in tail:
                    shift += 100000
                y['us'] += shift
        checks['reject_cadence_drift'] = base and not check(slow, expected, process)[0]['no_cadence_drift']
        gap = [dict(x) for x in native if not (x['event'] == 'live_native_pair_complete' and x is pairs[len(pairs) // 2])]
        checks['reject_missing_pair'] = base and not all(check(gap, expected, process)[0].values())
        if process and len(process) >= 4:
            leak = [dict(s) for s in process]
            leak[-1]['private_mb'] += 1024
            checks['reject_memory_growth'] = base and not check(native, expected, leak)[0]['private_memory_bounded']
        stalled = [dict(x) for x in native]
        hit = False
        for y in stalled:
            if y['event'] == 'live_native_pair_complete' and y['serial'] == pairs[len(pairs) // 2]['serial']:
                hit = True
            if hit and y['event'] == 'live_native_pair_complete':
                y['us'] += 3_000_000
        checks['reject_stall'] = base and not check(stalled, expected, process)[0]['no_multi_second_stall']
    report = dict(status='PASS' if all(checks.values()) else 'FAIL', checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, stats=stats,
                  limitation='Bounded endurance series only; not unlimited play, comfort, or HMD frame pacing.')
    (a.report or a.probe / 'endurance-stability-checks.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for k, v in checks.items():
        print('PASS' if v else 'FAIL', k)
    print(json.dumps(stats, indent=1))
    print('PASS', sum(checks.values()), 'FAIL', sum(not v for v in checks.values()))
    return int(not all(checks.values()))


if __name__ == '__main__':
    raise SystemExit(main())
