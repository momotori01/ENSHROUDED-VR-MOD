"""Audit scoped native shadow descriptor conversion and normal hook retirement."""
import argparse,json,math
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args()
rows=[]
for f in a.launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
 if not (f.parent/'world-shadow-projection.txt').exists():continue
 for line in f.open(encoding='utf-8'):
  e=json.loads(line)
  if e.get('event','').startswith('shadow_projection_') or e.get('event')=='view_resource_hook_complete':rows.append(e)
samples=[e for e in rows if e['event']=='shadow_projection_converted']
ends=[e for e in rows if e['event']=='shadow_projection_disabled']
complete=[e for e in rows if e['event']=='view_resource_hook_complete']
checks=dict(enabled_once=sum(e['event']=='shadow_projection_enabled' for e in rows)==1,
 no_failure=not any(e['event']=='shadow_projection_failure' for e in rows),
 retired_once=len(ends)==1 and ends[0].get('hook_disabled') is True,
 two_eyes={e.get('eye') for e in samples}=={0,1},
 bounded_scope=bool(samples) and all(e.get('scope') in ('faf4f_private_eye','private_eye') and e.get('source_unchanged') is True for e in samples),
 native_hook_completed=len(complete)==1 and complete[0].get('result')==0,
 ready_eyes_checked=len(ends)==1 and ends[0].get('failures')==0 and len(ends[0].get('ready_checks',[]))==2 and len(ends[0].get('warmup_calls',[]))==2 and all(ends[0]['ready_checks'][i]>=100 and ends[0][key]==ends[0]['ready_checks'][i]+ends[0]['warmup_calls'][i] for i,key in enumerate(['left','right'])))
by=[{e['owner'] for e in samples if e['eye']==eye} for eye in (0,1)]
checks['independent_owners']=all(len(s)==1 for s in by) and by[0].isdisjoint(by[1])
checks['valid_perspective']=bool(samples) and all(len(e['aspect_near_far_fov'])==4 and all(math.isfinite(x) and x>0 for x in e['aspect_near_far_fov']) and e['aspect_near_far_fov'][2]>e['aspect_near_far_fov'][1] and e['aspect_near_far_fov'][3]<math.pi for e in samples)
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,samples=len(samples),retirement=ends,
 limitation='Native input conversion and lifetime only; image quality needs separate inspection.')
(a.launch/'shadow-projection-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
print(json.dumps(r,indent=2));raise SystemExit(int(r['status']!='PASS'))
