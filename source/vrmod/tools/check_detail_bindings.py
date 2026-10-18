"""Audit actual private detail owner substitutions and normal hook retirement."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args()
rows=[]
for f in a.launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
 if not (f.parent/'world-detail-bindings.txt').exists():continue
 for line in f.open(encoding='utf-8'):
  e=json.loads(line)
  if e.get('event','').startswith('detail_binding_') or e.get('event')=='view_resource_hook_complete':rows.append(e)
samples=[e for e in rows if e['event']=='detail_binding_substituted']
ends=[e for e in rows if e['event']=='detail_binding_disabled']
complete=[e for e in rows if e['event']=='view_resource_hook_complete']
checks=dict(enabled_once=sum(e['event']=='detail_binding_enabled' for e in rows)==1,
 no_failure=not any(e['event']=='detail_binding_failure' for e in rows),
 retired_once=len(ends)==1 and ends[0].get('hook_disabled') is True,
 two_eyes={e.get('eye') for e in samples}=={0,1},
 bounded_scope=bool(samples) and all(e.get('scope')=='10acf9_private_eye' and e.get('source_owner_unchanged') is True for e in samples),
 distinct_source=bool(samples) and all(e['source']!=e['private'] and e['source_indirect']!=e['private_indirect'] and e['source_instances']!=e['private_instances'] for e in samples),
 native_hook_completed=len(complete)==1 and complete[0].get('result')==0,
 ready_eyes_checked=len(ends)==1 and ends[0].get('failures')==0 and len(ends[0].get('ready_checks',[]))==2 and len(ends[0].get('warmup_calls',[]))==2 and all(ends[0]['ready_checks'][i]>=100 and ends[0][key]==ends[0]['ready_checks'][i]+ends[0]['warmup_calls'][i] for i,key in enumerate(['left','right'])))
for key in ['private','private_indirect','private_instances']:
 by=[{e[key] for e in samples if e['eye']==eye} for eye in (0,1)]
 checks['independent_'+key]=all(len(s)==1 for s in by) and by[0].isdisjoint(by[1])
r=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,samples=len(samples),retirement=ends,
 limitation='Native hook/owner validation only; GPU bindings and image quality need separate inspection.')
(a.launch/'detail-bindings-check.json').write_text(json.dumps(r,indent=2)+'\n',encoding='utf-8')
print(json.dumps(r,indent=2));raise SystemExit(int(r['status']!='PASS'))
