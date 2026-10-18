"""Validate shared-camera application and stereo containment from native logs.

The enclosing view is a shared-update helper, never an eye image. This checks
native matrices, argument preservation, and near/far stereo corners, not the
native VFX shader's internal consumption or final effect visibility.
"""
import math
from check_recorded_xr_native import normalize,rotate


def check_shared_culling(native,skipped=frozenset()):
    """skipped: submitted serials whose desktop View was skipped (2026-10-06). The enclosing camera belongs to the original
    View, so those frames carry none; their VFX culling uses the stereo union planes instead (vfx_culling_union)."""
    def events(name):return [e for e in native if e['event']==name]
    done=events('live_native_pair_complete');serials={e['serial']:e['frame'] for e in done if e['serial'] not in skipped}
    all_centers=events('world_shared_culling_camera');centers=[e for e in all_centers if e['serial'] in serials]
    eyes={(e['serial'],e['eye']):e for e in events('calibration_eye_transform')}
    applied={(e['serial'],e['eye']):e for e in events('recorded_xr_eye_applied')}
    anchors={e['recorded_xr_frame']:e for e in events('recorded_xr_anchor')}
    complete=bool(done) and len(centers)==len(serials) and len({e['serial'] for e in centers})==len(serials) and all(
        e['serial'] in serials and e['frame']==serials[e['serial']] and all((e['serial'],i) in eyes and (e['serial'],i) in applied for i in (0,1)) for e in centers)
    checks={'shared_culling_all_submitted_frames':complete or (not serials and bool(skipped)),
            'shared_culling_preserved_game_arguments':bool(all_centers) and all(e['source_parameters_unchanged'] and e['projection_calls']==1 for e in all_centers)}
    if skipped:
        union=events('vfx_culling_union');summary=events('vfx_culling_union_summary')
        union_applied=summary[-1]['applied'] if summary else max((e['frames_applied'] for e in union),default=0)
        failures=summary[-1]['failures'] if summary else max((e['failures'] for e in union),default=0)
        # Without the end-of-run summary (older probes) the counter is only logged every 100 frames.
        checks['vfx_culling_union_covers_skipped_frames']=(bool(union) and failures==0 and not events('vfx_culling_union_failure') and
            all(e['applied'] and e['builds']==1 and e['stage_calls']==1 for e in union) and union_applied>=len(skipped)-(0 if summary else 100))
    errors={'shared_projection_matrix':0.,'shared_rotation_matrix':0.,'shared_frustum_slope_excess':0.,'shared_frustum_depth_excess_m':0.}
    if complete:
        for e in centers:
            q=normalize(e['orientation']);qi=[-q[0],-q[1],-q[2],q[3]]
            expected=[rotate(qi,axis) for axis in ([1,0,0],[0,1,0],[0,0,1])]
            errors['shared_rotation_matrix']=max(errors['shared_rotation_matrix'],max(abs(expected[r][c]-e['view_matrix'][r*4+c]) for r in range(3) for c in range(3)))
            l,r,u,d=map(math.tan,e['fov']);n,f=e['near'],e['far'];m=[0.]*16
            m[0]=2/(r-l);m[5]=2/(u-d);m[8]=-(r+l)/(r-l);m[9]=-(u+d)/(u-d)
            m[10]=-n/(f-n);m[11]=1;m[14]=n*f/(f-n)
            errors['shared_projection_matrix']=max(errors['shared_projection_matrix'],max(abs(a-b) for a,b in zip(m,e['projection_matrix'])))
            for eye in (0,1):
                native_eye=eyes[e['serial'],eye];eye_fov=applied[e['serial'],eye]['fov'];a=anchors[e['frame']]
                origin=[(x-y)/4294967296 for x,y in zip(native_eye['camera_q32'],e['camera_q32'])]
                matrix=native_eye['view_matrix']
                for depth in (a['near'],a['far']):
                    for x in map(math.tan,eye_fov[:2]):
                        for y in map(math.tan,eye_fov[2:]):
                            ray=[x*depth,y*depth,depth]
                            world=[origin[c]+sum(matrix[c*4+k]*ray[k] for k in range(3)) for c in range(3)]
                            local=rotate(qi,world);z=local[2]
                            errors['shared_frustum_depth_excess_m']=max(errors['shared_frustum_depth_excess_m'],n-z,z-f)
                            if z<=0:errors['shared_frustum_slope_excess']=math.inf;continue
                            errors['shared_frustum_slope_excess']=max(errors['shared_frustum_slope_excess'],l-local[0]/z,local[0]/z-r,d-local[1]/z,local[1]/z-u)
    checks['shared_culling_native_matrices_match']=complete and errors['shared_projection_matrix']<1e-5 and errors['shared_rotation_matrix']<1e-5
    # Native eye matrices are logged with six significant digits. At 10km,
    # their rounding permits centimetres of reconstructed far-corner error.
    checks['shared_culling_encloses_native_stereo_corners']=complete and errors['shared_frustum_slope_excess']<5e-5 and errors['shared_frustum_depth_excess_m']<.1
    # Preserve historical evidence: the stable builder was added after the
    # first helper-camera capture. New runs must prove both native callsites.
    if events('shared_stable_frustum_hook_enabled'):
        import collections
        stable=events('shared_stable_frustum')
        expected=collections.Counter((e['serial'],caller) for e in all_centers for caller in ('0xf9226','0xf9240'))
        checks['shared_stable_frustum_both_calls']=bool(all_centers) and collections.Counter((e['serial'],e['caller']) for e in stable)==expected and all(
            e.get('stable_frustum_calls')==2 and e.get('stable_frustum_mask')==3 and not e.get('stable_frustum_failed',True) for e in all_centers)
        removed=events('shared_stable_frustum_hook_removed')
        checks['shared_stable_frustum_no_failure_and_restored']=not events('shared_stable_frustum_failure') and len(removed)==1 and removed[0]['status']==0
        world_planes={e['serial']:e['planes'] for e in stable if e['caller']=='0xf9226'}
        maximum=-math.inf
        if complete and all(s in world_planes for s in serials):
            for e in centers:
                a=anchors[e['frame']]
                for eye in (0,1):
                    native_eye=eyes[e['serial'],eye];m=native_eye['view_matrix']
                    position=[x/2**32 for x in native_eye['camera_q32']]
                    for depth in (a['near'],a['far']):
                        for x in map(math.tan,applied[e['serial'],eye]['fov'][:2]):
                            for y in map(math.tan,applied[e['serial'],eye]['fov'][2:]):
                                ray=[x*depth,y*depth,depth]
                                point=[position[c]+sum(m[c*4+k]*ray[k] for k in range(3)) for c in range(3)]
                                maximum=max(maximum,max(sum(p[k]*point[k] for k in range(3))+p[3] for p in world_planes[e['serial']][:5]))
        errors['shared_stable_plane_max_excess_m']=maximum if math.isfinite(maximum) else None
        checks['shared_stable_planes_enclose_all_submitted_stereo_views']=math.isfinite(maximum) and maximum<=.001
    return checks,errors
