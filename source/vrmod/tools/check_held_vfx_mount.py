"""Audit scoped held emitter updates against independent pose calculations."""
import argparse,json,struct,math,collections
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args()
def tf(h):
 b=bytes.fromhex(h);return struct.unpack_from('<3q',b),struct.unpack_from('<4f',b,24),b[40:]
def cross(a,b):return [a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]]
rows=[json.loads(l) for l in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
mount=[x for x in rows if x['event']=='held_vfx_mounted'];rest=[x for x in rows if x['event']=='held_vfx_mount_restored'];summ=[x for x in rows if x['event']=='held_vfx_mount_summary']
errors=[];positions=[]
for x in mount:
 cp,cq,cs=tf(x['clone']);tp,tq,ts=tf(x['target']);bp,bq,bs=tf(x['before']);b=bytes.fromhex(x['offset_component']);local=struct.unpack_from('<3f',b,8);world=struct.unpack_from('<3f',b,20);oq=struct.unpack_from('<4f',b,32)
 v=cross(cq[:3],local);vv=cross(cq[:3],v);delta=[local[i]+2*cq[3]*v[i]+2*vv[i]+world[i] for i in range(3)];qv=cross(cq[:3],oq[:3]);eq=[cq[3]*oq[i]+cq[i]*oq[3]+qv[i] for i in range(3)]+[cq[3]*oq[3]-sum(cq[i]*oq[i] for i in range(3))]
 errors.append(dict(position=math.sqrt(sum(((tp[i]-cp[i])/2**32-delta[i])**2 for i in range(3))),rotation=min(max(abs(eq[i]-tq[i]) for i in range(4)),max(abs(eq[i]+tq[i]) for i in range(4))),scale_preserved=ts==bs))
 positions.append(tp)
span=[(max(v[i] for v in positions)-min(v[i] for v in positions))/2**32 for i in range(3)] if positions else [0,0,0]
checks=dict(sufficient_motion=len(mount)>=20 and max(span)>.2,declared_offset=bool(errors) and max(x['position'] for x in errors)<1e-6 and max(x['rotation'] for x in errors)<1e-6,effect_scale_preserved=bool(errors) and all(x['scale_preserved'] for x in errors),same_logged_scopes=collections.Counter((x['frame'],x['handle']) for x in mount)==collections.Counter((x['frame'],x['handle']) for x in rest),restoration_verified=bool(rest) and all(x['restored'] and x['target_stable'] and x['native_unchanged'] and x['cpu_complete'] for x in rest),summary_complete=bool(summ) and all(x['moved']==x['restored'] and x['moved']>0 and x['failures']==0 for x in summ),no_failures=not any('failure' in x['event'] for x in rows),hook_completed=any(x['event']=='view_resource_hook_complete' and x['result']==0 for x in rows))
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},mounts_logged=len(mount),restores_logged=len(rest),target_position_span_m=span,max_position_error_m=max((x['position'] for x in errors),default=None),summary=summ,limitations='Native emitter/attachment and scope evidence. Images must establish rendered flame/light position; real controllers not tested.')
(a.probe/'held-vfx-mount-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(not all(checks.values()))
