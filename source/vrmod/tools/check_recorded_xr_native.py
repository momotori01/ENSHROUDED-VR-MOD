"""Independent math checks for recorded OpenXR inputs rendered by the game.

No live OpenXR timing/submission claim. Recorded source precision limits pose
comparison; native reverse-Z cancellation permits small absolute matrix error.
"""
import argparse, json, math
from pathlib import Path
from check_native_series import rows
from export_recorded_xr import read_recording


def normalize(q):
    norm=math.sqrt(sum(x*x for x in q));return [x/norm for x in q]


def multiply(a,b):
    x,y,z,w=a;u,v,t,s=b
    return [w*u+x*s+y*t-z*v,w*v-x*t+y*s+z*u,w*t+x*v-y*u+z*s,w*s-x*u-y*v-z*t]


def rotate(q,v):
    q=normalize(q);return multiply(multiply(q,v+[0]),[-q[0],-q[1],-q[2],q[3]])[:3]


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
    scale,sizes,origin,frames=read_recording(a.probe/'recorded-xr.txt');log=rows(a.probe/'view-resource-probe.jsonl')
    anchors={e['recorded_xr_frame']:e for e in log if e['event']=='recorded_xr_anchor'}
    applied=[e for e in log if e['event']=='recorded_xr_eye_applied']
    native={(e['serial'],e['eye']):e for e in log if e['event']=='calibration_eye_transform'}
    checks={'all_recorded_frames_anchored':set(anchors)==set(frames),'all_recorded_eyes_applied':len(applied)==2*len(frames) and
            {(e['recorded_xr_frame'],e['eye']) for e in applied}=={(f,i) for f in frames for i in (0,1)}}
    errors={'position_m':0,'rotation_matrix':0,'projection_matrix':0};valid_time=True;valid_fov=True;once=True
    for e in applied:
        frame=e['recorded_xr_frame'];eye=e['eye'];time,views=frames[frame];v=views[eye];anchor=anchors[frame]
        actual=native[(e['serial'],eye)];valid_time &= e['recorded_predicted_time']==time;valid_fov &= e['fov']==v[7:]
        once &= e['projection_calls']==1
        # Complete xrLocateViews eye pose composed once with explicit recenter.
        local=[x+y for x,y in zip(origin[:3],rotate(origin[3:],v[:3]))];local[2]=-local[2]
        offset=rotate(anchor['orientation'],local)
        position_error=max(abs((x-y)/4294967296-z) for x,y,z in zip(actual['camera_q32'],anchor['camera_q32'],offset))
        errors['position_m']=max(errors['position_m'],position_error)
        q=normalize(multiply(origin[3:],normalize(v[3:7])));q=[-q[0],-q[1],q[2],q[3]]
        q=normalize(multiply(normalize(anchor['orientation']),q));qi=[-q[0],-q[1],-q[2],q[3]]
        expected=[rotate(qi,axis) for axis in ([1,0,0],[0,1,0],[0,0,1])]
        errors['rotation_matrix']=max(errors['rotation_matrix'],max(abs(expected[r][c]-actual['view_matrix'][r*4+c]) for r in range(3) for c in range(3)))
        left,right,up,down=map(math.tan,v[7:]);near=anchor['near'];far=anchor['far'];projection=[0]*16
        projection[0]=2/(right-left);projection[5]=2/(up-down);projection[8]=-(right+left)/(right-left)
        projection[9]=-(up+down)/(up-down);projection[10]=-near/(far-near);projection[11]=1;projection[14]=near*far/(far-near)
        errors['projection_matrix']=max(errors['projection_matrix'],max(abs(x-y) for x,y in zip(projection,actual['projection_matrix'])))
    checks.update(recorded_eye_time_preserved=valid_time,recorded_individual_fov_preserved=valid_fov,
                  exactly_one_projection_override_per_eye=once,complete_eye_pose_position=errors['position_m']<1e-5,
                  complete_eye_pose_rotation=errors['rotation_matrix']<1e-5,independent_asymmetric_native_projection=errors['projection_matrix']<1e-5)
    resolutions=[e for e in log if e['event']=='native_view_resolution_initialized']
    checks['runtime_recommended_extents_scaled']=len(resolutions)==2 and all(
        [e['width'],e['height']]==[math.ceil(v*scale) for v in sizes[i]] for i,e in enumerate(resolutions))
    checks['projection_hook_removed']=any(e['event']=='recorded_xr_projection_hook_removed' and e['status']==0 for e in log)
    for label in ('first','quarter','final'):
        textures=rows(a.probe/('replay-'+label)/'textures.jsonl')
        for eye in (0,1):
            named=[e for e in textures if e.get('name')==f'EVR calibration eye {eye}']
            checks[f'{label}_eye{eye}_actual_gpu_extent']=len(named)==1 and [named[0]['width'],named[0]['height']]==[math.ceil(v*scale) for v in sizes[eye]]
    result=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},max_errors=errors,limits=__doc__)
    (a.probe/'recorded-xr-checks.json').write_text(json.dumps(result,indent=2))
    for k,v in result['checks'].items():print(v,k)
    print(errors);return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
