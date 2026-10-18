"""Audit native versus stable eye planes from the same runtime descriptor and matrix."""
import argparse,json,math,struct,collections
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('probe',type=Path);a=p.parse_args()
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
def invert(m):
 a=[[m[r*4+c] for c in range(3)]+[float(r==c) for c in range(3)] for r in range(3)]
 for c in range(3):
  k=max(range(c,3),key=lambda r:abs(a[r][c]));a[c],a[k]=a[k],a[c];s=a[c][c];a[c]=[v/s for v in a[c]]
  for r in range(3):
   if r!=c:s=a[r][c];a[r]=[v-s*w for v,w in zip(a[r],a[c])]
 return [r[3:] for r in a]
rows=[];count=collections.Counter();failure=[];complete=False
for line in (a.probe/'view-resource-probe.jsonl').open(encoding='utf-8'):
 e=json.loads(line)
 if e['event'].endswith('_failure'):failure.append(e)
 if e['event']=='view_resource_hook_complete':complete=e.get('result')==0
 if e['event']!='eye_frustum_planes':continue
 count[(e['eye'],e['caller'])]+=1;m=list(map(f32,e['matrix']));inv=invert(m);d=bytes.fromhex(e['descriptor']);l,r,b,t,n,f=struct.unpack_from('<6f',d,4)
 def world(v):return [sum((v[k]-m[12+k])*inv[k][j] for k in range(3)) for j in range(3)]
 planes={key:[list(map(f32,pl)) for pl in e[key]] for key in ['native_planes','stable_planes']};metrics={}
 for z in [n,1,10,100,1000,f]:
  points=[world([x/n*z,y/n*z,z]) for x in [l,r] for y in [b,t]]
  metrics[str(z)]={key:max(sum(pl[k]*point[k] for k in range(3))+pl[3] for pl in ps for point in points) for key,ps in planes.items()}
 rows.append(dict(frame=e['frame'],eye=e['eye'],caller=e['caller'],applied=e['applied'],metrics=metrics))
groups={}
for caller in ['0xf9226','0xf9240']:
 rr=[r for r in rows if r['caller']==caller];groups[caller]={key:max((v[key] for r in rr for v in r['metrics'].values()),default=None) for key in ['native_planes','stable_planes']};groups[caller]['at_100m']={key:max((r['metrics']['100'][key] for r in rr),default=None) for key in ['native_planes','stable_planes']}
checks=dict(sufficient_samples=all(count[(eye,caller)]>=20 for eye in [0,1] for caller in groups),stable_contains_frustum=bool(rows) and all(v['stable_planes']<=1e-4 for r in rows for v in r['metrics'].values()),uniform_mode=len(set(r['applied'] for r in rows))==1,no_failures=not failure,hook_completed=complete)
out=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,samples=len(rows),applied=rows[0]['applied'] if rows else None,groups=groups,failures=failure,rows=rows,limitations='Contains intended frustum corners; does not establish GPU consumption or image quality.')
(a.probe/'eye-frustum-check.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in out.items() if k!='rows'},indent=2));raise SystemExit(not all(checks.values()))
