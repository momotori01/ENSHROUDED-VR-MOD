"""Read-only XR -> input snapshot checks; does not certify applied gameplay input."""
import math
from check_recorded_xr_native import normalize,multiply,rotate

def check_input_pose(native,xr):
    def events(rows,name):return [r for r in rows if r['event']==name]
    published=events(native,'input_pose_published');samples=events(native,'input_pose_sample')
    by_frame={e['frame']:e for e in published}
    # Event/special cameras deliberately invalidate the gameplay channel. Require
    # one explicit decision for every XR frame, using the frozen render policy.
    native_events=events(native,'input_pose_native_camera')
    native_frames={e['frame'] for e in native_events}
    settling=events(native,'input_pose_height_settling');settling_frames={e['frame'] for e in settling}
    roots=events(native,'root_camera_sample')
    native_ids={0,1,2,3,18,30,31,32,33,34,35,36,37,38,39,40,41,42}
    expected_native={e['frame'] for e in roots if e.get('camera_modes') and
                     (e.get('presentation_id',e['camera_id']) in native_ids or e.get('presentation_id',e['camera_id'])>=45)}
    begun={e['frame']:e for e in events(xr,'native_frame_begin')}
    heads={e['frame']:e for e in events(xr,'hmd')}
    anchors={e['recorded_xr_frame']:e for e in events(native,'recorded_xr_anchor')}
    senders={e['sample']:e for e in events(native,'input_sender_sample')}
    current=[e for e in samples if e['status']=='current']
    valid=[e for e in current if e['geometry_fallback']=='none']
    checks={
        'input_pose_one_decision_per_xr_frame':bool(published) and len(by_frame)==len(published) and len(native_frames)==len(native_events) and len(settling_frames)==len(settling) and not(set(by_frame)&native_frames) and not(settling_frames&(set(by_frame)|native_frames)) and set(by_frame)|native_frames|settling_frames==set(begun),
        'input_pose_native_decisions_match_camera_policy':native_frames==expected_native and all(e['invalidated'] for e in native_events),
        'input_pose_runtime_identity_and_flags':bool(published) and all(e['frame'] in begun and e['time']==begun[e['frame']]['time'] and e['head_flags']==begun[e['frame']]['head_flags'] and e['session_generation']==begun[e['frame']]['session_generation'] and e['origin_epoch']>0 for e in published),
        'input_pose_sender_identity':bool(samples) and all(e['sample'] in senders and e['entity']==senders[e['sample']]['entity'] and e['tick']==senders[e['sample']]['tick'] for e in samples),
        'input_pose_current_is_fresh_same_generation':bool(current) and all(e['frame'] in by_frame and e['entity']==by_frame[e['frame']]['entity'] and e['time']==by_frame[e['frame']]['time'] and e['session_generation']==by_frame[e['frame']]['session_generation'] and e['origin_epoch']==by_frame[e['frame']]['origin_epoch'] and e['head_flags']==by_frame[e['frame']]['head_flags'] and 0<=e['head_age_ms']<=e['max_age_ms'] and 0<=e['root_age_ms']<=e['max_age_ms'] for e in current),
        'input_pose_observation_never_applied':bool(samples) and all(e['geometry_preview_only'] and not e['applied'] and not e['gameplay_state_validated'] for e in samples),
        'input_pose_invalid_snapshot_never_proposed':all(e['geometry_fallback']=='tracking_unavailable' and 'aim_forward' not in e for e in samples if e['status']!='current' or e['head_flags']&15!=15),
    }
    if settling:
        live={e['frame'] for e in events(native,'live_native_pair_complete')}
        checks['input_pose_withheld_until_height_settled']=all(e['invalidated'] for e in settling) and (not by_frame or max(settling_frames)<min(by_frame)) and not(settling_frames&live)
    position_error=aim_error=move_error=0.
    if not published:return checks,{}
    first=heads[published[0]['frame']];forward=rotate(normalize(first['orientation']),[0,0,-1])
    yaw=math.atan2(-forward[0],-forward[2]) if forward[0]*forward[0]+forward[2]*forward[2]>1e-10 else 0
    origin_q=[0,math.sin(-yaw/2),0,math.cos(-yaw/2)];origin_p=rotate(origin_q,[-v for v in first['position']])
    for e in valid:
        f=e['frame'];h=heads[f];a=anchors[f]
        local=[x+y for x,y in zip(origin_p,rotate(origin_q,h['position']))];local[2]=-local[2]
        offset=rotate(a['orientation'],[x+y for x,y in zip(local,a['camera_offset'])])
        position_error=max(position_error,max(abs((x-y)/2**32-z) for x,y,z in zip(e['camera_q32'],a['camera_q32'],offset)))
        q=normalize(multiply(origin_q,normalize(h['orientation'])));q=[-q[0],-q[1],q[2],q[3]]
        q=normalize(multiply(normalize(a['orientation']),q));forward=rotate(q,[0,0,1])
        aim_error=max(aim_error,max(abs(x-y) for x,y in zip(e['aim_forward'],forward)))
        sx,sz=senders[e['sample']]['movement_stick'];y=e['yaw']
        expected=[sx*math.cos(y)+sz*math.sin(y),0,-sx*math.sin(y)+sz*math.cos(y)]
        move_error=max(move_error,max(abs(x-y) for x,y in zip(e['world_movement'],expected)))
        if forward[0]*forward[0]+forward[2]*forward[2]>1e-10:
            move_error=max(move_error,abs(math.remainder(e['yaw']-math.atan2(forward[0],forward[2]),2*math.pi)))
    checks['input_pose_geometry_matches_runtime_and_stable_root']=bool(valid) and max(position_error,aim_error,move_error)<1e-5
    return checks,dict(input_pose_position_error_m=position_error,input_pose_aim_error=aim_error,
                       input_pose_movement_error=move_error,input_pose_current_samples=len(current),input_pose_geometry_samples=len(valid))
