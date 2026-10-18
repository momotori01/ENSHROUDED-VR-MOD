"""Check bounded idle/walking HMD camera routing; report synthesized axes separately."""
import math,struct
from check_recorded_xr_native import normalize,multiply,rotate


def check_idle_head(native,xr,mode='idle'):
    if mode not in ('idle','walking'):raise ValueError('unknown camera input policy')
    walking=mode=='walking'
    melee=any(e['event']=='melee_head_policy_enabled' and e.get('camera_only') and e.get('native_digital_mask')==15 and e.get('fresh_melee_appearance_required') for e in native)
    melee_row=lambda e:walking and melee and e.get('melee_head_evidence') is True
    axes=lambda e:e.get('effective_stick',e['native_stick'])
    def events(rows,name):return [e for e in rows if e['event']==name]
    samples=events(native,f'input_head_{mode}_sample');applied=[e for e in samples if e['applied']]
    removed=events(native,f'input_head_{mode}_removed');published={e['frame']:e for e in events(native,'input_pose_published')}
    by_tick={e['tick']:e for e in applied};senders=events(native,'input_sender_sample')
    observed=[e for e in senders if e['tick'] in by_tick]
    checks={
        'idle_head_applied_and_all_ticks_logged':bool(applied) and len(removed)==1 and removed[0]['applied']==len(applied) and removed[0]['attempted']>=len(samples) and removed[0]['callbacks']==0,
        'idle_head_private_camera_only':bool(samples) and all(e.get('walking_only_diagnostic',False) if walking else e['idle_only_diagnostic'] for e in samples) and all(e['only_camera_bytes_changed'] and (e['applied'] or e['native_preserved']) for e in samples),
        'idle_head_ui_and_state_guard':bool(applied) and all(e['normal_ui_bound'] and e['player_state_current'] and 0<=e['ui_age_ms']<=200 and 0<=e['state_age_ms']<=200 and e['player_flags']&1 and not e['player_flags']&((1<<1)|(1<<2)|(1<<12)) for e in applied),
        'idle_head_no_buttons_movement_or_special_mode':bool(applied) and all(e['native_digital']==0 and e['native_stick']==[0,0] and e['client_mask']==0 and e['building_camera']==0 and not e['free_camera'] and e['native_input_mode'] in (0,1) for e in applied),
        'idle_head_fresh_runtime_generation':bool(applied) and all(e['frame'] in published and e['entity']==published[e['frame']]['entity'] and e['time']==published[e['frame']]['time'] and e['session_generation']==published[e['frame']]['session_generation'] and e['origin_epoch']==published[e['frame']]['origin_epoch'] and e['head_flags']==published[e['frame']]['head_flags'] and e['head_flags']&15==15 and 0<=e['head_age_ms']<=200 and 0<=e['root_age_ms']<=200 for e in applied),
    }
    if walking:
        fixture=any(e.get('controller_fixture_requested') for e in samples)
        if fixture:
            del checks['idle_head_private_camera_only']
            checks['walking_head_fixture_fields_only']=bool(samples) and all(e.get('controller_fixture_requested') and
                e.get('only_fixture_and_camera_bytes_changed') and (e['applied'] or e['native_preserved']) for e in samples)
        del checks['idle_head_no_buttons_movement_or_special_mode']
        checks['walking_head_analog_gamepad_or_idle_keyboard']=bool(applied) and all((e['native_digital']&~15==0 if melee_row(e) else e['native_digital']==0) and len(e['native_stick'])==2 and all(math.isfinite(v) and abs(v)<=1 for v in e['native_stick']) and
            (e['native_input_mode']==1 or e['native_input_mode']==0 and e['native_stick']==[0,0]) and e['client_mask']==0 and e['building_camera']==0 and not e['free_camera'] for e in applied)
        allowed=sum(1<<b for b in (0,6,8,13,14,16,17,18,19,20,21,22,23,24,40,41,42,46,47,49,50,52,54,55))
        checks['walking_head_grounded_state_and_gravity']=bool(applied) and all(e['actor_state_current'] and e['actor_state_effective']&((1<<0)|(1<<50))==((1<<0)|(1<<50)) and not e['actor_state_effective']&~(allowed|((1<<5) if melee_row(e) else 0)) and e['gravity_present'] and e['gravity_active'] and len(e['gravity_direction'])==3 and sum((v-w)**2 for v,w in zip(e['gravity_direction'],[0,-1,0]))<=1e-10 for e in applied)
        checks['walking_head_yaw_only_move_proposal']=bool(applied) and all(len(e['proposed_world_move'])==3 and max(abs(x-y) for x,y in zip(e['proposed_world_move'],[
            axes(e)[0]*math.cos(e['yaw'])+axes(e)[1]*math.sin(e['yaw']),0,
            -axes(e)[0]*math.sin(e['yaw'])+axes(e)[1]*math.cos(e['yaw'])]))<2e-6 for e in applied)
    camera_error=0.
    for e in observed:
        a=by_tick[e['tick']];raw=bytes.fromhex(e['camera_source_bytes'])
        if e['entity']!=a['entity'] or e['camera_q32']!=a['camera_q32'] or struct.unpack_from('<f',raw,40)[0]!=0:camera_error=math.inf
        camera_error=max(camera_error,max(abs(x-y) for x,y in zip(e['camera_orientation'],a['camera_orientation'])))
    checks['idle_head_reaches_native_input_source']=bool(observed) and camera_error<1e-7
    movement=events(native,'input_direction_sample');linked=[e for e in movement if e.get('tick') in by_tick]
    movement_error=0.;simulation_tick_differences=[];consumer_age_us=[];idle_native_camera_ticks=[]
    for e in linked:
        # ECS simulation tick is not the selected PlayerInput's client tick.
        # First prove the exact tick and full camera value. A queued native
        # simulation can consume it later than the publication freshness gate;
        # that delay is reported separately, not a tracking freshness proof.
        # If simulation selected another tick, require a recent earlier value.
        # Never accept a future publication or position-only correspondence.
        def matches(a):
            return a['entity']==e['entity'] and a['camera_q32']==e['camera_q32'] and max(abs(x-y) for x,y in zip(e['camera_orientation'],a['camera_orientation']))<1e-7
        same=by_tick[e['tick']]
        candidates=([same] if same['us']<=e['us'] and matches(same) else
            [a for a in applied if 0<=e['us']-a['us']<=200000 and matches(a)])
        if not candidates:
            # 2026-10-06: every mismatch seen (9 runs, one tick each) had a zero stick and zero desired movement, so the
            # camera read by that tick changed nothing; moving ticks always matched (game151616 walking fixture).
            # Zero-input ticks are reported, moving ticks must match.
            if walking and not any(abs(v)>0 for v in e.get('movement_stick',[1]))and not any(abs(v)>0 for v in e.get('desired_local',[1])):
                idle_native_camera_ticks.append(e['tick']);continue
            movement_error=math.inf;continue
        a=max(candidates,key=lambda v:v['us'])
        movement_error=max(movement_error,max(abs(x-y) for x,y in zip(e['camera_orientation'],a['camera_orientation'])))
        consumer_age_us.append(e['us']-a['us'])
        if e['tick']!=a['tick']:simulation_tick_differences.append(dict(simulation_tick=e['tick'],input_tick=a['tick'],age_us=e['us']-a['us']))
    checks['idle_head_reaches_native_movement_camera']=bool(linked) and movement_error<1e-7
    # Independent Runtime pose + stable-root oracle; include all applied ticks,
    # not just sampled sender records. Physical roll is omitted only from input.
    heads={e['frame']:e for e in events(xr,'hmd')};anchors={e['recorded_xr_frame']:e for e in events(native,'recorded_xr_anchor')}
    position_error=aim_error=yaw_error=0.
    if published:
        first=heads[next(iter(published))];f=rotate(normalize(first['orientation']),[0,0,-1])
        y=math.atan2(-f[0],-f[2]) if f[0]*f[0]+f[2]*f[2]>1e-10 else 0
        oq=[0,math.sin(-y/2),0,math.cos(-y/2)];op=rotate(oq,[-v for v in first['position']])
        for e in applied:
            h=heads[e['frame']];a=anchors[e['frame']]
            local=[x+y for x,y in zip(op,rotate(oq,h['position']))];local[2]=-local[2]
            offset=rotate(a['orientation'],[x+y for x,y in zip(local,a['camera_offset'])])
            position_error=max(position_error,max(abs((x-y)/2**32-z) for x,y,z in zip(e['camera_q32'],a['camera_q32'],offset)))
            q=normalize(multiply(oq,normalize(h['orientation'])));q=[-q[0],-q[1],q[2],q[3]]
            f=rotate(normalize(multiply(normalize(a['orientation']),q)),[0,0,1])
            actual=rotate(normalize(e['camera_orientation']),[0,0,1])
            aim_error=max(aim_error,max(abs(x-y) for x,y in zip(actual,f)),max(abs(x-y) for x,y in zip(e['aim_forward'],f)))
            right=rotate(normalize(e['camera_orientation']),[1,0,0])
            yaw_error=max(yaw_error,abs(right[1]),abs(right[0]-math.cos(e['yaw'])),abs(right[2]+math.sin(e['yaw'])))
    checks['idle_head_actual_camera_matches_runtime']=bool(applied) and max(position_error,aim_error,yaw_error)<1e-5
    errors=dict(idle_head_applied=len(applied),idle_head_sender_matches=len(observed),idle_head_movement_matches=len(linked),
        idle_head_source_error=camera_error,idle_head_movement_error=movement_error,idle_head_position_error=position_error,
        idle_head_aim_error=aim_error,idle_head_roll_free_error=yaw_error,idle_head_simulation_tick_differences=simulation_tick_differences,idle_head_zero_input_native_camera_ticks=idle_native_camera_ticks,
        idle_head_max_consumer_delay_us=max(consumer_age_us,default=None))
    if walking:
        checks={k.replace('idle_head_','walking_head_'):v for k,v in checks.items()}
        errors={k.replace('idle_head_','walking_head_'):v for k,v in errors.items()}
        errors['walking_nonzero_stick_applied']=sum(any(v!=0 for v in e['native_stick']) for e in applied)
        errors['walking_native_gamepad_mode_applied']=sum(e['native_input_mode']==1 and not e.get('synthetic_controls',False) for e in applied)
        errors['walking_fixture_nonzero_applied']=sum(e.get('synthetic_controls',False) and any(v!=0 for v in axes(e)) for e in applied)
    return checks,errors
