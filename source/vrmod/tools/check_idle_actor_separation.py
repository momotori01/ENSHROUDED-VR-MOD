"""Stationary idle fixture: changing HMD camera must not create movement input.

Correlates actual final ActorInput camera values with earlier private HMD input
records. Unknown simulation ticks are never treated as matching client ticks.
Stable-root samples are evidence for this stationary fixture, not proof about
all physics, network states, teleports or equipment. Head Aim combat is untested.
"""
import collections


def check_idle_actor(native, mode='idle',pose_expectation='dynamic'):
    if mode not in ('idle','walking'):raise ValueError('unknown input policy')
    def values(e):return (e['entity'],*e['camera_q32'],*e['camera_orientation'])
    applied=[e for e in native if e['event']==f'input_head_{mode}_sample' and e['applied']]
    # A walking policy run can still be a stationary fixture. Do not silently
    # select only its zero-control rows if actual movement was also requested.
    controls=[e for e in native if e['event']==f'input_head_{mode}_sample']
    by_camera=collections.defaultdict(list)
    for e in applied:by_camera[values(e)].append(e)
    final=[e for e in native if e['event']=='input_actor_final_sample']
    matched=[]
    for e in final:
        past=[a for a in by_camera[values(e)] if a['us']<=e['us']]
        if past:matched.append((e,max(past,key=lambda a:a['us'])))
    roots=[e for e in native if e['event']=='root_camera_sample']
    changed_positions={tuple(e['camera_q32']) for e,_ in matched}
    changed_orientations={tuple(e['camera_orientation']) for e,_ in matched}
    checks={
        'idle_actor_stationary_fixture_controls':bool(controls) and all(e['native_stick']==[0,0] and e['native_digital']==0 for e in controls),
        'idle_actor_receives_varying_hmd_camera':len(changed_positions)>1 and len(changed_orientations)>1,
        'idle_hmd_does_not_generate_final_movement':bool(matched) and all(e['desired_local']==[0,0,0] and e['world_move']==[0,0,0] and a['native_stick']==[0,0] and a['native_digital']==0 for e,a in matched),
        'idle_fixture_stable_root_unchanged':bool(roots) and all(len({tuple(e[k]) for e in roots})==1 for k in ('stable_q32','source_q32','target_q32')),
        'idle_actor_tick_availability_explicit':bool(final) and all(e['tick_available'] or e['tick']==0 for e in final),
    }
    if pose_expectation=='fixed':
        checks.pop('idle_actor_receives_varying_hmd_camera')
        checks['idle_actor_receives_fixed_hmd_camera']=len(changed_positions)==len(changed_orientations)==1
    elif pose_expectation not in ('dynamic','settled'):raise ValueError('unknown pose expectation')
    metrics=dict(idle_actor_camera_matches=len(matched),idle_actor_unknown_tick_matches=sum(not e['tick_available'] for e,_ in matched),
        idle_actor_distinct_positions=len(changed_positions),idle_actor_distinct_orientations=len(changed_orientations),
        idle_actor_distinct_body_look=len({tuple(e['world_look']) for e,_ in matched}),idle_actor_root_samples=len(roots),
        idle_actor_max_camera_consumer_delay_us=max((e['us']-a['us'] for e,a in matched),default=None))
    return checks,metrics
