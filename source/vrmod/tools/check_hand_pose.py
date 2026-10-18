"""Unbound RenderPose lifecycle evidence; not a rendered/controller-hand proof."""
import argparse,copy,json,struct
from pathlib import Path

def check(rows,post):
 def event(name):
  found=[r for r in rows if r.get('event')==name];return found[0] if len(found)==1 else {}
 s=event('hand_pose_source');c=event('hand_pose_created');u=event('hand_pose_updated');r=event('hand_pose_retired');h=event('hand_pose_hook_removed');end=event('hand_pose_complete')
 order=['hand_pose_source','hand_pose_created','hand_pose_updated','hand_pose_retired','hand_pose_hook_removed','hand_pose_complete']
 sig=[r for r in rows if r.get('event')=='hand_pose_signature'];wait=[r for r in rows if r.get('event')=='eye_cpu_tasks_completion']
 return dict(
  native_signatures=len(sig)==4 and {e['rva'] for e in sig}=={'0xf7c50','0xc9b80','0xdd190','0xda1a0'} and all(e.get('matched') is True for e in sig),
  no_failures=not any('failure' in e.get('event','') for e in rows),
  ordered_lifecycle=[e['event'] for e in rows if e['event'] in order]==order,
  idle_CPU_SDF_preserved=bool(wait) and all(e.get('complete') is True and e.get('sdf_results_preserved') is True for e in wait),
  distinct_pose=bool(s.get('handle')) and bool(c.get('handle')) and s['handle']!=c['handle'],
  source_ready=0<s.get('references',0)<2**32-1 and 0<s.get('joints',0)<=256,
  create_accounting=c.get('references_incremented') is True and c.get('live_incremented') is True,
  native_defaults=c.get('defaults_match') is True,
  unbound_scope=s.get('unbound_test') is True and c.get('bound_models')==0 and u.get('bound_models')==0 and end.get('drawn') is False,
  native_joint_update=u.get('native_update') is True and u.get('version_incremented') is True and u.get('delta')==.125,
  original_isolation=u.get('source_unchanged') is True and r.get('source_unchanged') is True,
  generation_retired=r.get('generation_invalidated') is True,
  references_restored=r.get('references_restored') is True,
  live_count_restored=r.get('live_restored') is True,
  hook_restored=h.get('restored') is True and h.get('callbacks')==0,
  complete=end.get('result')==0,
  game_survived=post.get('status')=='PASS' and post.get('expected_process_alive') is True and post.get('rendering_continued_four_seconds') is True and post.get('new_crash_directories')==[])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];post=json.loads((a.probe/'probe-postcheck.json').read_text());checks=check(rows,post)
 source=[r for r in rows if r['event']=='hand_pose_source'];count=source[0]['joints'] if len(source)==1 else 0
 checks['joint_evidence_size']=all((a.probe/n).stat().st_size==count*48 for n in ('pose-original-joints.bin','pose-owned-joints.bin'))
 provenance=json.loads((a.probe/'source.json').read_text());defaults=Path(provenance['hierarchy']).parent/'default-joints.bin'
 expected=bytearray(defaults.read_bytes()[:count*48]);old=struct.unpack_from('<f',expected,16)[0];struct.pack_into('<f',expected,16,old+.125)
 checks['independent_owned_joint_change']=(a.probe/'pose-owned-joints.bin').read_bytes()==expected
 if a.negative_tests:
  for event,key,val in [('hand_pose_created','references_incremented',False),('hand_pose_created','bound_models',1),('hand_pose_updated','source_unchanged',False),('hand_pose_updated','version_incremented',False),('hand_pose_retired','generation_invalidated',False),('hand_pose_retired','references_restored',False),('hand_pose_hook_removed','callbacks',1),('hand_pose_complete','result',4)]:
   bad=copy.deepcopy(rows)
   for e in bad:
    if e['event']==event:e[key]=val
   checks['reject_'+event+'_'+key]=not all(check(bad,post).values())
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},limitation=__doc__)
 (a.probe/'hand-pose-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(checks.values()),'FAIL',sum(not x for x in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
