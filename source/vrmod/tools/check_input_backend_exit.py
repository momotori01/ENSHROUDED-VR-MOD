"""Native read-only input backend destruction and synchronous diagnostic detach.
No synthetic controller; does not prove a held combat packet or ECS teardown.
"""
import argparse,copy,hashlib,json
from pathlib import Path

def audit(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 h=ev('input_backend_exit_hook');b=ev('input_backend_exit_begin');e=ev('input_backend_exit_end');removed=ev('world_lifecycle_removed');done=ev('world_lifecycle_observation_complete');roots=[r for r in ev('world_lifecycle_end') if r['operation']=='root_destroy'];scenes=ev('world_exit_owner_end');stop=ev('world_broker_stopped')
 c={}
 c['exact_read_only_hook']=len(h)==1 and h[0]['rva']=='0xdac6c0' and h[0]['read_only']
 c['one_native_backend']=len(b)==len(e)==1
 c['exact_native_arguments_and_snapshot']=c['one_native_backend'] and b[0]['caller']=='0xdacddd' and b[0]['call']==1 and all(b[0][k]==e[0][k] for k in ('call','thread','caller','allocator','backend','manager','module','api','connected')) and all(int(b[0][k],16)>0 for k in ('allocator','backend','manager','module')) and len(b[0]['api'])==3 and all(int(x,16)>0 for x in b[0]['api']) and b[0]['connected']==[0,0,0,0] and b[0]['read_only'] and e[0]['arguments_unchanged'] and not e[0]['reads_destroyed_memory'] and b[0]['us']<e[0]['us']
 c['both_worlds_already_returned']=c['one_native_backend'] and len(roots)==2 and {r['kind'] for r in roots}=={0,2} and all(r['us']<b[0]['us'] for r in roots) and next(r for r in roots if r['kind']==2)['thread']==b[0]['thread']
 c['scene_owners_already_returned']=c['one_native_backend'] and len(scenes)==2 and all(s['us']<b[0]['us'] and not s['reads_destroyed_memory'] for s in scenes)
 c['synchronous_after_native_return']=c['one_native_backend'] and len(done)==1 and done[0]['result']==0 and done[0].get('synchronous_exit_finalization') is True and done[0].get('native_input_owner_returned') is True and done[0].get('code_and_trampolines_retained') is True and done[0]['thread']==e[0]['thread'] and e[0]['us']<done[0]['us']
 c['all_hooks_detached_after_owner_return']=c['one_native_backend'] and len(removed)==1 and removed[0]['disabled'] and removed[0]['restored'] and removed[0]['callbacks']==removed[0]['failures']==0 and e[0]['us']<removed[0]['us'] and bool(done) and removed[0]['us']<done[0]['us']
 c['broker_released_before_completion']=len(stop)==len(done)==1 and all(stop[0][k]==0 for k in ('worlds','closures','leases','flags')) and stop[0]['us']<done[0]['us']
 c['no_failure']=not any('failure' in r['event'] or 'faulted' in r['event'] for r in rows)
 m=dict(scope=__doc__)
 if c['one_native_backend']:
  m.update(native_duration_us=e[0]['us']-b[0]['us'],root_to_input_us={str(r['kind']):b[0]['us']-r['us'] for r in roots},completion_us=done[0]['us']-e[0]['us'] if done else None)
 return c,m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 raw=(a.publisher/'view-resource-probe.jsonl').read_bytes();rows=[json.loads(x) for x in raw.splitlines()];c,m=audit(rows)
 c['explicit_policy']=(a.publisher/'input-backend-exit.txt').read_text().strip()=='EVR_INPUT_BACKEND_EXIT_1'
 c['normal_exit']=json.loads((a.publisher.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['validation_clean']=(a.publisher.parent/'validation.log').stat().st_size==0
 run=json.loads((a.publisher/'world-lifetimes-run.json').read_text());c['observer_run_completed']=run['status']=='PASS' and run['observer_completed']
 if a.negative_tests:
  for label,event,change,target in [('wrong_caller','input_backend_exit_begin',{'caller':'0x1'},'exact_native_arguments_and_snapshot'),('use_after_free','input_backend_exit_end',{'reads_destroyed_memory':True},'exact_native_arguments_and_snapshot'),('early_return','input_backend_exit_end',{'us':1},'exact_native_arguments_and_snapshot'),('async_finish','world_lifecycle_observation_complete',{'synchronous_exit_finalization':False},'synchronous_after_native_return'),('foreign_thread','world_lifecycle_observation_complete',{'thread':1},'synchronous_after_native_return'),('callbacks_live','world_lifecycle_removed',{'callbacks':1},'all_hooks_detached_after_owner_return'),('broker_leak','world_broker_stopped',{'leases':1},'broker_released_before_completion')]:
   bad=copy.deepcopy(rows);matches=[r for r in bad if r['event']==event]
   if matches:matches[0].update(change)
   c['negative_'+label]=bool(c.get(target)) and bool(matches) and audit(bad)[0].get(target) is False
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,source_sha256=hashlib.sha256(raw).hexdigest());(a.publisher/'input-backend-exit-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
