"""Check that fog projection matching stayed inside private-eye scopes and retired normally."""
import argparse,json,math
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args();rows=[]
for f in a.launch.glob('view-resource-probe-*/world-fog-projection.txt'):
 for line in f.with_name('view-resource-probe.jsonl').open(encoding='utf-8'):
  e=json.loads(line)
  if e.get('event','').startswith('fog_projection_') or e.get('event')=='view_resource_hook_complete':rows.append(e)
samples=[e for e in rows if e['event']=='fog_projection_sample'];ends=[e for e in rows if e['event']=='fog_projection_retired'];completes=[e for e in rows if e['event']=='view_resource_hook_complete'];enabled=[e for e in rows if e['event']=='fog_projection_enabled']
checks=dict(enabled_once=len(enabled)==1,retired_once=len(ends)==1 and ends[0].get('hook_disabled') is True,no_failure=not any(e['event']=='fog_projection_failure' for e in rows),two_eyes={e.get('eye') for e in samples}=={0,1},private_unchanged=bool(samples) and all(e.get('source_unchanged') and e.get('same_private_camera') and e.get('scope')=='192f6d_private_eye' for e in samples),output_matched=bool(samples) and all(e['output_matches_eye']==(e['policy']==2 or (e['policy']==3 and e['frame']>=1300)) for e in samples),native_completed=len(completes)==1 and completes[0].get('result')==0)
checks['balanced_ready']=len(ends)==1 and not ends[0]['failed'] and all(ends[0]['ready_checks'][i]>=100 and ends[0][key]==ends[0]['ready_checks'][i]+ends[0]['warmup_calls'][i] for i,key in enumerate(['left','right']))
checks['comparison_has_both_modes']=not any(e['policy']==3 for e in samples) or {e['output_matches_eye'] for e in samples}=={False,True}
checks['consistent_policy']=len(enabled)==len(ends)==1 and bool(samples) and all(e['policy']==enabled[0]['policy']==ends[0]['policy'] for e in samples)
checks['finite_native_projection']=bool(samples) and all(len(e['native_aspect_near_far_fov'])==4 and all(math.isfinite(v) and v>0 for v in e['native_aspect_near_far_fov']) and e['native_aspect_near_far_fov'][2]>e['native_aspect_near_far_fov'][1] for e in samples)
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,retirement=ends,samples=len(samples),limitation='Scoped CPU descriptor and normal retirement only. GPU reprojection and image quality need separate verification.')
(a.launch/'fog-projection-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8');print(json.dumps(r,indent=2));raise SystemExit(int(r['status']!='PASS'))
