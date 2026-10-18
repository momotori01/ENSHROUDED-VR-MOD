"""Bounded real-game endurance evidence, not playability or latency certification.

Monotonic wall time is distinct from the Mock's synthetic predicted display time.
Memory samples are observational: sparse process measurements cannot prove an
absence of leaks. Run after ordinary game shutdown and the integrated checker.
"""
import argparse, json, math
from pathlib import Path


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]


def percentile(values, fraction):
    values=sorted(values)
    return values[min(len(values)-1,math.floor((len(values)-1)*fraction))] if values else None


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe',type=Path)
    p.add_argument('--minimum-seconds',type=float,default=180)
    a=p.parse_args()
    if not math.isfinite(a.minimum_seconds) or a.minimum_seconds<=0:p.error('positive finite duration required')
    launch=a.probe.parent;n=rows(a.probe/'view-resource-probe.jsonl')
    first,last=min(e['us'] for e in n),max(e['us'] for e in n)
    x=[e for e in rows(launch/'native-xr/openxr.jsonl') if first<=e['us']<=last]
    pairs=[e for e in x if e['event']=='native_game_pair_submitted']
    budget=[e for e in n if e['event']=='view_resource_diagnostic_budget']
    _,requested,_=(a.probe/'live-xr-config.txt').read_text().split();requested=int(requested)
    span=(pairs[-1]['us']-pairs[0]['us'])/1e6 if len(pairs)>1 else 0
    gaps=[(b['us']-c['us'])/1000 for c,b in zip(pairs,pairs[1:])]
    heads=[e for e in x if e['event']=='hmd']
    positions={tuple(round(v,6) for v in e['position']) for e in heads}
    orientations={tuple(round(v,6) for v in e['orientation']) for e in heads}
    integrated=json.loads((a.probe/'live-native-checks.json').read_text())
    applied=[e for e in n if e['event']=='input_head_idle_sample' and e['applied']]
    applied_span=(applied[-1]['us']-applied[0]['us'])/1e6 if len(applied)>1 else 0
    checks={
        'explicit_endurance_policy':(a.probe/'world-endurance.txt').read_text().strip()=='EVR_WORLD_ENDURANCE_1',
        'bounded_endurance_budget':len(budget)==1 and budget[0]['endurance'] and budget[0]['seconds']==420 and budget[0]['frames']==requested and 301<=requested<=3000,
        'all_requested_pairs':len(pairs)==requested,
        'required_wall_clock_duration' : span>=a.minimum_seconds,
        'strictly_increasing_pair_times':bool(gaps) and min(gaps)>0,
        'varied_physical_pose':len(positions)>=50 and len(orientations)>=50,
        'core_validation_enabled':json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))['vulkan_validation']=='Core',
        'core_validation_no_messages':(launch/'validation.log').stat().st_size==0,
        'integrated_native_checks':bool(integrated['checks']) and all(v=='PASS' for v in integrated['checks'].values()),
    }
    if (a.probe/'world-input-head-idle.txt').exists():checks['idle_camera_applied_over_interval']=applied_span>=a.minimum_seconds
    memory_path=launch/'endurance-process-memory.jsonl'
    report=dict(scope=__doc__,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},
        metrics=dict(pairs=len(pairs),paired_wall_seconds=span,required_wall_seconds=a.minimum_seconds,
          diagnostic_pair_interval_ms=dict(p50=percentile(gaps,.5),p95=percentile(gaps,.95),maximum=max(gaps,default=None)),
          idle_camera_applied=len(applied),idle_camera_wall_seconds=applied_span,
          distinct_positions=len(positions),distinct_orientations=len(orientations)),
        process_memory=rows(memory_path) if memory_path.exists() else [])
    (a.probe/'native-endurance-checks.json').write_text(json.dumps(report,indent=2),encoding='utf8')
    for k,v in report['checks'].items():print(v,k)
    print(report['metrics']);print('PASS',sum(checks.values()),'FAIL',len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
