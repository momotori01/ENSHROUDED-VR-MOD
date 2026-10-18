"""Validate bounded live native game frames; not gameplay/comfort/full-quality certification.

CPU game matrices and runtime logs cover every frame. GPU readback covers the
first/middle/last pair. Image changes can also contain ordinary game animation.
Run after normal game shutdown to check native resource lifetime.
"""
import argparse, collections, json, math
from pathlib import Path
from check_native_calibration import Image, ImageChops, ImageStat
from check_recorded_xr_native import normalize, multiply, rotate
from check_private_pools import check_pools
from check_water_feedback import check_water, check_packet_evidence
from check_root_camera import check_root
from check_culling_history import check_history
from check_culling_gpu_memory import check_gpu_memory
from check_head_visibility import check_head
from check_shared_culling_camera import check_shared_culling
from check_vfx_transparent import check_vfx_transparent
from check_input_direction import check_input_direction
from check_input_pose import check_input_pose
from check_idle_head_camera import check_idle_head
from check_idle_actor_separation import check_idle_actor
from check_walking_fixture import check_walking_fixture


def rows(path):
    return [json.loads(l) for l in path.read_text(encoding='utf-8-sig').splitlines() if l.strip()]


def async_completion_policy(xr):
    """Explicit opt-in queue-ordered release (EVR_XR_ASYNC_COMPLETION=1, single-queue binding family)."""
    policy=[e for e in xr if e['event']=='native_xr_completion_policy']
    return len(policy)==1 and policy[0]['async'] is True and policy[0]['created_queue_count']==1 and policy[0]['queue_family']==policy[0]['binding_queue_family']

