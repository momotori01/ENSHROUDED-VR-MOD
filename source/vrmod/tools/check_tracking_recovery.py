"""Check real world stereo suspends during invalid Mock poses and resumes.

This certifies the recorded temporary-loss sequence only, not real Runtime
disconnect/recreation, comfort, latency, controller input, or whole image quality.
"""
import argparse,collections,csv,hashlib,json,pathlib

def load(path):return [json.loads(s) for s in path.read_text(encoding='utf-8-sig').splitlines() if s.strip()]

def check_tracking(native,xr,mock,pose_rows):
    def events(rows,name):return [e for e in rows if e['event']==name]
    waits=events(mock,'wait_frame');complete=events(xr,'empty_frame_completed')
    client_results=sorted(complete+events(xr,'empty_frame')+events(xr,'stereo_frame'),key=lambda e:e['us'])
    # Original empty_frame is an early reason log. Its completion supplies the
    # actual successful end boundary; application-deferred warmup logs already
    # occur after xrEndFrame and need no second event.
    completed={e['frame'] for e in complete}
    client_results=[e for e in client_results if e['event']!='empty_frame' or e['frame'] not in completed]
    mapping={app['frame']:runtime['frame'] for runtime,app in zip(waits,client_results)}
    wanted={f for f,runtime in mapping.items() if int(pose_rows[(runtime-1)%len(pose_rows)][8])==0}
    skipped=events(native,'world_tracking_frame_deferred');bridge=events(native,'live_native_frame_deferred');runtime_skip=events(xr,'native_frame_deferred')
    skipped_ids={e['frame'] for e in skipped};serials={e['serial'] for e in skipped}
    begun=events(xr,'native_frame_begin');published=events(native,'input_pose_published');submitted=events(xr,'native_game_pair_submitted')
    heads=events(native,'input_head_walking_sample');native_heads=events(xr,'hmd')
    checks={
        'tracking_fixture_runtime_frames_mapped':len(waits)==len(client_results)==len(mapping) and all(b['frame']>a['frame'] for a,b in zip(client_results,client_results[1:])),
        'tracking_loss_matches_explicit_csv':bool(wanted) and wanted==skipped_ids=={e['frame'] for e in bridge}=={e['frame'] for e in runtime_skip}=={e['frame'] for e in complete} and len(skipped)==len(bridge)==len(runtime_skip)==len(wanted),
        'tracking_no_stale_hmd_publication':not wanted.intersection(e['frame'] for e in begun+published+native_heads),
        'tracking_no_eye_draw_or_layer_during_loss':not wanted.intersection(e['frame'] for e in submitted) and not serials.intersection(e['serial'] for e in events(native,'calibration_eye_recorded')),
        'tracking_empty_frames_end_once':all(e['empty_frame_ended'] and e['head_flags']==0 for e in runtime_skip) and all(e['layers']==0 for e in complete),
        'tracking_one_attempt_per_game_serial':len(serials)==len(skipped) and all(b['serial']>a['serial'] for a,b in zip(skipped,skipped[1:])),
        'tracking_input_invalidated_before_fallback':bool(bridge) and all(e['input_pose_invalidated'] and e['head_flags']==0 for e in bridge),
        'tracking_same_session_and_origin_after_recovery':len({e['session_generation'] for e in begun+runtime_skip+published})==1 and len({e['origin_epoch'] for e in published})==1,
    }
    groups=[]
    for f in sorted(wanted):
        if groups and groups[-1][-1]+1==f:groups[-1].append(f)
        else:groups.append([f])
    by_skipped={e['frame']:e for e in skipped};by_begin={e['frame']:e for e in begun};by_pub={e['frame']:e for e in published}
    reports=[]
    for group in groups:
        first,last=group[0],group[-1];start=by_skipped[first]['us'];end=by_pub.get(last+1,{}).get('us',start)
        during=[e for e in heads if start<=e['us']<end]
        following=[e for e in heads if e['us']>=end and e.get('frame')==last+1 and e['applied']]
        reports.append(dict(first=first,last=last,empty_frames=len(group),game_serials=[by_skipped[first]['serial'],by_skipped[last]['serial']],
            duration_us=end-start,fallback_samples=len(during),recovered_head_samples=len(following),
            native_controls_preserved=bool(during) and all(not e['applied'] and e['status']=='tracking' and e['native_preserved'] and e['native_stick']==[0,0] and e['native_digital']==0 for e in during),
            recovered=last+1 in by_begin and bool(following)))
    checks['tracking_all_loss_intervals_preserve_native_input']=bool(reports) and all(r['native_controls_preserved'] for r in reports)
    checks['tracking_all_intervals_resume_new_hmd_input']=bool(reports) and all(r['recovered'] for r in reports)
    checks['tracking_prolonged_loss_exceeds_old_retry_limit']=any(r['empty_frames']>20 for r in reports)
    checks['tracking_real_pairs_before_and_after_loss']=bool(groups) and any(e['frame']<groups[-1][0] for e in submitted) and any(e['frame']>groups[-1][-1] for e in submitted)
    return checks,dict(intervals=reports,total_empty_frames=len(wanted),rendered_pairs=len(submitted))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();launch=a.probe.parent
    if not (launch/'process-exit.json').exists():p.error('normal game shutdown must be recorded first')
    native=load(a.probe/'view-resource-probe.jsonl');lo=min(e['us'] for e in native);hi=max(e['us'] for e in native)
    xr=[e for e in load(launch/'native-xr/openxr.jsonl') if lo<=e['us']<=hi];mock=[e for e in load(launch/'mock.jsonl') if lo<=e['us']<=hi]
    with (launch/'mock-pose.csv').open() as f:poses=list(csv.reader(line for line in f if not line.startswith('#')))
    checks,metrics=check_tracking(native,xr,mock,poses)
    checks['tracking_full_world_checks_pass']=all(v=='PASS' for v in json.loads((a.probe/'live-native-checks.json').read_text())['checks'].values())
    report=dict(scope=__doc__,checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},metrics=metrics,
        pose_csv_sha256=hashlib.sha256((launch/'mock-pose.csv').read_bytes()).hexdigest())
    (a.probe/'tracking-recovery-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print(sum(checks.values()),'PASS /',sum(not v for v in checks.values()),'FAIL');return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
