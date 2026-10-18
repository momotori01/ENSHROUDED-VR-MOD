"""Convert validated paired OpenXR logs to bounded offline native-render inputs.

Preserves complete Runtime eye poses/FOVs and recommended extents. First logged
head pose sets an explicit recenter transform. No resampling or invented poses.
"""
import argparse, hashlib, json, math
from pathlib import Path


def read_recording(path):
    it=iter(path.read_text().split())
    if next(it)!='EVR_RECORDED_XR_1':raise ValueError('recorded XR magic')
    count=int(next(it));scale=float(next(it));sizes=[[int(next(it)),int(next(it))] for _ in (0,1)]
    origin=[float(next(it)) for _ in range(7)];frames={}
    for _ in range(count):
        index=int(next(it));time=int(next(it));views=[[float(next(it)) for _ in range(11)] for _ in (0,1)]
        if index in frames:raise ValueError('duplicate recorded frame')
        frames[index]=(time,views)
    if next(it,None) is not None:raise ValueError('trailing recorded data')
    return scale,sizes,origin,frames


def export(source, output, count=300, scale=1):
    rows=[json.loads(s) for s in source.read_text().splitlines()]
    sizes={e['eye']:e for e in rows if e['event']=='swapchain'}
    eyes={}; heads={}; committed={}
    for e in rows:
        if e['event']=='eye_render':
            key=(e['frame'],e['eye'])
            if key in eyes:raise ValueError('multiple sessions/duplicate eye frame in input')
            eyes[key]=e
        elif e['event']=='hmd':heads[e['frame']]=e
        elif e['event']=='stereo_frame':committed[e['frame']]=e['predicted_time']
    selected=sorted(committed)[:count]
    if len(selected)!=count or count<1 or count>1000 or set(sizes)!={0,1}:raise ValueError('incomplete recorded series')
    if not math.isfinite(scale) or scale<=0:raise ValueError('invalid scale')
    first=heads[selected[0]];q=first['orientation'];norm=math.sqrt(sum(v*v for v in q));q=[v/norm for v in q]
    qi=[-v for v in q[:3]]+[q[3]]
    def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
    v=[-v for v in first['position']];uv=cross(qi[:3],v);uuv=cross(qi[:3],uv)
    origin=[v[i]+2*qi[3]*uv[i]+2*uuv[i] for i in range(3)]+qi
    lines=[f'EVR_RECORDED_XR_1 {count} {scale:.9g}']
    for eye in (0,1):lines.append(f"{sizes[eye]['recommended_width']} {sizes[eye]['recommended_height']}")
    lines.append(' '.join(format(v,'.9g') for v in origin))
    for frame in selected:
        pair=[eyes[(frame,eye)] for eye in (0,1)]
        if any(e['time']!=committed[frame] for e in pair):raise ValueError('mixed eye predicted times')
        lines.append(f'{frame} {committed[frame]}')
        for e in pair:
            values=e['position']+e['orientation']+e['fov']
            if not all(math.isfinite(v) for v in values):raise ValueError('nonfinite eye')
            lines.append(' '.join(format(v,'.9g') for v in values))
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text('\n'.join(lines)+'\n')
    provenance=dict(source=str(source.resolve()),sha256=hashlib.sha256(source.read_bytes()).hexdigest(),
                    runtime=[e for e in rows if e['event']=='runtime'],frames=selected,render_scale=scale,
                    recenter_from_head=first,live_openxr_session=False,description=__doc__)
    output.with_suffix('.json').write_text(json.dumps(provenance,indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--frames',type=int,default=300);p.add_argument('--scale',type=float,default=1)
    a=p.parse_args();export(a.source,a.output,a.frames,a.scale)
    print('Exported recorded Runtime input; no live OpenXR session:',a.output)
