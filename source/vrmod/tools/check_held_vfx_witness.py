"""Read-only comparison of declared VFX offset, original model, and emitter."""
import argparse,json,math,struct
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
def transform(h):
 b=bytes.fromhex(h);return struct.unpack_from('<3q',b),struct.unpack_from('<4f',b,24),struct.unpack_from('<3f',b,40)
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
def rotate(q,v,normalize=False):
 if normalize:q=[x/math.sqrt(sum(x*x for x in q)) for x in q]
 c=cross(q[:3],v);cc=cross(q[:3],c)
 return [v[i]+2*q[3]*c[i]+2*cc[i] for i in range(3)]
def mul(a,b):
 c=cross(a[:3],b[:3]);return [a[3]*b[i]+a[i]*b[3]+c[i] for i in range(3)]+[a[3]*b[3]-sum(a[i]*b[i] for i in range(3))]
rows=[];errors=[]
for line in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines():
 e=json.loads(line)
 if 'failure' in e['event']:errors.append(e)
 if e['event']!='held_vfx_render_witness':continue
 pos,q,scale=transform(e['source_model']);ep,eq,es=transform(e['native_effect']);rp,rq,rs=transform(e['render_effect'])
 b=bytes.fromhex(e['offset_component']);local=struct.unpack_from('<3f',b,8);world=struct.unpack_from('<3f',b,20);offsetq=struct.unpack_from('<4f',b,32)
 delta=[(ep[i]-pos[i])/2**32 for i in range(3)];v=[local[i]*scale[i] for i in range(3)];raw=rotate(q,v);unscaled=rotate(q,local);norm=rotate(q,v,True);outq=mul(q,offsetq)
 rows.append(dict(frame=e['frame'],entity=e['entity'],item=e['item'],handle=e['handle'],source_scale=scale,source_qnorm=math.sqrt(sum(x*x for x in q)),native_delta=delta,local=local,world=world,unscaled_position_error_m=math.sqrt(sum((delta[i]-world[i]-unscaled[i])**2 for i in range(3))),raw_position_error_m=math.sqrt(sum((delta[i]-world[i]-raw[i])**2 for i in range(3))),normalized_position_error_m=math.sqrt(sum((delta[i]-world[i]-norm[i])**2 for i in range(3))),orientation_error=min(max(abs(outq[i]-eq[i]) for i in range(4)),max(abs(outq[i]+eq[i]) for i in range(4))),native_render_equal=e['native_effect'][:104]==e['render_effect'][:104],age_ms=e['age_ms']))
r=dict(samples=len(rows),items=sorted(set(x['item'] for x in rows)),max_unscaled_error_m=max((x['unscaled_position_error_m'] for x in rows),default=None),max_raw_error_m=max((x['raw_position_error_m'] for x in rows),default=None),max_normalized_error_m=max((x['normalized_position_error_m'] for x in rows),default=None),max_orientation_error=max((x['orientation_error'] for x in rows),default=None),all_native_render_equal=bool(rows) and all(x['native_render_equal'] for x in rows),failures=errors,rows=rows)
(a.probe/'held-vfx-witness-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in r.items() if k not in ('rows','failures')},indent=2))
