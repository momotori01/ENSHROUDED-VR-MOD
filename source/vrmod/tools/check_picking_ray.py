"""Native PickingTarget request geometry; no weapon, damage or hit-result claim."""
import collections,math,struct
from check_recorded_xr_native import rotate


def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def ulp32(x):return 2**(math.frexp(abs(x))[1]-24) if x else 2**-149


def camera_coverage(applied, matched, pose_expectation, stopped_at=None):
    key=lambda e:(e['entity'],*e['camera_q32'],*e['camera_orientation'])
    consumed={key(e) for e,a in matched}
    checks={'picking_consumes_varying_hmd_camera':len(consumed)>1}
    if stopped_at is not None:
        # Used only by the separately audited admission-stop scenario. The
        # physical head can stay fixed for the ENTIRE authorized picking window,
        # while the native attack moves the root after VR aim has been revoked.
        # Never use post-stop vanilla camera samples as evidence of tracked aim.
        checks['picking_epoch_window_closed']=bool(applied) and all(a['us']<stopped_at for a in applied)
        checks['picking_matches_before_epoch_close']=bool(matched) and all(a['us']<=e['us']<stopped_at for e,a in matched)
        produced={key(a) for a in applied}
        if len(produced)==1:
            checks.pop('picking_consumes_varying_hmd_camera')
            checks['picking_consumes_fixed_camera_in_authorized_window']=consumed==produced
    if pose_expectation=='fixed':
        checks.pop('picking_consumes_varying_hmd_camera',None)
        checks['picking_consumes_fixed_hmd_camera']=len(consumed)==1
    elif pose_expectation=='fixed-root-motion':
        checks['picking_fixed_head_orientation_with_root_motion']=len({tuple(e['camera_orientation']) for e,a in matched})==1
    elif pose_expectation not in ('dynamic','settled'):raise ValueError('unknown pose expectation')
    return checks


def check_picking(native,pose_expectation='dynamic',*,stopped_at=None):
    rows=[e for e in native if e['event']=='input_picking_sample']
    end=[e for e in native if e['event']=='input_picking_removed']
    checks={
        'picking_no_read_failure':not any(e['event']=='input_picking_failure' for e in native),
        'picking_restored':len(end)==1 and end[0]['restored'] and end[0]['callbacks']==end[0]['failures']==0,
        'picking_samples_accounted':bool(rows) and len(end)==1 and len(rows)==end[0]['samples']<=256 and end[0]['calls']>=len(rows),
        'picking_read_only_local':bool(rows) and all(e['entity']==e['local_entity']!=0 and e['read_only'] and e['source_camera_unchanged'] and e['return_is_output'] and e['caller']=='0x3d6b11' for e in rows),
        'picking_native_request_preserved':bool(rows) and all(e['ray_transform_q']==[0,0,0,1] and e['ray_scale']==[1,1,1] and e['range']==31 and e['filter']==0x208 and e['flags']==1 and not e['option'] for e in rows),
    }
    valid=True;max_origin=max_direction=max_forward=0.
    for e in rows:
        q=e['camera_orientation'];forward=rotate(q,[0,0,1])
        # Decimal log strings round-trip to the original float, but subtracting
        # them directly as doubles amplifies their decimal residual at world
        # coordinates. Recover the actual native endpoints before subtraction.
        origin=list(map(f32,e['float_origin']));point=list(map(f32,e['float_endpoint']))
        delta=[b-a for a,b in zip(origin,point)];length=math.sqrt(sum(v*v for v in delta))
        if not length:valid=False;continue
        wanted=[x/length for x in delta]
        pe=max(abs(e['ray_origin_q32'][j]/2**32-(e['camera_q32'][j]/2**32+forward[j]*e['camera_distance'])) for j in range(3))
        de=max(abs(x-y) for x,y in zip(wanted,e['ray_direction']))
        fe=max(abs(x-y) for x,y in zip(forward,e['ray_direction']))
        # Native performs two world-float conversions, then normalizes their
        # difference. Account for the measured coordinate ULP, not a VR deadzone.
        precision=max(ulp32(v) for v in origin+point)
        exact_q32=[int(f32(f32(v*2**32)+.5)) for v in origin]
        valid &= (pe<4*precision+2e-6 and de<3e-7 and fe<4*precision+2e-6 and
                  exact_q32==e['ray_origin_q32'] and abs(sum(x*x for x in q)-1)<2e-5)
        max_origin=max(max_origin,pe);max_direction=max(max_direction,de);max_forward=max(max_forward,fe)
    checks['picking_geometry_matches_native_camera']=bool(rows) and valid
    applied=[e for e in native if e['event'] in ('input_head_idle_sample','input_head_walking_sample') and e['applied']]
    metrics=dict(picking_samples=len(rows),picking_origin_error_m=max_origin,picking_endpoint_direction_error=max_direction,
                 picking_forward_quantization_error=max_forward,picking_request_ids=len({e['request_id'] for e in rows}))
    if applied:
        by_camera=collections.defaultdict(list)
        key=lambda e:(e['entity'],*e['camera_q32'],*e['camera_orientation'])
        for e in applied:by_camera[key(e)].append(e)
        matched=[]
        for e in rows:
            prior=[a for a in by_camera[key(e)] if a['us']<=e['us']]
            if prior:matched.append((e,max(prior,key=lambda a:a['us'])))
        checks.update(camera_coverage(applied,matched,pose_expectation,stopped_at))
        checks['picking_hmd_first_person_origin']=bool(matched) and all(e['camera_distance']==0 for e,a in matched)
        metrics.update(picking_hmd_matches=len(matched),picking_hmd_max_consumer_delay_us=max((e['us']-a['us'] for e,a in matched),default=None))
    return checks,metrics
