"""Validate native read-only Root snapshots and bounded first-person anchors.

Checks the logged native interpolation inputs independently; stationary Root
with changing HMD is evidence only for the observed world, not all gameplay.
"""
import math
import struct
from check_recorded_xr_native import normalize, rotate


def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]


def check_root(rows,mode):
    samples=[e for e in rows if e['event']=='root_camera_sample']
    anchors={e['recorded_xr_frame']:e for e in rows if e['event']=='recorded_xr_anchor'}
    removed=[e for e in rows if e['event']=='root_camera_hook_removed']
    checks={
        'root_camera_samples_complete':bool(samples) and {s['frame'] for s in samples}==set(anchors),
        'root_camera_local_entity_stable':bool(samples) and len({s['entity'] for s in samples})==1 and all(s['entity']>0 for s in samples),
        'root_camera_snapshots_fresh':bool(samples) and all(0<=s.get('render_entry_age_ms',s['age_ms'])<=1000 for s in samples),
        'root_camera_hook_restored':len(removed)==1 and removed[0]['original_bytes_restored'] and removed[0]['remaining_callbacks']==0 and removed[0]['read_failures']==0,
    }
    if any('copied_at_render_entry' in s for s in samples):
        serials={e['frame']:e['serial'] for e in rows if e['event'] in ('world_warmup_frame_ended','live_native_pair_complete')}
        checks['root_camera_same_render_scope']=all(s.get('copied_at_render_entry') and s['render_serial']==serials.get(s['frame']) and s['age_ms']>=s['render_entry_age_ms'] for s in samples)
    error=0
    for s in samples:
        gap=max(1,s['tick']-s['source_tick']);alpha=f32(f32(f32(gap-1)+f32(s['fraction']))/f32(gap))
        expected=[a+int(f32(f32(f32(f32(b-a)*2**-32)*alpha)*2**32)) for a,b in zip(s['source_q32'],s['target_q32'])]
        error=max(error,max(abs(a-b) for a,b in zip(expected,s['stable_q32'])))
    checks['root_camera_native_interpolation_matches']=bool(samples) and error==0
    checks['root_camera_native_height_matches']=bool(samples) and all(abs(s['height']-(s['camera_target_q32'][1]-s['controller_root_q32'][1])/2**32)<1e-6 for s in samples)
    if mode==2:
        calibration=[e for e in rows if e['event']=='root_camera_calibrated']
        heading_calibration=[e for e in rows if e['event']=='root_heading_calibrated']
        controlled=any(s.get('camera_modes') for s in samples)
        native_ids=set(range(4))|{18,30,31,32,33}|set(range(34,43))|set(range(45,256))
        fp=[s for s in samples if not s.get('camera_modes') or s.get('presentation_id',s['camera_id']) not in native_ids]
        third=[s for s in samples if s not in fp]
        checks['first_person_single_height_calibration']=len(calibration)==1 and calibration[0]['height']>0
        checks['first_person_stable_root_used']=bool(fp) and all(anchors[s['frame']]['anchor_kind']=='stable_root_first_person_diagnostic' and anchors[s['frame']]['camera_q32']==s['stable_q32'] for s in fp)
        settled=any('height_calibrated' in a for a in anchors.values())
        if not settled:
            checks['first_person_height_frozen_without_shoulder']=len(calibration)==1 and all(max(abs(x-y) for x,y in zip(anchors[s['frame']]['camera_offset'],[0,calibration[0]['height'],0]))<1e-6 for s in fp)
        else:
            # Settled-height builds: warmup anchors before calibration follow the native
            # height (provisional); every later first-person anchor and every live pair
            # uses the single frozen calibration.
            height=calibration[0].get('height_precise',calibration[0]['height']) if len(calibration)==1 else None
            provisional=[s for s in fp if anchors[s['frame']].get('height_calibrated') is False]
            frozen=[s for s in fp if anchors[s['frame']].get('height_calibrated') is True]
            live={e['frame'] for e in rows if e['event']=='live_native_pair_complete'}
            checks['first_person_height_frozen_without_shoulder']=height is not None and bool(frozen) and all(max(abs(x-y) for x,y in zip(anchors[s['frame']]['camera_offset'],[0,height,0]))<1e-6 for s in frozen) and all(max(abs(x-y) for x,y in zip(anchors[s['frame']]['camera_offset'],[0,s['height'],0]))<1e-6 for s in provisional)
            checks['first_person_height_settled_before_live_pairs']=len(calibration)==1 and all(a.get('height_calibrated') is True for f,a in anchors.items() if f in live) and (not provisional or max(s['frame'] for s in provisional)<min(s['frame'] for s in frozen)) and calibration[0].get('settle_spread_m',1)<=0.001 and calibration[0].get('settle_span_ms',0)>=1500 and calibration[0].get('settle_samples',0)>=15 and abs(calibration[0]['height_precise']-calibration[0]['height'])<1e-6 and calibration[0].get('camera_id')==19 and calibration[0].get('settle_values')=='height_distance_shoulder'
            # Independent cross-check: when the run ends on a steady PlayerCameraIdle, the
            # frozen height must equal that steady native height (catches 1.75 vs 1.65 m).
            tail=samples[-60:]
            if len(tail)==60 and all(s['camera_id']==19 for s in tail) and max(s['height'] for s in tail)-min(s['height'] for s in tail)<=0.001:
                checks['first_person_height_matches_steady_idle_camera']=len(calibration)==1 and abs(calibration[0].get('height_precise',calibration[0]['height'])-tail[-1]['height'])<=0.001
        if third:
            checks['event_native_camera_position_and_rotation']=all(
                anchors[s['frame']]['anchor_kind']=='native_event_or_special_camera_diagnostic' and
                anchors[s['frame']]['camera_q32']==anchors[s['frame']]['native_camera_q32'] and
                anchors[s['frame']]['camera_offset']==[0,0,0] and
                max(abs(a-b) for a,b in zip(normalize(anchors[s['frame']]['orientation']),normalize(anchors[s['frame']]['native_orientation'])))<1e-5 for s in third)
        if controlled:
            checks['first_person_heading_calibrated_once']=len(heading_calibration)==1
            checks['first_person_same_controller_input_current']=all(s.get('control_valid') for s in fp)
            controls=[e for e in rows if e['event']=='camera_control_observer_removed']
            checks['camera_control_hooks_restored']=len(controls)==1 and controls[0]['restored'] and controls[0]['failures']==0 and controls[0]['callbacks']==0
        yaw_error=0;previous=0
        for s in fp:
            f=rotate(normalize(s['unshaken_orientation']),[0,0,1])
            yaw=math.atan2(f[0],f[2]) if f[0]*f[0]+f[2]*f[2]>1e-10 else previous;previous=yaw
            if controlled:
                # turn_sign -1 since 2026-10-09 (the accumulated orbit input turns the native camera against heading());
                # older logs have no field and were produced with +1.
                sign=heading_calibration[0].get('turn_sign',1) if len(heading_calibration)==1 else 1
                yaw=(heading_calibration[0]['yaw']+sign*(s['gameplay_turn']-heading_calibration[0]['gameplay_turn'])) if len(heading_calibration)==1 else math.nan
            q=normalize(anchors[s['frame']]['orientation'])
            actual=rotate(q,[0,0,1]);expected=[math.sin(yaw),0,math.cos(yaw)]
            yaw_error=max(yaw_error,max(abs(x-y) for x,y in zip(actual,expected)),abs(q[0]),abs(q[2]))
        checks['first_person_input_only_yaw' if controlled else 'first_person_unshaken_yaw_only']=bool(fp) and yaw_error<1e-5 and (not controlled or len(heading_calibration)==1)
    return checks
