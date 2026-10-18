"""Read-only native movement observation checks; not VR input integration proof."""
import argparse,json,math,pathlib,struct
from check_picking_ray import check_picking

def check_input_direction(native,pose_expectation='dynamic',*,picking_stopped_at=None):
    def events(n):return [e for e in native if e['event']==n]
    samples=events('input_direction_sample');removed=events('input_direction_observer_removed')
    checks={
        'input_observer_no_failure':not events('input_direction_failure'),
        'input_observer_restored':len(removed)==1 and removed[0]['restored'] and removed[0]['remaining_callbacks']==0 and removed[0]['failures']==0,
        'input_samples_accounted':bool(samples) and len(removed)==1 and len(samples)==removed[0]['samples']<=256 and removed[0]['calls']>=len(samples),
        'input_local_entity_only':bool(samples) and all(e['entity']==e['local_entity'] and e['entity']!=0 and e['read_only'] for e in samples),
        'input_known_native_callers':bool(samples) and all(e['caller'] in ('0x2883e1','0x2884a5') for e in samples),
        'input_valid_modes_and_sticks':bool(samples) and all(e['input_mode'] in (0,1) and all(math.isfinite(x) and abs(x)<=1.001 for x in e['movement_stick']) for e in samples),
    }
    error=0.;unit_error=0.
    for e in samples:
        x,y,z,w=e['camera_orientation'];unit_error=max(unit_error,abs(x*x+y*y+z*z+w*w-1))
        # Native padded row-vector matrix, independently expanded from xyzw.
        wanted=[1-2*(y*y+z*z),2*(x*y+w*z),2*(x*z-w*y),
                2*(x*y-w*z),1-2*(x*x+z*z),2*(y*z+w*x),
                2*(x*z+w*y),2*(y*z-w*x),1-2*(x*x+y*y)]
        if len(e['native_basis'])!=9 or not all(math.isfinite(v) for v in wanted+e['native_basis']):error=float('inf');continue
        error=max(error,max(abs(a-b) for a,b in zip(wanted,e['native_basis'])))
    checks['input_native_basis_matches_camera']=bool(samples) and error<2e-6 and unit_error<2e-5
    camera_comparison={}
    ranged_metrics={}
    if events('input_ranged_enabled'):
        ranged=events('input_ranged_sample');ended=events('input_ranged_removed')
        checks['input_ranged_no_failure']=not events('input_ranged_failure')
        checks['input_ranged_shared_hook_restored']=len(ended)==1 and ended[0]['restored'] and ended[0]['failures']==ended[0]['callbacks']==0
        checks['input_ranged_samples_accounted']=bool(ranged) and len(ended)==1 and len(ranged)==ended[0]['samples']<=128 and ended[0]['calls']>=len(ranged)
        checks['input_ranged_read_only_local']=bool(ranged) and all(e['read_only'] and e['entity']==e['local_entity']!=0 and e['caller']=='0x290957' for e in ranged)
        checks['input_ranged_schema_valid']=bool(ranged) and all(e['weapon_state'] in range(5) and type(e['turn_towards_target'])==bool and type(e['fire_queued'])==bool and 0<=e['missing_resources']<=65535 and
            all(0<=e[k]['slot']<=255 and 0<=e[k]['state_mask']<=65535 for k in ('weapon_instance','ammo_instance','action_item')) for e in ranged)
        ranged_metrics=dict(ranged_states=sorted({e['weapon_state'] for e in ranged}),ranged_samples=len(ranged),
            ranged_weapon_items=sorted({e['weapon_instance']['item_id'] for e in ranged}),ranged_action_items=sorted({e['action_item']['item_id'] for e in ranged}),
            ranged_turn_towards_target_samples=sum(e['turn_towards_target'] for e in ranged))
        if any(e.get('item_metadata_version')==1 for e in events('input_ranged_enabled')):
            def valid_metadata(m):
                return isinstance(m,dict) and 0<m.get('item_id',0)<2**32 and 0<=m.get('category',-1)<=16 and 0<=m.get('ammunition_type',-1)<=3 and 0<=m.get('equipment_slot',-1)<=40 and type(m.get('ranged_weapon'))==bool and m['ranged_weapon']==(m['category']==2 and m['equipment_slot'] in (4,5))
            def valid_instance(item):
                if item['item_id']==0:return item.get('lookup')=='no_item' and item.get('metadata_valid') is False and item.get('metadata') is None
                return item.get('lookup')=='found' and item.get('metadata_valid') is True and valid_metadata(item.get('metadata')) and item['item_id']==item['metadata']['item_id']
            catalog=events('input_item_catalog');entries=events('input_item_catalog_entry')
            checks['input_item_catalog_read_only_stable']=len(catalog)==1 and catalog[0]['read_only'] and catalog[0]['stable_header'] and 0<catalog[0]['capacity']<=2**20 and not catalog[0]['capacity']&(catalog[0]['capacity']-1) and 0<catalog[0]['count']<=catalog[0]['capacity'] and 0<len(entries)==catalog[0]['entries']<=24 and 0<catalog[0]['scanned']<=min(4096,catalog[0]['capacity'])
            checks['input_item_catalog_identity_and_types']=bool(entries) and len(catalog)==1 and all(e['read_only'] and e['pointer_matches'] and e['entity']==catalog[0]['entity'] and e['lookup_index']==e['index']<catalog[0]['scanned'] and 0<e['probes']<=4096 and valid_metadata(e['metadata']) for e in entries) and len({e['metadata']['item_id'] for e in entries})==len(entries)
            checks['input_item_selected_identity_and_types']=bool(ranged) and all(all(k in e and valid_instance(e[k]) for k in ('weapon_instance','ammo_instance','action_item','sequence_item','sequence_ammo')) for e in ranged)
            ranged_metrics.update(item_catalog_entries=len(entries),item_catalog_categories=sorted({e['metadata']['category'] for e in entries}),
                selected_nonzero_items=sum(bool(e[k]['item_id']) for e in ranged for k in ('weapon_instance','ammo_instance','action_item','sequence_item','sequence_ammo')))
    if events('input_actor_final_enabled'):
        final=events('input_actor_final_sample');end=events('input_actor_final_removed')
        checks['input_actor_final_no_failure']=not events('input_actor_final_failure')
        checks['input_actor_final_restored']=len(end)==1 and end[0]['restored'] and end[0]['callbacks']==0 and end[0]['failures']==0
        checks['input_actor_final_samples_accounted']=bool(final) and len(end)==1 and len(final)==end[0]['samples']<=256 and end[0]['calls']>=len(final)
        checks['input_actor_final_read_only_local']=bool(final) and all(e['read_only'] and e['entity']==e['local_entity']!=0 and e['caller']=='0x2894fb' for e in final)
        checks['input_actor_final_finite_vectors']=bool(final) and all(all(len(e[k])==3 and all(math.isfinite(v) for v in e[k]) for k in ('desired_local','world_move','world_look')) for e in final)
        checks['input_actor_final_effective_state']=bool(final) and all(e['actor_state_effective']==((e['actor_state_raw']|e['actor_state_add'])&~e['actor_state_remove'] if e['actor_sequence_flags']&1 else e['actor_state_raw']) for e in final)
        if any('gravity_present' in e for e in final):
            checks['input_actor_gravity_snapshot_valid']=all('gravity_present' in e and type(e['gravity_present'])==bool and type(e['gravity_active'])==bool and len(e['gravity_direction'])==3 and all(math.isfinite(v) for v in e['gravity_direction']) and math.isfinite(e['gravity_value']) and
                (e['gravity_present'] or not e['gravity_active'] and e['gravity_direction']==[0,0,0] and e['gravity_value']==0) for e in final)
    if events('input_sender_observer_enabled'):
        sent=events('input_sender_sample');ended=events('input_sender_observer_removed')
        checks['input_sender_observer_restored']=len(ended)==1 and ended[0]['restored']
        checks['input_sender_samples_accounted']=bool(sent) and len(ended)==1 and len(sent)==ended[0]['samples']<=256 and ended[0]['calls']>=len(sent)
        checks['input_sender_local_and_copy_preserved']=bool(sent) and all(e['read_only'] and e['entity']==e['local_entity']!=0 and e['native_copy_matches'] for e in sent)
        if sent and all('camera_source_bytes' in e and 'camera_owner_bytes' in e for e in sent):
            valid=True;max_delta=[0.,0.,0.];mismatches=0;different_bytes=set()
            for e in sent:
                try:
                    source=bytes.fromhex(e['camera_source_bytes']);owner=bytes.fromhex(e['camera_owner_bytes'])
                    if len(source)!=64 or len(owner)!=64:raise ValueError('camera size')
                    valid &= (source==owner)==e['camera_matches_local_data']
                    valid &= list(struct.unpack_from('<3q',source))==e['camera_q32']
                    valid &= max(abs(a-b) for a,b in zip(struct.unpack_from('<4f',source,24),e['camera_orientation']))<1e-8
                    for camera in (source,owner):
                        q=struct.unpack_from('<4f',camera,24);distance,fov,aspect,near,far=struct.unpack_from('<5f',camera,40)
                        valid &= all(math.isfinite(x) for x in (*q,distance,fov,aspect,near,far)) and abs(sum(x*x for x in q)-1)<2e-5
                        valid &= distance>=0 and 0<fov<math.pi and aspect>0 and 0<near<far
                    mismatches+=source!=owner;different_bytes.update(i for i in range(64) if source[i]!=owner[i])
                    for i,(a,b) in enumerate(zip(struct.unpack_from('<3q',source),struct.unpack_from('<3q',owner))):max_delta[i]=max(max_delta[i],abs(a-b)/2**32)
                except (ValueError,struct.error,TypeError):valid=False
            # The source is an input-tick snapshot; owner camera may be newer.
            # Pipeline equality is verified separately at all actual consumers.
            checks['input_sender_camera_values_and_comparison_valid']=valid
            camera_comparison=dict(mismatches=mismatches,samples=len(sent),changed_byte_offsets=sorted(different_bytes),max_position_difference_m=max_delta)
        else:
            # Historical diagnostics have no raw values to establish timing differences.
            checks['input_sender_camera_matches_local_data']=bool(sent) and all(e['camera_matches_local_data'] for e in sent)
    if events('input_pipeline_enabled'):
        rows=events('input_pipeline_sample');ended=events('input_pipeline_removed')
        senders={e['sample']:e for e in events('input_sender_sample')}
        checks['input_pipeline_no_failure']=not events('input_pipeline_failure')
        checks['input_pipeline_restored']=len(ended)==1 and ended[0]['restored'] and ended[0]['remaining_callbacks']==0 and ended[0]['failures']==0
        checks['input_pipeline_samples_accounted']=bool(rows) and len(ended)==1 and len(rows)==ended[0]['samples'] and len({e['sample'] for e in rows})==len(rows)
        checks['input_pipeline_source_correlated']=bool(rows) and all(e['sample'] in senders and e['entity']==senders[e['sample']]['entity'] and e['tick']==senders[e['sample']]['tick']==e['history_tick'] and e['read_only'] for e in rows)
        checks['input_pipeline_prediction_history_serialization_equal']=bool(rows) and all(e['prediction_seen'] and e['prediction_matches'] and e['history_matches'] and e['serialized_matches'] for e in rows)
    if events('input_shadow_enabled'):
        rows=events('input_shadow_scope');ended=events('input_shadow_removed')
        checks['input_shadow_no_failure']=not events('input_shadow_failure')
        checks['input_shadow_scopes_balanced']=len(ended)==1 and ended[0]['copies']==ended[0]['scopes']>0 and ended[0]['failures']==0
        checks['input_shadow_hooks_restored']=len(ended)==1 and ended[0]['restored'] and ended[0]['remaining_callbacks']==0
        identity=all(e['identity_only'] for e in events('input_shadow_enabled'))
        fixture=any(e.get('controller_fixture',False) for e in events('input_shadow_enabled'))
        name='input_shadow_fixture_fields_only' if fixture else 'input_shadow_identity_only' if identity else 'input_shadow_private_camera_only'
        checks[name]=bool(rows) and all(e['identity_only']==identity and (e.get('controller_fixture') and e.get('permitted_fields_only') if fixture else identity or e['camera_only']) and e['source_unchanged'] and e['shadow_unchanged'] and e['bytes']==1392 for e in rows)
    if events('input_ui_observer_enabled'):
        rows=events('input_ui_sample');senders={e['sample']:e for e in sent}
        checks['input_ui_samples_match_sender']=bool(rows) and len(rows)==len(senders) and all(e['sample'] in senders and e['entity']==senders[e['sample']]['entity'] and e['tick']==senders[e['sample']]['tick'] for e in rows)
        checks['input_ui_read_only']=bool(rows) and all(not e['applied'] and not e['gameplay_state_validated'] for e in rows)
        checks['input_ui_has_readable_known_state']=any(e['known_enum'] for e in rows)
        if rows and 'native_sample' in rows[0]:
            checks['input_ui_candidate_is_explicit_normal_hud']=all(e['normal_hud_candidate']==(e['known_enum'] and e.get('same_input_context',True) and e['hud_screen']==1 and e['auxiliary_state']==0 and not e['native_result'] and int(e['world'],16)!=0) for e in rows)
            if 'same_input_context' in rows[0]:
                checks['input_ui_context_identity_consistent']=all(e['same_input_context']==(int(e['sender_context'],16)!=0 and e['sender_context']==senders[e['sample']]['context'] and e['ui_entity']==e['entity']!=0) for e in rows)
                checks['input_ui_context_identity_observed']=any(e['known_enum'] and e['same_input_context'] for e in rows)
            checks['input_ui_known_enum_consistent']=all(e['known_enum']==(e['native_sample']>0 and e['readable'] and e['stable_read'] and 0<=e['age_ms']<=200 and 0<=e['hud_screen']<50) for e in rows)
            ended=events('input_ui_observer_removed');source=events('input_ui_native_sample')
            checks['input_ui_native_hook_restored']=len(ended)==1 and ended[0]['restored'] and ended[0]['callbacks']==0 and ended[0]['read_failures']==0 and ended[0]['calls']>0
            checks['input_ui_native_result_preserved']=bool(source) and all(e['return_unchanged'] and e['caller']=='0x780575' for e in source)
        else:
            checks['input_ui_candidate_is_explicit_normal_hud']=bool(rows) and all(e['normal_hud_candidate']==(e['readable'] and e['stable_read'] and e['hud_screen']==1 and e['wheel_menu']==0) for e in rows)
            checks['input_ui_known_enum_consistent']=bool(rows) and all(e['known_enum']==(e['readable'] and e['stable_read'] and 0<=e['hud_screen']<50 and 0<=e['wheel_menu']<3) for e in rows)
    picking_metrics={}
    if events('input_picking_enabled'):
        picking_checks,picking_metrics=check_picking(native,pose_expectation,stopped_at=picking_stopped_at);checks.update(picking_checks)
    return checks,{'native_basis_error':error,'quaternion_unit_error':unit_error,'camera_snapshot_comparison':camera_comparison,**picking_metrics,**ranged_metrics}

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args()
    native=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()]
    checks,errors=check_input_direction(native)
    (a.probe/'input-direction-checks.json').write_text(json.dumps(dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},errors=errors,limitation=__doc__),indent=2)+'\n')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print('PASS',sum(checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))

if __name__=='__main__':raise SystemExit(main())
