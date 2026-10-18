"""Validate a live, private native-UI readback (not a stereo or comfort test)."""
import argparse,json,pathlib,hashlib
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=pathlib.Path);p.add_argument('--require-cleanup',action='store_true');a=p.parse_args()
    events=[json.loads(s) for s in (a.launch/'vulkan.jsonl').read_text(encoding='utf-8-sig').splitlines()]
    checks={};details=[]
    captures=[e for e in events if e['event']=='ui_capture_saved']
    checks['capture_observed']=bool(captures)
    checks['validation_empty']=(a.launch/'validation.log').is_file() and (a.launch/'validation.log').stat().st_size==0
    ready=[e for e in events if e['event']=='ui_pipeline_clone_ready']
    checks['normal_additive_font_clones']=len(ready)==3 and sum(bool(e['additive_alpha_zero']) for e in ready)==1
    checks['no_capture_errors']=not any(e['event'] in ('ui_capture_begin_failed','ui_capture_end_failed','ui_capture_readback_failed') for e in events)
    for event in captures:
        frame=event['frame'];stem=a.launch/f'ui-frame-{frame}';raw=stem.with_suffix('.rgba8').read_bytes();meta=json.loads(stem.with_suffix('.json').read_text())
        checks[f'{frame}_metadata_matches_log']=all(meta[k]==event[k] for k in meta)
        checks[f'{frame}_dimensions']=0<meta['width']<=8192 and 0<meta['height']<=8192 and len(raw)==meta['width']*meta['height']*4
        alpha=raw[3::4];transparent=alpha.count(0);opaque=alpha.count(255);partial=len(alpha)-transparent-opaque
        rgb=sum(any(raw[i:i+3]) for i in range(0,len(raw),4))
        checks[f'{frame}_transparent_region']=transparent>len(alpha)//20
        checks[f'{frame}_visible_ui']=rgb>32 and partial+opaque>32
        checks[f'{frame}_bounded_coverage']=partial+opaque<len(alpha)*95//100
        checks[f'{frame}_gpu_complete']=event['queue_idle'] is True and event['version']>0 and event['draws']>0
        details.append(dict(frame=frame,sha256=hashlib.sha256(raw).hexdigest(),transparent=transparent,opaque=opaque,partial=partial,rgb_pixels=rgb))
    if a.require_cleanup:
        exited=json.loads((a.launch/'process-exit.json').read_text(encoding='utf-8-sig'))
        checks['normal_exit']=exited['status']=='exited' and exited['exit_code']==0
        cleanup=[e for e in events if e['event']=='ui_capture_cleanup']
        checks['targets_released_after_idle']=len(cleanup)==1 and 1<=cleanup[0]['targets']<=8 and cleanup[0]['idle_result']==0
        destroyed=[e for e in events if e['event']=='ui_pipeline_clone_destroyed']
        checks['all_clones_destroyed']={e['clone'] for e in ready}=={e['clone'] for e in destroyed}
        cleanup=[e for e in events if e['event']=='ui_pipeline_probe_cleanup']
        checks['no_pipeline_or_shader_records_left']=len(cleanup)==1 and cleanup[0]['remaining_clones']==0 and cleanup[0]['remaining_shader_records']==0
    report=dict(validation_mode=json.loads((a.launch/'validation-config.json').read_text(encoding='utf-8-sig'))['mode'],checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},images=details,limits=[
        'Pixels detect obvious empty/opaque failures, not UI completeness or correct gameplay behavior.',
        'This launch does not prove OpenXR quad submission, menus, stereo image fidelity or HMD comfort.',
        'Native desktop byte equality is separately tested in the standalone GPU fixture; no live unchanged-frame comparison is claimed.'
    ])
    (a.launch/'ui-capture-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print(sum(checks.values()),'PASS /',sum(not v for v in checks.values()),'FAIL')
    return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
