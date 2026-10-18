"""CPU duration telemetry only; never infer GPU time or motion-to-photon latency."""
import argparse
import collections
import json
import pathlib
import statistics


def rows(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line]


def distribution(values):
    values=sorted(values)
    if not values:return None
    return dict(count=len(values),minimum=values[0],median=statistics.median(values),
                p95=values[max(0,(95*len(values)+99)//100-1)],maximum=values[-1])


def analyze(probe):
    launch=probe.parent
    native=rows(probe/'view-resource-probe.jsonl')
    xr=rows(launch/'native-xr/openxr.jsonl')
    timings=[r for r in xr if r['event']=='native_completion_cpu_timing']
    submitted=[r for r in xr if r['event']=='native_game_pair_submitted']
    stages=['evidence_us','device_idle_us','transfer_and_capture_us','end_frame_us']
    checks={
        'one_timing_per_submitted_pair':bool(timings) and collections.Counter(r['frame'] for r in timings)==collections.Counter(r['frame'] for r in submitted) and len({r['frame'] for r in timings})==len(timings),
        'nonnegative_cpu_durations':bool(timings) and all(type(r[k])==int and r[k]>=0 for r in timings for k in stages+['total_us']),
        'contiguous_stage_duration_sum':bool(timings) and all(0<=r['total_us']-sum(r[k] for k in stages)<=3 for r in timings),
        'capture_flag_explicit':bool(timings) and all(type(r['capture'])==bool for r in timings),
    }
    groups={name:{k:distribution([r[k] for r in timings if predicate(r)]) for k in stages+['total_us']}
            for name,predicate in [('all',lambda r:True),('ordinary',lambda r:not r['capture']),('capture',lambda r:r['capture'])]}
    detail_keys=['transfer_free_us','transfer_allocate_us','transfer_record_us',
                 'transfer_submit_wait_us','transfer_map_us','transfer_write_us']
    detail={name:{k:distribution([r[k] for r in timings if predicate(r) and k in r]) for k in detail_keys}
            for name,predicate in [('ordinary',lambda r:not r['capture']),('capture',lambda r:r['capture'])]}
    queue=[r['acquire_cpu_us'] for r in native if r['event']=='game_session_queue_lease' and 'acquire_cpu_us' in r]
    report=dict(scope=__doc__,probe=str(probe),checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},
                unit='microseconds',completion=groups,transfer_detail=detail,native_queue_lease=distribution(queue),
                capture_memory=[dict(frame=r['frame'],types=r['readback_memory_types'],flags=r['readback_memory_flags'])
                                for r in timings if r['capture'] and 'readback_memory_flags' in r],
                limits=['Core validation and diagnostic readbacks affect durations.',
                        'Does not measure time before entry to evrFinishNativeXr.',
                        'Device idle includes all earlier device work; transfer includes fence wait and optional disk capture.'])
    (probe/'native-cpu-timing.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    print(json.dumps(report,indent=2))
    return int(not all(checks.values()))


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('probe',type=pathlib.Path)
    raise SystemExit(analyze(parser.parse_args().probe))
