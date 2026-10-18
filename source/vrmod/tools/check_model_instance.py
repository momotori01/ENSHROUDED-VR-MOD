"""Check unused native model ownership evidence, not rendered VR weapon proof."""
import argparse,json,pathlib

def check(rows,post):
    def event(name):
        found=[r for r in rows if r.get('event')==name]
        return found[0] if len(found)==1 else {}
    source=event('model_instance_source');created=event('model_instance_created')
    moved=event('model_instance_moved');retired=event('model_instance_retired')
    sdf=event('model_instance_sdf_created');sdf_end=event('model_instance_sdf_retired')
    hold=event('model_instance_hold_started');detached=event('model_instance_sdf_detached')
    held=[r for r in rows if r.get('event')=='model_instance_hold_sample']
    if hold:retired=event('model_instance_hold_retired')
    end=event('model_instance_complete');hook=event('model_instance_hook_removed')
    signatures=[r for r in rows if r.get('event')=='model_instance_signature']
    ordered=['model_instance_source','model_instance_sdf_created','model_instance_created','model_instance_moved','model_instance_sdf_retired','model_instance_retired','model_instance_hook_removed','model_instance_complete']
    if hold:ordered=['model_instance_source','model_instance_sdf_created','model_instance_created','model_instance_moved']+(['model_instance_sdf_detached'] if source.get('auxiliary64') else [])+['model_instance_hold_started','model_instance_hold_retired','model_instance_hook_removed','model_instance_complete']
    lifecycle=[r.get('event') for r in rows if r.get('event') in ordered]
    checks={
      'signature_sites_unique':{x['rva'] for x in signatures}=={'0xf7c50','0xd5870','0xc97c0','0xc77d0','0xd9f40','0xc5c60','0x123300','0x128f30'} and len(signatures)==8 and all(x.get('matched') is True for x in signatures),
      'no_failure':not any('failure' in r.get('event','') for r in rows),
      'lifecycle_order':lifecycle==ordered,
      'owned_distinct_handle':bool(source.get('source_handle')) and bool(created.get('handle')) and source['source_handle']!=created['handle']==retired.get('handle'),
      'resource_references_acquired':source.get('references',0)>0 and created.get('references_before')==source.get('references') and created.get('references_after')==source.get('references',0)+1,
      'source_unchanged':moved.get('source_unchanged') is True and retired.get('source_unchanged') is True,
      'native_transform_and_history':moved.get('native_transform_api') is True and moved.get('previous_preserved') is True and moved.get('delta_q32')==536870912,
      'clone_kept_hidden':created.get('mask')==0 and moved.get('mask')==0,
      'sdf_allocation_accounted':bool(sdf) and sdf.get('present')==bool(source.get('auxiliary64')) and sdf.get('count_after')==sdf.get('count_before',-1)+int(bool(sdf.get('present'))),
      'sdf_count_restored':bool(sdf_end) and sdf_end.get('present')==sdf.get('present') and sdf_end.get('count_restored') is True,
      'unknown_auxiliary_refused':source.get('auxiliary68')==0,
      'retirement_generation_and_refcount':retired.get('generation_invalidated') is True and retired.get('references_restored') is True,
      'hook_restored_without_callbacks':hook.get('restored') is True and hook.get('callbacks')==0,
      'completed_successfully':end.get('result')==0,
      'unused_proof_scope':all(r.get('drawn') is False for r in (source,created,end)) and retired.get('gpu_retirement_verified') is False,
      'game_survived_and_rendered':post.get('status')=='PASS' and post.get('expected_process_alive') is True and post.get('rendering_continued_four_seconds') is True and post.get('new_crash_directories')==[],
    }

    if hold:
        checks['sdf_count_restored']=retired.get('sdf_unchanged') is True and (not source.get('auxiliary64') or (detached.get('only_owned_handle_cleared') is True and detached.get('count_before')==sdf.get('count_after') and detached.get('count_after')==sdf.get('count_before')))
        checks['retirement_generation_and_refcount']=retired.get('generation_invalidated') is True and retired.get('references_before',0)>=2 and retired.get('references_after')==retired.get('references_before',0)-1
        checks['held_frames_complete']=hold.get('frames_requested',0)>0 and len(held)==hold['frames_requested']==retired.get('frames') and [e['sample'] for e in held]==list(range(1,len(held)+1))
        checks['held_hidden_sdf_free_source_unchanged']=bool(held) and all(e.get('handle')==created.get('handle') and e.get('mask')==0 and e.get('sdf_handle')==0 and e.get('source_unchanged') is True and e.get('native_transform') is True for e in held)
        checks['held_across_time']=retired.get('elapsed_ms',0)>0
    return checks


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args()
    rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    post=json.loads((a.probe/'probe-postcheck.json').read_text(encoding='utf-8-sig'))
    checks=check(rows,post)
    result={'checks':{k:'PASS' if v else 'FAIL' for k,v in checks.items()},'limitation':__doc__}
    (a.probe/'model-instance-checks.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print('PASS',sum(checks.values()),'FAIL',sum(not v for v in checks.values()))
    return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
