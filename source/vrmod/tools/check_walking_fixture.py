"""Check explicitly synthesized walking input; never claims real Xbox coverage."""
import collections,math


def check_walking_fixture(native):
    rows=[e for e in native if e['event']=='input_head_walking_sample']
    synthetic=[e for e in rows if e.get('synthetic_controls')]
    active=[e for e in synthetic if any(e['effective_stick'])]
    phases={e['fixture_phase'] for e in active};ordered=True;prior=-1;completed=False
    schedule=True
    for e in rows:
        if not e.get('controller_fixture_requested'):schedule=False;continue
        if e['fixture_state'] in (2,3):completed=True
        if not e['synthetic_controls']:continue
        settle=e.get('fixture_settle_ms',1000);phase_ms=e.get('fixture_phase_ms',1100)
        pulse=e.get('fixture_pulse_ms',500);run=e.get('fixture_run_ms',6400)
        schedule &= (settle,phase_ms,pulse,run) in ((1000,1100,500,6400),(1000,850,250,5400))
        ms=e['fixture_elapsed_ms'];schedule &= e['applied'] and e['fixture_state']==1 and 0<=ms<run
        ordered &= ms>=prior and not completed;prior=ms
        phase=0;wanted=[0,0]
        if settle<=ms<settle+phase_ms*4:
            phase=(ms-settle)//phase_ms+1
            if (ms-settle)%phase_ms<pulse:wanted=([0,.25],[0,-.25],[.25,0],[-.25,0])[phase-1]
        schedule &= e['fixture_phase']==phase and e['effective_input_mode']==1 and e['effective_stick']==wanted
    key=lambda e:(e['entity'],*e['camera_q32'],*e['camera_orientation'])
    by_camera=collections.defaultdict(list)
    for e in active:by_camera[key(e)].append(e)
    matched=[]
    for e in native:
        if e['event']!='input_actor_final_sample':continue
        past=[a for a in by_camera[key(e)] if a['us']<=e['us']]
        if past:matched.append((e,max(past,key=lambda a:a['us'])))
    errors=[]
    for e,a in matched:
        sx,sz=a['effective_stick'];yaw=a['yaw']
        expected=[sx*math.cos(yaw)+sz*math.sin(yaw),0,-sx*math.sin(yaw)+sz*math.cos(yaw)]
        errors.append(max(abs(x-y) for x,y in zip(e['world_move'],expected)))
    roots=[e for e in native if e['event']=='root_camera_sample']
    positions=[e['stable_q32'] for e in roots]
    displacement=max((math.sqrt(sum(((p[i]-positions[0][i])/2**32)**2 for i in range(3))) for p in positions),default=0)
    tail=roots[-32:];tail_start=tail[0]['us'] if tail else 0
    settled_inputs=[e for e in rows if e['us']>=tail_start and e['applied']]
    checks=dict(
        walking_fixture_explicit_native_controls_idle=bool(synthetic) and all(e['native_stick']==[0,0] and e['native_digital']==0 and e['only_fixture_and_camera_bytes_changed'] for e in synthetic),
        walking_fixture_schedule_and_one_shot=bool(synthetic) and schedule and ordered,
        walking_fixture_four_pulses_completed=phases=={1,2,3,4} and any(e['fixture_state']==2 and e['fixture_reason']=='complete' for e in rows) and not any(e['fixture_state']==3 for e in rows),
        walking_fixture_all_phases_reach_final_actor={a['fixture_phase'] for e,a in matched}=={1,2,3,4},
        walking_fixture_yaw_only_native_world_movement=bool(errors) and max(errors)<2e-6,
        walking_fixture_actual_root_moves=displacement>1e-4,
        walking_fixture_settled_root_stable_with_hmd_motion=len(tail)==32 and bool(settled_inputs) and
            len({tuple(e['stable_q32']) for e in tail})==1 and len({tuple(e['camera_q32']) for e in settled_inputs})>1 and
            all(e.get('fixture_state') in (2,3) and e['effective_stick']==[0,0] and e['native_digital']==0 for e in settled_inputs),
    )
    metrics=dict(walking_fixture_synthetic_ticks=len(synthetic),walking_fixture_nonzero_ticks=len(active),walking_fixture_phases=sorted(phases),
                 walking_fixture_final_actor_matches=len(matched),walking_fixture_world_movement_error=max(errors,default=None),
                 walking_fixture_root_displacement_m=displacement,
                 walking_fixture_abort_reasons=sorted({e['fixture_reason'] for e in rows if e.get('fixture_state')==3}),
                 walking_fixture_real_controller_tested=False)
    return checks,metrics
