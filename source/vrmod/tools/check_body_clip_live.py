"""Audit private-eye body clip publication and native lifetime.

This checks stationary equipment CPU/GPU-address evidence, not all garment
shaders, shadows, actual animation states, or subjective comfort. GPU capture
comparison is separately required. Never run a final audit before game exit.
"""
import argparse
import collections
import json
import math
from pathlib import Path

SOURCES = {
    'depth': ('e7ddf2d6f02a752e57e06f9a9ca780421c30c711ccfe0dcbadab6364313d8934',
              'd2a76728ec37aa6c8bb8dad108d85fcb6e22aba9919398ce96b10b74241b453f'),
    'color': ('fd8ea5dffef32fd659442ec9fce7011f4ede8703e1a34380c10eac39c361ff8a',
              'b0a66863dce6a7c8df8f01df4b45b69319d915482780db94b590406e17896998'),
}

def rows(p):
    return [json.loads(s) for s in p.read_text(encoding='utf-8-sig').splitlines() if s.strip()]


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('probe',type=Path)
    parser.add_argument('--allow-unvalidated-capture',action='store_true',help='RenderDoc evidence only; excludes validation acceptance and labels it NOT RUN')
    args=parser.parse_args();p=args.probe;launch=p.parent
    config_launch=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
    if args.allow_unvalidated_capture and not config_launch.get('renderdoc'):parser.error('capture-only option requires a recorded RenderDoc launch')
    if not (launch/'process-exit.json').exists():parser.error('normal managed game exit is required')
    native=rows(p/'view-resource-probe.jsonl');gpu=rows(launch/'vulkan.jsonl')
    def events(data,name):return [e for e in data if e['event']==name]
    config=(p/'world-body-clip.txt').read_text().split();distance=float(config[1]);parts=set(map(int,config[2:]))
    glide_file=p/'world-body-glide-clip.txt';glide_distance=None
    if glide_file.exists():
        glide_config=glide_file.read_text().split()
        if len(glide_config)!=2 or glide_config[0]!='EVR_BODY_GLIDE_CLIP_1':raise ValueError('glide clip config format')
        glide_distance=float(glide_config[1])
        if not math.isfinite(glide_distance) or not distance<=glide_distance<=1:raise ValueError('glide clip distance invalid')
    motion_file=p/'world-body-motion-clip.txt';motion_distance=None
    if motion_file.exists():
        motion_config=motion_file.read_text().split()
        if len(motion_config)!=2 or motion_config[0]!='EVR_BODY_MOTION_CLIP_1':raise ValueError('motion clip config format')
        motion_distance=float(motion_config[1])
        if glide_distance is not None or not math.isfinite(motion_distance) or not distance<=motion_distance<=1:raise ValueError('motion clip distance invalid')
    actor_observed=events(native,'body_motion_actor_observation')
    actor={e['sample']:e for e in actor_observed}
    motion_frames=events(native,'body_motion_frame')
    motion={e['serial']:e for e in motion_frames}
    data=events(native,'body_clip_payload');inactive=events(native,'body_clip_inactive')
    installed=events(native,'body_upload_hook_enabled');removed=events(native,'body_upload_hook_removed')
    original=events(native,'body_upload_observation')
    patches=events(gpu,'body_shader_patched');created=events(gpu,'body_payload_created');released=events(gpu,'body_payloads_released')
    serials={e['serial'] for e in events(native,'live_native_pair_complete')}
    warmup={e['serial'] for e in events(native,'world_warmup_frame_ended')}
    root={e['render_serial']:e for e in events(native,'root_camera_sample')}
    # Derive native camera decisions from existing head visibility evidence;
    # do not independently guess presentation policy from a broad ID range.
    third={e['serial'] for e in events(native,'head_visibility_native_camera')}
    fp=serials-third;all_fp=(serials|warmup)-third
    recorded={e['serial'] for e in events(native,'calibration_eye_recorded')}
    rendered_fp=all_fp&recorded
    # Early pipeline-not-ready warmup can return before the native draw upload.
    # It must submit no layers and contain no completed eye records or writes.
    skipped_warmup=warmup-recorded
    grouped=collections.defaultdict(list)
    for e in data:grouped[e['serial']].append(e)
    observed=events(native,'local_equipment_observation')
    ids={m['handle']&0xffffff for e in observed for m in e['models'] if m['part'] in parts and m['handle']}
    foreign={m['handle']&0xffffff for e in observed for m in e['models'] if m['part'] not in parts and m['handle']}
    alloc={e['address']:e for e in created}
    def expected_motion(serial):
        m=motion.get(serial,{});a=actor.get(m.get('actor_sample'),{});r=root.get(serial,{})
        effective=(a.get('raw',0)|a.get('add',0))&~a.get('remove',0) if a.get('sequence',0)&1 else a.get('raw',0)
        return bool(a and m.get('known') and a.get('read_only') and a.get('entity')==r.get('entity') and serial not in third
                    and not r.get('synthetic_presentation',True) and 0<=m.get('render_entry_ms',0)-a.get('milliseconds',0)<=200
                    and not effective&(1<<7) and effective&((1<<1)|(1<<3)|(1<<32)|(1<<37)))
    def expected_distance(e):
        if motion_distance is not None and expected_motion(e['serial']):return motion_distance
        r=root.get(e['serial'],{})
        return glide_distance if glide_distance is not None and r.get('camera_id')==25 and not r.get('synthetic_presentation',True) else distance
    distance_keys={round(expected_distance(e),6) for e in data}
    checks={
        'explicit_config':config[0]=='EVR_BODY_CLIP_1' and bool(parts) and all(4<=x<24 and x!=15 for x in parts),
        'game_exited_normally':json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0,
        'body_device_prepared':len(events(gpu,'body_clip_device_ready'))==1,
        'both_reviewed_shaders_patched':{e['kind'] for e in patches}==set(SOURCES) and all((e['source_sha256'],e['patched_sha256'])==SOURCES[e['kind']] and e['zero_payload_inactive'] for e in patches),
        'no_body_failures':not [e for e in native+gpu if e['event'].startswith('body_') and ('failure' in e['event'] or 'fallback' in e['event'])],
        'both_upload_hooks_installed':len(installed)==2 and {e['kind'] for e in installed}==set(SOURCES) and all(not e['read_only'] for e in installed),
        'hooks_restored_original_calls_observed':len(removed)==2 and all(e['original_bytes_restored'] and e['original_count']>0 for e in removed),
        'four_uploads_each_submitted_first_person_frame':bool(fp) and all(collections.Counter((e['eye'],e['kind']) for e in grouped[s])=={(i,k):1 for i in (0,1) for k in SOURCES} for s in fp),
        'four_uploads_each_warmup_first_person_frame':bool(warmup) and all(collections.Counter((e['eye'],e['kind']) for e in grouped[s])=={(i,k):1 for i in (0,1) for k in SOURCES} for s in rendered_fp),
        'unrendered_warmup_submits_no_layers_or_payload':all(e['layers']==0 for e in events(native,'world_warmup_frame_ended') if e['serial'] in skipped_warmup) and not any(e['serial'] in skipped_warmup for e in data),
        'all_writes_only_accounted_private_eyes':bool(data) and all(e['eye'] in (0,1) and e['serial'] in all_fp for e in data),
        'only_eight_byte_padding_changed':bool(data) and all(e['only_private_padding_changed'] and e['offset']==(472 if e['kind']=='depth' else 632) for e in data),
        'same_immutable_payload_all_four_uploads':bool(data) and all(len({(e['address'],tuple(e['indices']),e['equipment_sample']) for e in v})==1 for v in grouped.values()),
        'valid_current_local_handles':bool(data) and all(e['handles_revalidated'] and e['serial'] in root and e['entity']==root[e['serial']]['entity'] for e in data),
        'stationary_selected_equipment_only':bool(ids) and bool(data) and all(set(e['indices'])==ids and not set(e['indices'])&foreign for e in data),
        'requested_distance_unchanged':bool(data) and all(math.isclose(e['distance'],expected_distance(e),abs_tol=1e-7) for e in data),
        'gpu_allocation_precedes_each_use':bool(alloc) and len(alloc)==len(created) and all(e['address'] in alloc and alloc[e['address']]['us']<e['us'] and int(e['address'],16)%8==0 for e in data),
        'allocation_immutable_and_correct_size':bool(created) and all(e['immutable'] and e['bytes']==112 and e['models']==len(ids) for e in created),
        'one_immutable_allocation_per_distance_and_equipment':len(created)==len(distance_keys) and {round(e['distance'],6) for e in created}==distance_keys,
        'allocations_released_only_at_device_destroy':len(released)==1 and released[0]['allocations']==len(created) and released[0]['boundary']=='native_device_destroy' and released[0]['us']>max(e['us'] for e in data) and any(e['us']>released[0]['us'] for e in events(gpu,'destroy_device')),
        'balanced_eye_upload_counters':len(removed)==2 and all(e['eye_counts']==[sum(d['kind']==e['kind'] and d['eye']==i for d in data)+sum(d['kind']==e['kind'] and d['eye']==i for d in inactive) for i in (0,1)] for e in removed),
        'initial_native_padding_zero_all_phases':len(original)==24 and all(e['candidate_padding']=='0x0' and e['read_only'] and e['source_unchanged'] for e in original),
        'initial_native_padding_binary_evidence':bool(original) and all(len((p/e['file']).read_bytes())==e['bytes'] and (p/e['file']).read_bytes()[e['candidate_offset']:e['candidate_offset']+8]==bytes(8) for e in original),
        'no_unexpected_inactive_fallback':all(e['serial'] in third and e['reason']=='native camera or absent current equipment' for e in inactive),
        'core_sync_validation_clean':config_launch.get('vulkan_validation')=='Sync' and (launch/'validation.log').exists() and not (launch/'validation.log').read_text(encoding='utf-8',errors='replace').strip(),
    }
    if glide_distance is not None:
        checks['glide_override_matches_copied_camera']=all(e.get('camera_id')==root[e['serial']]['camera_id'] and e.get('glide_override')==(root[e['serial']]['camera_id']==25 and not root[e['serial']]['synthetic_presentation']) for e in data)
        checks['glide_and_base_both_observed']=len(distance_keys)==2
        checks['base_distance_restored_after_glide']=bool(data) and math.isclose(data[-1]['distance'],distance,abs_tol=1e-7) and not data[-1]['glide_override']
    if motion_distance is not None:
        checks['motion_source_raw_fields_independently_derived']=bool(actor) and len(actor)==len(actor_observed) and all(e['read_only'] and e['state']==(((e['raw']|e['add'])&~e['remove']) if e['sequence']&1 else e['raw']) for e in actor_observed)
        checks['motion_frozen_once_per_render_entry']=bool(motion) and len(motion)==len(motion_frames) and all(e['serial'] in motion for e in data)
        checks['motion_source_identity_and_publication_order']=all(m['actor_sample'] in actor and actor[m['actor_sample']]['us']<=m['us'] and m['actor_ms']==actor[m['actor_sample']]['milliseconds'] and m['actor_state']==actor[m['actor_sample']]['state'] and m['actor_entity']==actor[m['actor_sample']]['entity'] for m in motion_frames if m['known'])
        checks['motion_render_owner_and_root_sample']=all(m['root_entity']==root[n]['entity'] and m['root_sample']==root[n]['sample'] for n,m in motion.items() if n in root)
        checks['motion_all_four_uploads_match_independent_actor_policy']=all(e['motion_override']==expected_motion(e['serial']) and e['actor_sample']==motion[e['serial']]['actor_sample'] and motion[e['serial']]['us']<e['us'] for e in data)
        checks['motion_and_base_both_observed']=len(distance_keys)==2
        checks['base_restored_after_motion']=bool(data) and not expected_motion(data[-1]['serial']) and math.isclose(data[-1]['distance'],distance,abs_tol=1e-7)
        checks['actual_jump_and_glide_overrides_observed']=all(any(e['motion_override'] and actor.get(e['actor_sample'],{}).get('state',0)&(1<<bit) for e in data) for bit in (1,37))
    if args.allow_unvalidated_capture:
        del checks['core_sync_validation_clean']
        print('NOT RUN Core+Sync acceptance: this is RenderDoc evidence only')
    if third:
        checks['native_event_payload_never_written']=not any(e['serial'] in third for e in data)
        checks['native_event_all_four_uploads_inactive']=all(collections.Counter((e['eye'],e['kind']) for e in inactive if e['serial']==s)=={(i,k):1 for i in (0,1) for k in SOURCES} for s in serials&third)
        checks['first_person_restored_after_native']=bool(fp) and max(fp)>max(serials&third)
    report={'validation_acceptance':'NOT RUN' if args.allow_unvalidated_capture else 'REQUIRED', 'checks':checks,'passed':sum(checks.values()),'failed':sum(not v for v in checks.values()),'submitted_pairs':len(serials),'warmup_pairs':len(warmup),'payload_uploads':len(data),'native_pairs':len(serials&third),'parts':sorted(parts),'distance':distance,'limitation':__doc__}
    (p/'body-clip-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for k,v in checks.items():print(('PASS' if v else 'FAIL'),k)
    print(f"{report['passed']} PASS / {report['failed']} FAIL")
    return int(report['failed']>0)

if __name__=='__main__':raise SystemExit(main())