def async_completion_chain(xr,submitted,count):
    """Each in-flight pair is proven complete before the next pair reuses the copy resources,
    and the last one by the teardown device idle. Strict (capture) frames still report fence completion."""
    timing={e['frame']:e for e in xr if e['event']=='native_completion_cpu_timing'}
    drained=[e for e in xr if e['event']=='native_xr_async_drained']
    if len(submitted)!=count or not all(e['eyes']==2 and e.get('queue_ordered_release') is True and e['gpu_fence_complete']!=e.get('async_completion') for e in submitted):return False
    if not any(e.get('async_completion') for e in submitted):return False
    # Eye luma frames (EVR_EYE_LUMA_EVERY, 2026-10-06) read the XR images back, so StereoTransfer waits for that copy
    # inside the frame: the next pair then finds nothing pending. That is a stricter completion, not a gap.
    luma={e['frame'] for e in xr if e['event']=='native_eye_luma'}
    for a,b in zip(submitted,submitted[1:]):
        if a.get('async_completion') and not (b['frame'] in timing and (timing[b['frame']].get('previous_pending') is True or
                (a['frame'] in luma and timing[b['frame']].get('previous_pending') is False))):return False
    last=submitted[-1]
    return not last.get('async_completion') or (len(drained)==1 and drained[0]['device_idle_complete'] is True and drained[0]['transfer_pending'] is True and drained[0]['us']>last['us'])

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path)
    p.add_argument('--pose-expectation',choices=('dynamic','fixed','settled','fixed-root-motion'),default='dynamic',help='Use fixed only for deliberately stationary Runtime pose diagnostics')
    p.add_argument('--hand-tracking-loss',action='store_true',help='Explicit runtime-loss fixture: require hidden models only during CSV right-hand tracking loss')
    p.add_argument('--epoch-admission-stop',action='store_true',help='Require independently audited admission-stop; scope picking movement to authorized VR input interval')
    p.add_argument('--terminal-contact-stop',action='store_true',help='Independently audit completed-action terminal bridge before scoping input interval')
    p.add_argument('--joined-contact-stop',action='store_true',help='Require complete native joined contact/input owner evidence before scoping input')
    a=p.parse_args();launch=a.probe.parent
    if sum((a.epoch_admission_stop,a.terminal_contact_stop,a.joined_contact_stop))>1:p.error('Select one shutdown evidence policy')
    # Managed launches publish this only after their held process handle exits.
    # Avoid overwriting a final report with transient destruction failures while
    # Alt-F4 is still draining native Scene/GPU resources. Older archives without
    # the lifetime watcher still use the native destroy events below.
    if (launch/'process-watch.json').exists() and not (launch/'process-exit.json').exists():
        p.error('managed process exit is not recorded yet; complete ordinary shutdown before final lifetime checks')
    native=rows(a.probe/'view-resource-probe.jsonl')
    picking_stop=None
    if a.epoch_admission_stop:
        from check_worldless_input_drain import validated_stop
        picking_stop=validated_stop(a.probe,native)
    if a.terminal_contact_stop:
        from check_terminal_contact import validated_stop
        picking_stop=validated_stop(a.probe,native)
    if a.joined_contact_stop:
        from check_joined_contact_owner import validated_stop
        picking_stop=validated_stop(a.probe,native)
    all_xr=rows(launch/'native-xr/openxr.jsonl');gpu=rows(launch/'vulkan.jsonl')
    # A refused diagnostic can close its XR session and be retried in a fresh
    # DLL generation on the same game device. Logs append; select this probe's
    # actual lifetime instead of combining frames from different sessions.
    scope_start=min(e['us'] for e in native);scope_end=max(e['us'] for e in native)
    xr=[e for e in all_xr if scope_start<=e['us']<=scope_end]
    _,count,scale=(a.probe/'live-xr-config.txt').read_text().split();count=int(count);scale=float(scale)
    def events(data,name):return [e for e in data if e['event']==name]
    submitted=events(xr,'native_game_pair_submitted');pairs=events(native,'calibration_pair_recorded')
    applied=events(native,'recorded_xr_eye_applied');transforms=events(native,'calibration_eye_transform')
    poses={(e['frame'],e['eye']):e for e in events(xr,'runtime_eye_pose')}
    heads={e['frame']:e for e in events(xr,'hmd')}
    anchors={e['recorded_xr_frame']:e for e in events(native,'recorded_xr_anchor')}
    root_mode=int((a.probe/'world-root-camera.txt').read_text().split()[1]) if (a.probe/'world-root-camera.txt').exists() else 0
    sources=events(xr,'native_eye_source');ends=events(xr,'stereo_frame')
    frame_ids=[e['frame'] for e in submitted];serials=[e['serial'] for e in pairs]
    warmups=events(native,'world_warmup_frame_ended')
    warmup_ids={e['frame'] for e in warmups};warmup_serials={e['serial'] for e in warmups}
    all_applied,all_transforms=applied,transforms
    applied=[e for e in applied if e['recorded_xr_frame'] in frame_ids]
    transforms=[e for e in transforms if e['serial'] in serials]
    eye_calls=[e for e in events(native,'calibration_eye_recorded') if e['serial'] in serials]
    game_devices=[e for e in events(gpu,'create_device') if e.get('openxr_extensions_prepared')]
    bindings=events(xr,'borrowed_vulkan')
    binding_family=bindings[0].get('queue_family') if len(bindings)==1 else None
    # Startup probes may create/destroy other Vulkan instances and reuse handles.
    # Match the prepared game device's parent and require its later destruction.
    normal_exit=False
    if len(game_devices)==1:
        creation=game_devices[0]
        device_end=[e for e in events(gpu,'destroy_device') if e['device']==creation['device'] and e['us']>creation['us']]
        instance_end=[e for e in events(gpu,'destroy_instance') if e['instance']==creation['instance'] and e['us']>creation['us']]
        normal_exit=len(device_end)==len(instance_end)==1 and device_end[0]['us']<instance_end[0]['us']
    checks={
        'requested_live_pairs_submitted':len(submitted)==count and len(set(frame_ids))==count,
        'native_pair_count':len(pairs)==count,
        'one_game_serial_per_pair':len(serials)==count and all(y>x for x,y in zip(serials,serials[1:])),
        'balanced_native_eye_calls':collections.Counter(e['eye'] for e in eye_calls)=={0:count,1:count},
        'same_serial_both_eyes':collections.Counter((e['serial'],e['eye']) for e in transforms)==collections.Counter((s,i) for s in serials for i in (0,1)),
        'same_predicted_time_at_submit':len(ends)==count and all(e['frame']==s['frame'] and e['predicted_time']==s['time'] for e,s in zip(ends,submitted)),
        'both_gpu_completions_before_release':(len(submitted)==count and all(e['gpu_fence_complete'] and e['eyes']==2 for e in submitted)) if not async_completion_policy(all_xr) else async_completion_chain(all_xr,submitted,count),
        'live_runtime_eye_data_complete':len(poses)==2*(count+len(warmups)) and set(poses)=={(f,i) for f in set(frame_ids)|warmup_ids for i in (0,1)},
        'native_pose_application_complete':len(applied)==2*count and all(e['live_openxr_session'] and e['projection_calls']==1 for e in applied),
        'source_images_ready_on_binding_family':binding_family is not None and len(sources)==2*count and all(e['layout'] in (1,2,5) and (e['layout']!=5 or e.get('usage',0)&(4|128)) and e['family']==binding_family and e['generation']>0 and e['submission']>0 for e in sources),
        'distinct_source_and_xr_images':all(len({e['image'] for e in sources if e['frame']==f})==2 and len({e['xr_image'] for e in sources if e['frame']==f})==2 for f in frame_ids),
        'only_two_private_views':len(events(native,'native_view_created'))==2,
        'borrowed_game_device':any(e['event']=='borrowed_vulkan' and e['owned'] is False for e in xr),
        'xr_session_closed':len(events(xr,'native_xr_closed'))==1,
        'normal_game_exit':normal_exit,
        'render_hook_restored':any(e['original_bytes_restored'] and e['status']==0 for e in events(native,'view_resource_hook_removed')) or any(e['original_bytes_restored'] for e in events(native,'view_resource_hook_complete')),
        'render_diagnostic_completed_without_error':any(e['result']==0 for e in events(native,'view_resource_hook_complete')) and not events(native,'view_resource_hook_failure'),
        'scene_hook_removed':any(e['status']==0 for e in events(native,'calibration_scene_hook_removed')),
        'game_survived_probe':json.loads((a.probe/'probe-postcheck.json').read_text())['status']=='PASS',
        'no_forced_native_retirement':not events(native,'calibration_gpu_retired'),
    }
    checks['all_native_frames_accounted_for']={e['frame'] for e in events(native,'live_native_frame_begin')}==set(frame_ids)|warmup_ids and not (set(frame_ids)&warmup_ids)
    checks['all_applied_cameras_accounted_for']=all(e['recorded_xr_frame'] in set(frame_ids)|warmup_ids for e in all_applied) and all(e['serial'] in set(serials)|warmup_serials for e in all_transforms)
    # Desktop skip (2026-10-06, default for live world series): on skipped desktop frames the original View is not
    # rendered; eye 0 owns the shared updates and VFX, eye 1 skips/borrows. Skipped frames = submitted serials without
    # an original (eye -1) shared stage; they must form one contiguous tail and need the desktop skip policy.
    skip_policy=(a.probe/'world-desktop-skip.txt').exists()
    original_stage_serials={e['serial'] for e in events(native,'world_shared_render_stage') if e['eye']==-1}
    skipped_serials={s for s in serials if s not in original_stage_serials} if skip_policy else set()
    if skip_policy:
        ordered=sorted(serials);tail=[s for s in ordered if s in skipped_serials]
        checks['desktop_skip_contiguous_tail']=bool(tail) and tail==[s for s in ordered if s>=tail[0]]
    if (a.probe/'world-shared-update-policy.txt').exists():
        stages=events(native,'world_shared_render_stage')
        originals={(e['serial'],e['function'],e['object']):e for e in stages if e['eye']==-1}
        owners={(e['serial'],e['function'],e['object']):e for e in stages if e['eye']==0}
        eye_stages=[e for e in stages if e['eye']>=0]
        def shared_ok(e):
            k=(e['serial'],e['function'],e['object'])
            if k in originals:
                return e['skipped']==e['calls']==1 and not e['proof_rejected'] and originals[k]['skipped']==0 and originals[k]['us']<e['us']
            if not skip_policy or e['serial'] in original_stage_serials or e['proof_rejected'] or e['calls']!=1:return False
            if e['eye']==0:return e['skipped']==0
            return e['skipped']==1 and k in owners and owners[k]['skipped']==0 and owners[k]['us']<e['us']
        checks['world_shared_updates_original_proven_before_each_eye']=bool(eye_stages) and all(shared_ok(e) for e in eye_stages)
        checks['world_shared_updates_both_eyes_each_submitted_serial']=collections.Counter(
            (e['serial'],e['function'],e['eye']) for e in eye_stages if e['serial'] in serials)==collections.Counter(
            (s,fn,i) for s in serials for fn in ('0x1d19f0','0x1d5260') for i in (0,1))
        removed=events(native,'world_shared_stage_hook_removed')
        checks['world_shared_update_hooks_removed']=len(removed)==2 and all(e['status']==0 for e in removed)
    if events(native,'world_shared_free_hook_enabled'):
        removed=events(native,'world_shared_free_hook_removed')
        checks['shared_array_free_observer_removed']=len(removed)==1 and removed[0]['status']==0 and removed[0]['callbacks']==0
    if (a.probe/'world-share-vfx-lights.txt').exists():
        borrowed=events(native,'shared_point_lights_scope')
        selected=[e for e in borrowed if e['serial'] in serials]
        # Skipped desktop frames: eye 0 owns the VFX lights (not borrowed, its own counter); eye 1 borrows as before.
        checks['point_lights_borrowed_once_per_submitted_eye']=bool(selected) and collections.Counter(
            (e['serial'],e['eye']) for e in selected)==collections.Counter((s,i) for s in serials for i in (0,1)) and all(
                e['calls']==1 and (not e['borrowed'] and e['owned_counter']==e['source_counter'] if (e['serial'] in skipped_serials and e['eye']==0) else
                e['borrowed'] and 0<e['capacity']<=65536 and e['owned_counter']!=e['source_counter']) for e in selected)
        checks['point_lights_all_scopes_restored']=bool(borrowed) and all(e['view_restored'] and e['parameters_restored'] and e['source_unchanged'] and not e['failed'] for e in borrowed)
        checks['point_lights_no_guard_failures']=not events(native,'shared_point_lights_failure')
        removed=events(native,'shared_point_lights_hook_removed')
        checks['point_lights_hook_removed']=len(removed)==1 and removed[0]['status']==0
    if (a.probe/'world-early-xr.txt').exists():
        early=events(native,'world_early_xr_frame');by_frame={e['frame']:e for e in early}
        originals=events(native,'world_shared_render_stage')
        checks['early_xr_each_submitted_frame']=len(by_frame)==len(early) and bool(submitted) and all(f in by_frame for f in frame_ids)
        # Skipped desktop frames: the early XR frame precedes eye 0's (owner's) two shared updates instead.
        checks['early_xr_precedes_original_shared_updates']=bool(early) and all(
            len([s for s in originals if s['serial']==e['serial'] and s['eye']==(0 if e['serial'] in skipped_serials else -1) and s['us']>e['us']])==2 for e in early)
    if events(native,'terrain_readiness_hook_enabled'):
        removed=events(native,'terrain_readiness_hook_removed')
        checks['terrain_readiness_observer_removed']=len(removed)==1 and removed[0]['status']==0
    cpu_waits=events(native,'eye_cpu_tasks_completion')
    # 1.0 (2026-10-16): the scene's brick collectors are left to the game (view_resource_probe.cpp finishEyeCpuTasks);
    # only the selected View's two remain, and a busy one after the bounded wait no longer ends the series.
    if any('scene_collectors_stepped' in e for e in cpu_waits):
        checks['world_cpu_wait_leaves_scene_bricks']=all(e.get('scene_collectors_stepped') is False for e in cpu_waits)
    if events(native,'download_feedback_hook_enabled'):
        removed=events(native,'download_feedback_hook_removed')
        checks['download_feedback_observer_removed']=len(removed)==1 and removed[0]['status']==0
        feedback=events(native,'download_feedback_pool')
        checks['download_feedback_snapshots_complete']=bool(feedback) and all(not e['overflow'] and all(c['before']['valid'] and c['after']['valid'] for c in e['calls']) for e in feedback)
        checks['native_feedback_copies_succeeded']=bool(feedback) and all(c['status']==0 for e in feedback for c in e['calls'])
    if (a.probe/'world-shared-update-policy.txt').exists() and any('sdf_results_preserved' in e for e in cpu_waits):
        checks['world_cpu_wait_preserves_sdf_results']=bool(cpu_waits) and all(e.get('sdf_results_preserved') and len(e.get('collector_statuses',[]))==(2 if 'scene_collectors_stepped' in e else 5) for e in cpu_waits)
    if warmups:
        diagnostics=events(native,'world_native_single_pair_diagnostic')+events(native,'world_native_series_diagnostic')
        limit=diagnostics[0].get('max_warmup_frames',120) if len(diagnostics)==1 else 0
        checks['explicit_bounded_world_warmup']=len(diagnostics)==1 and limit in (120,600) and len(warmups)==len(warmup_ids)==len(warmup_serials)<=limit
        checks['warmup_both_eyes_attempted_same_frame']=collections.Counter((e['serial'],e['eye']) for e in events(native,'calibration_eye_begin') if e['serial'] in warmup_serials)==collections.Counter((s,i) for s in warmup_serials for i in (0,1))
        checks['warmup_ended_with_no_layer']={e['frame'] for e in events(xr,'empty_frame') if e.get('reason')=='world_view_resources_pending'}==warmup_ids and all(e['layers']==0 for e in warmups)
    lighting=events(native,'world_lighting_readiness')
    if lighting:
        ready=[e for e in lighting if e['serial'] in serials]
        checks['lighting_ready_before_each_submitted_eye']=len(ready)==2*count and all(e['before'] and e['after'] for e in ready) and collections.Counter((e['serial'],e['eye']) for e in ready)==collections.Counter((s,i) for s in serials for i in (0,1))
    errors={'position_m':0.,'rotation_matrix':0.,'projection_matrix':0.}
    checks['runtime_fov_and_pose_match_game']=checks['live_runtime_eye_data_complete'] and checks['native_pose_application_complete']
    if checks['runtime_fov_and_pose_match_game']:
        first=heads[events(native,'live_native_frame_begin')[0]['frame']];origin_q=[-x for x in normalize(first['orientation'])[:3]]+[normalize(first['orientation'])[3]]
        origin_p=rotate(origin_q,[-x for x in first['position']])
        if root_mode==2:
            forward=rotate(normalize(first['orientation']),[0,0,-1]);yaw=math.atan2(-forward[0],-forward[2]) if forward[0]*forward[0]+forward[2]*forward[2]>1e-10 else 0
            origin_q=[0,math.sin(-yaw/2),0,math.cos(-yaw/2)];origin_p=rotate(origin_q,[-x for x in first['position']])
        actual={(e['serial'],e['eye']):e for e in transforms}
        for e in applied:
            f=e['recorded_xr_frame'];eye=e['eye'];v=poses[(f,eye)];anchor=anchors[f];n=actual[(e['serial'],eye)]
            checks['runtime_fov_and_pose_match_game'] &= e['recorded_predicted_time']==v['time'] and max(abs(x-y) for x,y in zip(e['fov'],v['fov']))<1e-6
            local=[x+y for x,y in zip(origin_p,rotate(origin_q,v['position']))];local[2]=-local[2]
            offset=rotate(anchor['orientation'],[x+y for x,y in zip(local,anchor.get('camera_offset',[0,0,0]))])
            errors['position_m']=max(errors['position_m'],max(abs((x-y)/4294967296-z) for x,y,z in zip(n['camera_q32'],anchor['camera_q32'],offset)))
            q=normalize(multiply(origin_q,normalize(v['orientation'])));q=[-q[0],-q[1],q[2],q[3]]
            q=normalize(multiply(normalize(anchor['orientation']),q));qi=[-q[0],-q[1],-q[2],q[3]]
            expected=[rotate(qi,axis) for axis in ([1,0,0],[0,1,0],[0,0,1])]
            errors['rotation_matrix']=max(errors['rotation_matrix'],max(abs(expected[r][c]-n['view_matrix'][r*4+c]) for r in range(3) for c in range(3)))
            left,right,up,down=map(math.tan,v['fov']);near=anchor['near'];far=anchor['far'];m=[0]*16
            m[0]=2/(right-left);m[5]=2/(up-down);m[8]=-(right+left)/(right-left);m[9]=-(up+down)/(up-down)
            m[10]=-near/(far-near);m[11]=1;m[14]=near*far/(far-near)
            errors['projection_matrix']=max(errors['projection_matrix'],max(abs(x-y) for x,y in zip(m,n['projection_matrix'])))
        checks['runtime_fov_and_pose_match_game'] &= all(v<1e-5 for v in errors.values())
    if root_mode:checks.update(check_root(native,root_mode))
    if (a.probe/'world-shared-culling-camera.txt').exists():
        shared_checks,shared_errors=check_shared_culling(native,skipped_serials);checks.update(shared_checks);errors.update(shared_errors)
    if events(native,'vfx_transparent_observer_enabled'):
        vfx_checks,vfx_errors=check_vfx_transparent(native,skipped_serials);checks.update(vfx_checks);errors.update(vfx_errors)
    if events(native,'input_pose_published'):
        pose_checks,pose_errors=check_input_pose(native,xr);checks.update(pose_checks);errors.update(pose_errors)
    if events(native,'input_head_idle_sample'):
        idle_checks,idle_errors=check_idle_head(native,xr);checks.update(idle_checks);errors.update(idle_errors)
        if events(native,'input_actor_final_sample'):
            actor_checks,actor_errors=check_idle_actor(native,pose_expectation=a.pose_expectation);checks.update(actor_checks);errors.update(actor_errors)
    if (a.probe/'world-input-head-walking.txt').exists():
        walking_checks,walking_errors=check_idle_head(native,xr,'walking');checks.update(walking_checks);errors.update(walking_errors)
        controls=events(native,'input_head_walking_sample')
        if controls and not (a.probe/'world-input-walk-fixture.txt').exists() and all(e['native_stick']==[0,0] and e['native_digital']==0 for e in controls) and events(native,'input_actor_final_sample'):
            actor_checks,actor_errors=check_idle_actor(native,'walking',a.pose_expectation);checks.update(actor_checks);errors.update(actor_errors)
    if (a.probe/'world-input-walk-fixture.txt').exists():
        fixture_checks,fixture_errors=check_walking_fixture(native);checks.update(fixture_checks);errors.update(fixture_errors)
    if (a.probe/'world-input-observer.txt').exists():
        input_checks,input_errors=check_input_direction(native,a.pose_expectation,picking_stopped_at=picking_stop);checks.update(input_checks);errors.update(input_errors)
    if (a.probe/'world-equipment-observer.txt').exists():
        equipment=events(native,'local_equipment_observation');removed=events(native,'equipment_observer_removed')
        checks['equipment_observer_complete']=len(equipment)==20 and len({e['entity'] for e in equipment})==1 and all(len(e['models'])==24 for e in equipment)
        checks['equipment_observer_restored']=len(removed)==1 and removed[0]['original_bytes_restored'] and removed[0]['remaining_callbacks']==removed[0]['read_failures']==0
        checks['equipment_observer_no_read_failure']=not events(native,'equipment_observer_read_failure')
        ownership=events(native,'world_native_series_diagnostic') or events(native,'world_native_single_pair_diagnostic')
        checks['equipment_scene_and_table_ownership']=bool(equipment) and len(ownership)==1 and all(
            e['scene']==ownership[0]['proven_scene'] and e['owner_view']==ownership[0]['proven_original_view'] and
            all((m['handle']&0xffffff)<min(e['object_count'],e['generation_count']) for m in e['models'] if m['handle'])
            for e in equipment)
        checks['equipment_head_and_body_identified']=bool(equipment) and all(e['models'][0]['handle'] and any(m['handle'] for m in e['models'][6:15]) for e in equipment)
        checks['equipment_model_evidence_saved']=bool(equipment) and all((a.probe/'equipment-models'/f'{m["part"]}-{m["name"]}.bin').is_file() and (a.probe/'equipment-models'/f'{m["part"]}-{m["name"]}.bin').stat().st_size==432 for m in equipment[0]['models'] if m['handle'])
    swaps=events(xr,'swapchain');res=events(native,'native_view_resolution_initialized')
    checks['runtime_recommended_extents_scaled']=len(swaps)==len(res)==2 and all(
        r['width']==s['width']==math.ceil(s['recommended_width']*scale) and r['height']==s['height']==math.ceil(s['recommended_height']*scale)
        for r,s in zip(res,swaps))
    disposal=events(native,'calibration_scene_disposal_complete')
    checks['native_scene_cleanup']=len(disposal)==1 and disposal[0]['views_destroyed']==2 and disposal[0]['owned_groups_cleared']
    launch_config=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
    if launch_config.get('vulkan_validation','None')!='None':
        validation=launch/'validation.log'
        checks['vulkan_validation_no_messages']=validation.is_file() and not validation.read_text(encoding='utf-8-sig').strip()
    registered=events(gpu,'native_eye_image_registered')
    for eye in (0,1):
        entries=[e for e in registered if e['eye']==eye]
        matches=[e for e in events(gpu,'render_image_destroyed') if entries and e['image']==entries[0]['image'] and e['generation']==entries[0]['generation']]
        device_end=events(gpu,'destroy_device')
        checks[f'eye{eye}_image_lifetime']=len(entries)==len(matches)==len(disposal)==len(device_end)==1 and disposal[0]['us']<matches[0]['us']<device_end[0]['us']
    images={};stats={}
    capture_frames=[frame_ids[i-1] for i in sorted({1,count//2,count}-{0}) if i<=len(frame_ids)]
    checks['capture_frame_selection_complete']=len(capture_frames)==len({1,count//2,count}-{0})
    for f in capture_frames:
        paths=[launch/f'native-xr/frame-{f}-eye-{eye}.ppm' for eye in (0,1)]
        checks[f'frame{f}_two_captures']=all(path.exists() for path in paths)
        if not checks[f'frame{f}_two_captures']:continue
        ims=[Image.open(path).convert('RGB') for path in paths];images[f]=ims
        stats[f]={'mean':[ImageStat.Stat(im).mean for im in ims],'variance':[ImageStat.Stat(im).var for im in ims],
                  'difference':ImageStat.Stat(ImageChops.difference(*ims)).mean}
        checks[f'frame{f}_nonblack_nonuniform']=all(sum(m)>9 and sum(v)>12 for m,v in zip(stats[f]['mean'],stats[f]['variance']))
        checks[f'frame{f}_eyes_differ']=sum(stats[f]['difference'])>0
        checks[f'frame{f}_capture_extent']=len(swaps)==2 and all(im.size==(s['width'],s['height']) for im,s in zip(ims,swaps))
    if count>1:
        pose_count=len({tuple(h['position']+h['orientation']) for h in heads.values()})
        camera_counts=[len({tuple(e['camera_q32']+e['view_matrix']) for e in transforms if e['eye']==eye}) for eye in (0,1)]
        if a.pose_expectation=='dynamic':
            checks['runtime_pose_changes']=pose_count>1
            checks['game_camera_changes']=all(n>1 for n in camera_counts)
        elif a.pose_expectation=='fixed-root-motion':
            # Fixed physical head, legitimate native action/root locomotion.
            # Require actual root movement and constant camera offset per eye.
            by_serial={e['serial']:e['recorded_xr_frame'] for e in applied}
            relative={eye:[] for eye in (0,1)};stable=True
            for e in transforms:
                anchor=anchors[by_serial[e['serial']]]
                relative[e['eye']].append(tuple((q-r)/4294967296 for q,r in zip(e['camera_q32'],anchor['camera_q32'])))
                stable &= anchor['anchor_kind']=='stable_root_first_person_diagnostic'
            checks['runtime_pose_fixed_with_native_root_motion']=pose_count==1 and len({tuple(v['camera_q32']) for v in anchors.values()})>1
            checks['game_camera_follows_root_without_animation_rotation']=stable and all(len(v)==count and max(max(x[i] for x in v)-min(x[i] for x in v) for i in range(3))<2e-6 for v in relative.values()) and all(len({tuple(e['view_matrix'][i] for i in (0,1,2,4,5,6,8,9,10)) for e in transforms if e['eye']==eye})==1 for eye in (0,1))
        elif a.pose_expectation=='settled':
            # Explicit calibration pose in frame one, then a held pose for all
            # subsequent warmup/submitted frames. Never weaken the fixed case.
            ordered=sorted(heads)
            held={tuple(heads[f]['position']+heads[f]['orientation']) for f in ordered[1:]}
            submitted_poses={tuple(heads[f]['position']+heads[f]['orientation']) for f in frame_ids}
            checks['runtime_pose_settled_after_calibration']=len(ordered)>1 and len(held)==1 and held==submitted_poses
            checks['game_camera_fixed_after_warmup']=all(n==1 for n in camera_counts)
        else:
            checks['runtime_pose_fixed']=pose_count==1
            checks['game_camera_fixed']=all(n==1 for n in camera_counts)
        checks['captured_frame_updates']=bool(frame_ids) and frame_ids[0] in images and frame_ids[-1] in images and all(sum(ImageStat.Stat(ImageChops.difference(images[frame_ids[0]][eye],images[frame_ids[-1]][eye])).mean)>0 for eye in (0,1))
    if (launch/'mock.jsonl').exists():
        all_mock=rows(launch/'mock.jsonl');mock=[e for e in all_mock if scope_start<=e['us']<=scope_end]
        hist=collections.Counter(e['event'] for e in mock)
        ui_ends=events(xr,'ui_quad_submitted')
        checks['mock_balanced_acquire_wait_release']=hist['acquire']==hist['wait_image']==hist['release']==2*(count+len(warmups))+len(ui_ends)
        ended=events(mock,'end_frame');empty=events(xr,'empty_frame')
        waits=events(mock,'wait_frame');completed={e['frame']:e for e in events(xr,'empty_frame_completed')}
        app_results=sorted(ends+[completed.get(e['frame'],e) for e in empty],key=lambda e:e['us'])
        # Mock's counter belongs to its shared Runtime instance; the client
        # restarts its diagnostic counter for each new session. Neither counter
        # is an OpenXR frame identifier. Match the ordered wait/end/result calls.
        sequence=(len(ended)==len(waits)==len(app_results) and
                  [e['frame'] for e in ended]==[e['frame'] for e in waits] and
                  len({e['frame'] for e in ended})==len(ended) and
                  all(w['us']<m['us']<=a['us'] for w,m,a in zip(waits,ended,app_results)))
        checks['mock_wait_end_result_sequence']=sequence
        checks['mock_projection_layer_every_rendered_frame']=sequence and {a['frame'] for m,a in zip(ended,app_results) if m['layers'] in (1,2)}==set(frame_ids)
        checks['mock_ui_layers_match_client']=sequence and hist['ui_quad']==len(ui_ends) and {a['frame'] for m,a in zip(ended,app_results) if m['layers']==2}=={e['frame'] for e in ui_ends}
        checks['mock_empty_frames_have_no_projection']=sequence and {a['frame'] for m,a in zip(ended,app_results) if m['layers']==0}=={e['frame'] for e in empty} and len(ended)==count+len(empty)
        # The borrowed preflight instance belongs to the entire game device,
        # so its destruction can occur after the native Scene probe's end.
        checks['mock_runtime_cleanup']=hist['session_destroyed']==1 and sum(e['event']=='instance_destroyed' for e in all_mock)==1
    feature_guards=events(native,'world_feature_guard_observation')
    if feature_guards and feature_guards[0]['features']&0x8000:
        fsr=events(native,'private_fsr3_context')
        checks['fsr3_private_temporal_resources']=len(fsr)==2 and {e['eye'] for e in fsr}=={0,1} and len({e['object'] for e in fsr})==2 and all(e['object']!=e['original'] and e['program_id'] and e['private_image_handles']>0 for e in fsr)
    if events(native,'private_native_memory_pool_created'):checks.update(check_pools(native,transient=(a.probe/'world-eye-private-transient.txt').exists()))
    packet_hashes={}
    if (a.probe/'world-culling-history.txt').exists():
        checks.update(check_history(native,True));checks.update(check_gpu_memory(native,gpu,True))
    loss_frames=set()
    if a.hand_tracking_loss:
        if not (a.probe/'world-motion-tracking-loss.txt').exists():raise ValueError('hand loss checker needs explicit probe policy')
        seq=[[float(v) for v in line.split(',')] for line in (launch/'mock-hands.csv').read_text().splitlines() if line and not line.startswith('#')]
        loss_frames={int(v[0]) for v in seq if v[26]==0}
        flags=[r for r in native if r['event']=='motion_tracking_sample']
        checks['hand_loss_CSV_and_actual_runtime_flags']=bool(loss_frames) and len(flags)==3 and [r['tracked'] for r in flags]==[True,False,True] and flags[1]['frame']==min(loss_frames) and flags[2]['frame']==max(loss_frames)+1 and flags[1]['grip_flags']==0 and flags[0]['grip_flags']==flags[2]['grip_flags']==15
    if (a.probe/'world-head-visibility.txt').exists():checks.update(check_head(native,gpu,loss_frames))
    if (a.probe/'world-water-feedback.txt').exists():
        checks.update(check_water(native,True,skipped_serials))
        packet_checks,packet_hashes=check_packet_evidence(a.probe,native);checks.update(packet_checks)
    report={'checks':{k:'PASS' if v else 'FAIL' for k,v in checks.items()},'max_errors':errors,'image_stats':stats,'pose_expectation':a.pose_expectation,'water_packet_sha256':packet_hashes,'probe_time_scope_us':[scope_start,scope_end],'other_session_log_rows':len(all_xr)-len(xr),'limits':__doc__}
    suffix=('-'+a.pose_expectation if a.pose_expectation!='dynamic' else '')+('-epoch-stop' if a.epoch_admission_stop else '')+('-terminal-stop' if a.terminal_contact_stop else '')
    report['epoch_admission_stop']=a.epoch_admission_stop
    report['terminal_contact_stop']=a.terminal_contact_stop
    (a.probe/('live-native-checks'+suffix+'.json')).write_text(json.dumps(report,indent=2))
    for k,v in report['checks'].items():print(v,k)
    print(errors);print('PASS',sum(checks.values()),'FAIL',len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
