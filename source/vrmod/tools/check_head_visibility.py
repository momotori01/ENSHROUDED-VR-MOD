"""Live private head payload checks; GPU/visual proof is a separate capture comparison."""
from collections import Counter, defaultdict


def check_head(native, gpu, tracking_loss_frames=()):
    def events(rows,name):return [e for e in rows if e['event']==name]
    hands_only=any(e.get('hands_only') for e in events(gpu,'head_shader_patched'))
    patches=events(gpu,'head_shader_patched');on=events(native,'head_visibility_enabled');off=events(native,'head_visibility_removed')
    selections=events(native,'motion_visual_selection')
    selection_by_serial={e['serial']:e for e in selections}
    motion=bool(events(native,'motion_visual_created') or events(native,'motion_visual_context'))
    frame_by_serial={e['serial']:e['recorded_xr_frame'] for e in events(native,'recorded_xr_eye_applied')}
    lost=lambda e:motion and frame_by_serial.get(e['serial']) in tracking_loss_frames
    data=events(native,'head_visibility_payload');fallbacks=events(native,'head_visibility_fallback')
    observations=events(native,'local_equipment_observation');roots=events(native,'root_camera_sample')
    completed=events(native,'live_native_pair_complete');warmup=events(native,'world_warmup_frame_ended')
    serials={e['serial'] for e in completed};warmup_serials={e['serial'] for e in warmup};root_serials={e['render_serial']:e for e in roots}
    native_ids=set(range(4))|{18,30,31,32,33}|set(range(34,43))|set(range(45,256))
    third={e['render_serial'] for e in roots if e.get('camera_modes') and e.get('presentation_id',e.get('camera_id',255)) in native_ids}
    fp_serials=serials-third
    native_eyes=events(native,'head_visibility_native_camera')
    grouped=defaultdict(list)
    for e in data:grouped[e['serial'],e['eye']].append(e)
    checks={
        'head_prepared_dynamic_shader':bool(patches) and all(e['source_sha256']=='04ceb2a4bc7dab905ae022fcbb8464dde3cd78516ecde5978f928d0e4a7b8ff9' and e['patched_sha256'] in ({'3555f30e8d49f16666ce56585733c0f7d3e503c8a1e1005081f04fd7f3dd7845','b18c9028a7c336cdcb943f3010756a81b8544f2839b031f52c2c912f7ec0c481','9c4ff675431d52d49c8c9d2991151a16945b04de7dc58d7ed07043558ea843f9'} if hands_only else {'8cfcb566b2c9afdc4f0bca1e026530b0fb70abf93fa0f8719d64464c48028c9b'}) and e['native_zero_payload_is_inactive'] for e in patches) and not events(gpu,'head_shader_patch_failure'),
        'head_private_upload_hook_verified':len(on)==1 and on[0]['rva']=='0x80db8' and on[0]['payload_offset']==4132 and on[0]['payload_bytes']==12 and on[0]['shader_modules']>0,
        'head_no_rejected_state':not events(native,'head_visibility_failure'),
        'head_upload_hook_restored':len(off)==1 and off[0]['original_bytes_restored'] and off[0]['original_calls_untouched']>0,
        'head_continuous_equipment_observer':len(events(native,'equipment_observer_enabled'))==1 and events(native,'equipment_observer_enabled')[0].get('continuous',False),
        'head_each_submitted_eye_has_both_culling_phases':bool(serials) and all(Counter(e['culling_flags']&1 for e in grouped[s,eye])=={0:1,1:1} for s in fp_serials for eye in (0,1)),
        'head_payload_only_private_valid_frame':bool(data) and all(e['eye'] in (0,1) and e['serial'] in serials|warmup_serials and e['only_private_stack_padding_changed'] and e['handles_revalidated'] for e in data),
        'head_fallbacks_preserve_payload':all(e['payload_unchanged'] for e in fallbacks),
    }
    if third:
        native_grouped=defaultdict(list)
        for e in native_eyes:native_grouped[e['serial'],e['eye']].append(e)
        checks['event_head_visible_both_eyes_and_phases']=all(Counter(e['culling_flags']&1 for e in native_grouped[s,eye])=={0:1,1:1} for s in serials&third for eye in (0,1))
        checks['event_head_payload_untouched']=bool(native_eyes) and all(e['payload_unchanged'] and e['serial'] in third for e in native_eyes) and not any(e['serial'] in third for e in data)
    valid=True
    for e in data:
        ids=sorted(set(e['indices']));p=e['payload'];count=(p[2]>>16)&7
        decoded=[(p[n//2]>>(16*(n%2)))&65535 for n in range(count)]
        if hands_only:
            address=p[0]|p[1]<<32
            valid &= bool(e.get('hands_only')) and p[2]==(0x45564850 if e.get('shield_instance',0xffffffff)!=0xffffffff else 0x4556484f) and address!=0 and address%8==0 and e['hand_instance'] not in ids
        else:valid &= 1<=count<=5 and (p[2]>>16)&0xfff8==0xa5d0 and decoded==ids
    checks['head_payload_decode_matches_indices']=bool(data) and valid
    checks['head_matches_local_root_entity_and_serial']=bool(data) and all(e['serial'] in root_serials and e['entity']==root_serials[e['serial']]['entity'] and 0<=e['snapshot_entry_age_ms']<=1000 for e in data)
    checks['head_snapshot_continues_after_initial_20']=bool(data) and max(e['equipment_sample'] for e in data)>min(e['equipment_sample'] for e in data)+20
    checks['head_same_payload_for_both_eyes']=bool(serials) and all(len({(tuple(e['payload']),e['equipment_sample']) for eye in (0,1) for e in grouped[s,eye]})==1 for s in fp_serials)
    checks['head_recorded_write_counts_match']=len(off)==1 and off[0]['writes']==[sum(e['eye']==eye for e in data) for eye in (0,1)]
    # This check certifies stationary-equipment evidence. Transitions need
    # per-update equipment observations rather than the initial twenty samples.
    selected_parts=set(range(24)) if motion else set(range(22))-{11} if hands_only else {0,1,2,3,15}
    observed={m['handle']&0xffffff for e in observations for m in e['models'] if m['part'] in selected_parts and m['handle']}
    body={m['handle']&0xffffff for e in observations for m in e['models'] if m['part'] not in selected_parts and m['handle']}
    if motion:
        observed|={e['source_handle']&0xffffff for e in events(native,'motion_visual_created')}
        if selections:
            observed|={e['source_handle']&0xffffff for e in selections if e['source_handle']}
            checks['appearance_original_selection_same_frame']=all(e['serial'] in selection_by_serial and selection_by_serial[e['serial']]['source_unchanged'] and (not selection_by_serial[e['serial']]['source_handle'] or (selection_by_serial[e['serial']]['parent']==e['entity'] and (selection_by_serial[e['serial']]['source_handle']&0xffffff) in e['indices'])) for e in data)

    checks['head_observed_parts_only_body_excluded']=bool(data) and bool(observed) and all(set(e['indices'])==observed and not set(e['indices'])&body for e in data)
    switching=bool(events(native,'equipment_fixture_enabled')) or any(e.get('schedule') in ('EVR_QUICK_WHEEL_REBIND_1','EVR_QUICK_WHEEL_CONSUMABLE_1','EVR_QUICK_WHEEL_CONSUMABLE_2','EVR_QUICK_WHEEL_SURVEY_1','EVR_GRIP_ROUTER_EQUIP_1','EVR_GRIP_ROUTER_EQUIP_2') for e in events(native,'quick_wheel_fixture_enabled'))
    if switching:
        # This explicit trial changes held items only. Its same-frame original
        # appearance evidence replaces the old all-frames union of weapon IDs.
        stable_body={m['handle']&0xffffff for e in observations for m in e['models'] if m['part'] in selected_parts and m['handle']}
        checks['head_observed_parts_only_body_excluded']=bool(data) and bool(stable_body) and all(e['serial'] in selection_by_serial and set(e['indices'])==stable_body|({selection_by_serial[e['serial']]['source_handle']&0xffffff} if selection_by_serial[e['serial']]['source_handle'] else set()) for e in data)

    if hands_only:
        checks['appearance_pointer_tag_alignment_valid']=checks.pop('head_payload_decode_matches_indices')
        checks['appearance_body_armour_hidden_hands_weapons_not_selected']=checks.pop('head_observed_parts_only_body_excluded')
        if motion:checks['appearance_original_body_and_weapon_hidden']=checks.pop('appearance_body_armour_hidden_hands_weapons_not_selected')
        hands={m['handle']&0xffffff for e in observations for m in e['models'] if m['part']==11 and m['handle']}
        if motion:hands={e['handle']&0xffffff for e in events(native,'motion_visual_created')}
        alloc={int(e['address'],16):e for e in events(gpu,'body_payload_created')}
        released=events(gpu,'body_payloads_released')
        if motion and not hands and selections:
            # The explicit fallback keeps hands but has no supported weapon.
            # Do not silently treat absence of an owned weapon as a success.
            checks['appearance_current_hand_owned']=bool(data) and bool(events(native,'motion_hand_created')) and all(e['serial'] in selection_by_serial and not selection_by_serial[e['serial']]['weapon_ready'] and e['hand_instance']==0xffffffff for e in data)
        elif switching:
            checks['appearance_current_hand_owned']=bool(hands) and all(e['serial'] in selection_by_serial and (e['hand_instance'] in hands if selection_by_serial[e['serial']]['weapon_ready'] else e['hand_instance']==0xffffffff) for e in data)
        else:
            checks['appearance_current_hand_owned']=bool(hands) and all((e['hand_instance']==0xffffffff if lost(e) else e['hand_instance'] in hands) for e in data)

        checks['appearance_owned_immutable_gpu_payload']=bool(data) and all((e['payload'][0]|e['payload'][1]<<32) in alloc and alloc[e['payload'][0]|e['payload'][1]<<32]['us']<e['us'] and alloc[e['payload'][0]|e['payload'][1]<<32]['tag']==e['payload'][2] and alloc[e['payload'][0]|e['payload'][1]<<32]['hand_plus_one']==(0 if e['hand_instance']==0xffffffff else e['hand_instance']+1) and alloc[e['payload'][0]|e['payload'][1]<<32]['models']==len(e['indices']) and alloc[e['payload'][0]|e['payload'][1]<<32]['immutable'] for e in data)
        extra_created=events(native,'motion_hand_created')
        if extra_created:
            extra={e['handle']&0xffffff for e in extra_created}
            checks['appearance_second_model_prepared_shader']=all(e['patched_sha256'] in {'b18c9028a7c336cdcb943f3010756a81b8544f2839b031f52c2c912f7ec0c481','9c4ff675431d52d49c8c9d2991151a16945b04de7dc58d7ed07043558ea843f9'} for e in patches)
            checks['appearance_second_model_owned']=all((e.get('extra_instance')==0xffffffff if lost(e) or e.get('hands_covered') else e.get('extra_instance') in extra and e['extra_instance'] not in e['indices'] and e['extra_instance']!=e['hand_instance']) for e in data if e['serial'] in serials)
            checks['appearance_second_model_immutable_payload']=all((e['payload'][0]|e['payload'][1]<<32) in alloc and alloc[e['payload'][0]|e['payload'][1]<<32].get('extra_plus_one')==(0 if e.get('extra_instance',0xffffffff)==0xffffffff else e['extra_instance']+1) for e in data)
        if events(native,'motion_shield_created'):
            shields={e['handle']&0xffffff for e in events(native,'motion_shield_created')}
            checks['appearance_third_model_owned']=all(e.get('shield_instance',0xffffffff)==0xffffffff or e['shield_instance'] in shields and e['shield_instance'] not in e['indices'] and e['shield_instance'] not in {e['hand_instance'],e['extra_instance']} for e in data)
            checks['appearance_third_model_immutable_payload']=all(alloc.get(e['payload'][0]|e['payload'][1]<<32,{}).get('shield_plus_one')==(0 if e.get('shield_instance',0xffffffff)==0xffffffff else e['shield_instance']+1) for e in data)
        checks['appearance_allocation_released_on_device_destroy']=len(released)==1 and released[0]['allocations']==len(alloc) and released[0]['boundary']=='native_device_destroy' and all(e['us']<released[0]['us'] for e in data)
    return checks
