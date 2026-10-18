"""Finite non-XR native exit ordering evidence; no active VR/GPU retirement proof."""
import argparse,copy,hashlib,json,pathlib
from collections import Counter
from check_world_preclose import audit as preclose_audit

def audit(rows,gpu):
 ev=lambda name:[r for r in rows if r.get('event')==name]
 c={};hooks=ev('world_exit_progress_hook');bounds=ev('world_exit_progress_boundary');removed=ev('world_exit_progress_removed');owners=ev('world_exit_owner_begin');oe=ev('world_exit_owner_end');pc=ev('world_preclose_begin');pe=ev('world_preclose_end');armed=ev('world_exit_progress_armed')
 c['two_exact_read_only_hooks']=len(hooks)==2 and {r['rva'] for r in hooks}=={'0xf7c50','0xc1ba0'} and all(r['read_only'] for r in hooks)
 c['single_bounded_arm']=len(armed)==1 and armed[0]['duration_ms']==5000 and armed[0]['read_only']
 bs={(r['preclose_call'],r['phase']):r for r in bounds}
 c['each_preclose_has_two_boundaries']=len(bounds)==2*len(pc)==len(bs) and all((r['preclose_call'],phase) in bs for r in pc for phase in ('before_preclose','after_preclose'))
 c['boundaries_surround_original']=bool(pc) and all(bs.get((r['preclose_call'],'before_preclose'),{}).get('us',10**30)<r['us'] for r in pc) and all(bs.get((r['preclose_call'],'after_preclose'),{}).get('us',10**30)<r['us'] and r['us']>next(x['us'] for x in pc if x['preclose_call']==r['preclose_call']) for r in pe)
 samples=bounds+removed
 c['render_observer_was_live_before_exit']=bool(samples) and all(r['render_calls']>100 and r['last_view']!='0x0' and r['last_scene']!='0x0' for r in samples)
 c['observed_no_more_3d_render_callbacks']=bool(samples) and len({r['render_calls'] for r in samples})==1 and all(r['render_calls']==r['render_returns'] and r['render_active']==0 for r in samples) and not ev('world_exit_render_begin') and not ev('world_exit_render_end')
 c['owner_pairs_preserve_arguments']=bool(owners) and len(owners)==len(oe) and all(any(all(x[k]==r[k] for k in ('owner_call','thread','caller','address','owner','scene','view')) and x['us']>r['us'] and x['arguments_unchanged'] and not x['reads_destroyed_memory'] for x in oe) for r in owners)
 main=[r for r in owners if r['caller']=='0x77d438']
 roots=[r for r in ev('world_lifecycle_end') if r['operation']=='root_destroy' and r['kind']==2]
 c['main_scene_disposed_after_client_root']=len(main)==len(roots)==1 and main[0]['us']>roots[0]['us'] and main[0]['thread']==roots[0]['thread'] and main[0]['view']==main[0]['last_view'] and main[0]['scene']==main[0]['last_scene']
 c['hook_drained_with_exact_owner_count']=len(removed)==1 and removed[0]['restored'] and removed[0]['owner_calls']==len(owners) and all(x['us']<removed[0]['us'] for x in oe)
 a=[r for r in gpu if r.get('event')=='exit_progress_armed'];calls=[r for r in gpu if r.get('event')=='exit_progress_gpu_call'];beg={r['call']:r for r in calls if r['phase']=='begin'};end={r['call']:r for r in calls if r['phase']=='end'}
 c['layer_optin_and_no_overflow']=len(a)==1 and a[0]['duration_ms']==5000 and a[0]['event_budget']==8192 and a[0]['read_only'] and not any(r.get('event')=='exit_progress_overflow' for r in gpu)
 c['queue_calls_complete_and_contiguous']=bool(beg) and set(beg)==set(end)==set(range(1,len(beg)+1)) and len(calls)==2*len(beg) and all(x['operation']==end[k]['operation'] and x['thread']==end[k]['thread'] and x['object']==end[k]['object'] and x['us']<end[k]['us'] for k,x in beg.items())
 c['successful_queue_returns_not_global_fences']=bool(end) and all(r['result']==0 and not r['gpu_completion_proven'] for r in end.values())
 c['graphics_still_presents_after_scene_disposal']=len(main)==1 and any(r['operation']=='present' and r['us']>main[0]['us'] for r in end.values())
 server=[r for r in pc if r['kind']==0]
 c['graphics_present_during_server_retry']=len(server)>1 and any(r['operation']=='present' and server[0]['us']<r['us']<server[-1]['us'] for r in end.values())
 destroys=[r for r in end.values() if r['operation']=='destroy_device'];waits=[r for r in end.values() if r['operation']=='queue_wait_idle']
 c['native_queue_idle_then_device_destruction']=len(destroys)==1 and len(waits)>=1 and all(w['us']<destroys[0]['us'] for w in waits)
 c['trace_started_before_deletion_and_covers_exit']=bool(a) and bool(pc) and bool(destroys) and a[0]['us']<pc[0]['us']<destroys[0]['us']<a[0]['us']+5000*1000
 return c,dict(render_calls=samples[0]['render_calls'] if samples else None,render_callbacks_during_exit=len(ev('world_exit_render_begin')),queue_return_counts=dict(Counter(r['operation'] for r in end.values())),owner_count=len(owners),client_root_to_main_scene_us=main[0]['us']-roots[0]['us'] if len(main)==len(roots)==1 else None,scope=__doc__)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();raw=(a.publisher/'view-resource-probe.jsonl').read_bytes();gr=(a.publisher.parent/'vulkan.jsonl').read_bytes();rows=[json.loads(l) for l in raw.splitlines()];gpu=[json.loads(l) for l in gr.splitlines()];c,m=audit(rows,gpu)
 old=preclose_audit(rows,True)[0];c['existing_preclose_enforcement']=bool(old) and all(old.values())
 c['explicit_policy']=(a.publisher/'world-exit-progress.txt').read_text().strip()=='EVR_WORLD_EXIT_PROGRESS_1'
 run=json.loads((a.publisher/'world-lifetimes-run.json').read_text());ex=json.loads((a.publisher.parent/'process-exit.json').read_text(encoding='utf-8-sig'))
 c['native_export_and_normal_exit']=run['status']=='PASS' and run['remote_thread_result']==0 and ex['exit_code']==0
 c['validation_clean']=(a.publisher.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  for name,source,event,field,value,target in [
   ('missing_hook','native','world_exit_progress_hook','rva','0x1','two_exact_read_only_hooks'),
   ('no_baseline','native','world_exit_progress_boundary','render_calls',0,'render_observer_was_live_before_exit'),
   ('lost_render','native','world_exit_progress_boundary','render_returns',0,'observed_no_more_3d_render_callbacks'),
   ('owner_wrong_view','native','world_exit_owner_begin','view','0x0','main_scene_disposed_after_client_root'),
   ('owner_early','native','world_exit_owner_begin','us',1,'main_scene_disposed_after_client_root'),
   ('hook_retained','native','world_exit_progress_removed','restored',False,'hook_drained_with_exact_owner_count'),
   ('queue_failed','gpu','exit_progress_gpu_call','result',-1,'successful_queue_returns_not_global_fences'),
   ('queue_missing_pair','gpu','exit_progress_gpu_call','call',999999,'queue_calls_complete_and_contiguous'),
   ('window_short','gpu','exit_progress_armed','duration_ms',1,'layer_optin_and_no_overflow')]:
   rr,gg=copy.deepcopy(rows),copy.deepcopy(gpu);source_rows=rr if source=='native' else gg;r=next(r for r in source_rows if r['event']==event and (event!='exit_progress_gpu_call' or r['phase']=='end'));r[field]=value
   c['reject_'+name]=bool(c.get(target)) and audit(rr,gg)[0].get(target) is False
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,native_sha256=hashlib.sha256(raw).hexdigest(),vulkan_sha256=hashlib.sha256(gr).hexdigest(),limitations=__doc__);(a.publisher/'world-exit-progress-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(json.dumps(m));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
