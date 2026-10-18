"""Native resource observation only; not proof of GPU VFX stereo rendering."""
import collections


def check_vfx_transparent(native,skipped=frozenset()):
    # skipped (2026-10-06): desktop-skipped serials. The original View is not rendered, eye 0 owns VFX natively and only
    # eye 1 borrows (observed/rendered through the bridge); eye 0 has no bridge records for those serials.
    def events(name):return [e for e in native if e['event']==name]
    sources=events('vfx_transparent_source')
    source_by_serial={e['serial']:e for e in sources}
    eyes=events('vfx_transparent_eye_observed')
    eye_by_key={(e['serial'],e['eye']):e for e in eyes}
    submitted={e['serial'] for e in events('live_native_pair_complete')}
    removed=events('vfx_transparent_observer_removed')
    checks={
        'vfx_observer_no_failure':not events('vfx_transparent_failure'),
        'vfx_observer_removed':len(removed)==1 and removed[0]['status']==0 and not removed[0]['failed'],
        'vfx_source_once_per_serial':bool(sources) and len(sources)==len(source_by_serial) and all(e['calls']==1 for e in sources),
        'vfx_observer_read_only':bool(eyes) and all(e['read_only'] for e in sources+eyes),
        'vfx_all_submitted_eyes_observed':bool(submitted) and all(((s,1) in eye_by_key and (s,0) not in eye_by_key) if s in skipped else
            ((s,i) in eye_by_key and s in source_by_serial for i in (0,1)) for s in submitted),
        'vfx_eye_observation_unique':len(eyes)==len(eye_by_key),
        'vfx_source_target_buffers_distinct':bool(sources) and all(len({e['target_index'],e['source_index'],e['sort_index']})==3 and 0<e['capacity']<=65536 for e in sources),
    }
    matched=bool(eyes) and all(e['serial'] in source_by_serial for e in eyes if e['serial'] not in skipped)
    checks['vfx_same_serial_resource_identity']=matched and all(
        e['source_counter_offset']==source_by_serial[e['serial']]['counter_offset'] and
        e['source_graph_context']==source_by_serial[e['serial']]['graph_context'] and
        e['counter']!=source_by_serial[e['serial']]['counter'] and e['counter_offset']!=e['source_counter_offset']
        for e in eyes if e['serial'] not in skipped)
    checks['vfx_eye_counters_distinct']=bool(submitted) and all(
        (s,0) in eye_by_key and (s,1) in eye_by_key and eye_by_key[s,0]['counter_offset']!=eye_by_key[s,1]['counter_offset'] for s in submitted if s not in skipped)
    delta_error=0.
    if matched:
        for e in eyes:
            if e['serial'] in skipped:continue
            expected=[(x-y)/4294967296 for x,y in zip(source_by_serial[e['serial']]['position_q32'],e['position_q32'])]
            delta_error=max(delta_error,max(abs(x-y) for x,y in zip(e['delta'],expected)))
    checks['vfx_camera_rebase_matches_origins']=matched and delta_error<1e-6
    enabled=events('vfx_transparent_render_hook_enabled')
    if enabled:
        expected={'0x273e0','0x7b680','0x1753a0','0x104760'}
        graphics_gate=any(e['rva']=='0xf5bcd' for e in enabled)
        binding_gate=any(e['rva']=='0x10f3a7' for e in enabled)
        if graphics_gate:expected.add('0xf5bcd')
        if binding_gate:expected.add('0x10f3a7')
        removed_render=events('vfx_transparent_render_hook_removed')
        complete=events('vfx_transparent_eye_complete')
        submitted_eyes=[e for e in complete if e['serial'] in submitted]
        payloads=events('vfx_transparent_payload')
        checks['vfx_render_hooks_restored']=len(enabled)==len(removed_render)==len(expected) and {e['rva'] for e in enabled}==expected and {e['rva'] for e in removed_render}==expected and all(e['status']==0 for e in removed_render)
        checks['vfx_submitted_stage_counts']=bool(submitted) and collections.Counter((e['serial'],e['eye']) for e in submitted_eyes)==collections.Counter((s,i) for s in submitted for i in ((1,) if s in skipped else (0,1))) and all(
            all(e[k]==1 for k in ('writebacks','allocations','counter_bindings','draw_gates','resource_gets')) for e in submitted_eyes)
        checks['vfx_private_view_ownership_preserved']=bool(complete) and all(e['private_vfx_view_still_null'] for e in complete)
        if graphics_gate:checks['vfx_submitted_graphics_stages']=bool(submitted_eyes) and all(1<=e.get('graphics_gates',0)<=2 for e in submitted_eyes)
        if binding_gate:
            checks['vfx_submitted_binding_stages']=bool(submitted_eyes) and all(e.get('binding_gates')==e.get('binding_resource_gets')==1 for e in submitted_eyes)
            bindings=events('vfx_transparent_graphics_bindings')
            checks['vfx_graphics_descriptors_present']=bool(bindings) and all(int(e[k],16)!=0 for e in bindings for k in ('global_set','vfx_set','eye_set')) and collections.Counter((e['serial'],e['eye']) for e in bindings)==collections.Counter({(e['serial'],e['eye']):e['graphics_gates'] for e in complete if e['graphics_gates']})
        checks['vfx_extended_payload_scoped']=bool(payloads) and all(e['bytes']==28 and e['destination_counter_offset']!=e['source_counter_offset'] and
            (e['serial'],e['eye']) in eye_by_key and e['source_counter_offset']==eye_by_key[e['serial'],e['eye']]['source_counter_offset'] and
            e['destination_counter_offset']==eye_by_key[e['serial'],e['eye']]['counter_offset'] for e in payloads)
        checks['vfx_payloads_match_native_writebacks']=collections.Counter((e['serial'],e['eye']) for e in payloads)==collections.Counter(
            (e['serial'],e['eye']) for e in complete if e['writebacks']==1)
        gates=events('vfx_transparent_draw_gate')
        checks['vfx_draw_gate_only_private_saved_flags']=bool(gates) and all(e['only_saved_zf_changed'] and e['view_pointer_unchanged'] for e in gates)
    return checks,{'vfx_rebase_delta_m':delta_error}
