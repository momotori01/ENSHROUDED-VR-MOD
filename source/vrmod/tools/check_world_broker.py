"""Audit native lifecycle publisher and separate DLL consumer against both raw logs."""
import argparse,copy,hashlib,json,pathlib
from check_world_lifetimes import audit as audit_lifecycle

def audit(publisher,consumer,early_admission=False):
 checks,base=audit_lifecycle(publisher,'epoch')
 checks={'native_'+k:v for k,v in checks.items()}
 def event(rows,name):return [r for r in rows if r.get('event')==name]
 def ok(k,v):checks[k]=bool(v)
 def key(r):return (r['provider'],r['slot'],r['serial'])
 def identity(r):return tuple(r[k] for k in ('world','registry','owner_root','kind'))
 en=event(publisher,'world_broker_enabled');made=event(publisher,'world_broker_created');closing=event(publisher,'world_broker_closing');children=event(publisher,'world_broker_child');ended=event(publisher,'world_broker_root_ended');stop=event(publisher,'world_broker_stopped')
 pin=event(consumer,'world_consumer_pinned');ready=event(consumer,'world_consumer_ready');dead=event(consumer,'world_consumer_ended');release=event(consumer,'world_consumer_released');reject=event(consumer,'world_consumer_old_rejected');done=event(consumer,'world_consumer_complete')
 ok('single_process_provider',len(en)==1 and en[0]['flags']==1 and en[0]['provider']!=0 and en[0].get('single_process_provider') is True and en[0].get('gameplay_connected') is False and en[0]['worlds']==en[0]['closures']==en[0]['leases']==0)
 all_values=made+closing+children+ended+pin+dead+release+reject
 ok('same_provider_across_both_dlls',len(en)==1 and bool(all_values) and all(r['provider']==en[0]['provider'] for r in all_values))
 sets=[made,closing,pin,dead,release,reject]
 maps=[{key(r):r for r in a} for a in sets]
 ok('three_distinct_world_epochs',all(len(a)==len(m)==3 for a,m in zip(sets,maps)) and all(set(m)==set(maps[0]) for m in maps) and {r['kind'] for r in made}=={0,1,2})
 ok('monotonic_publisher_serials',[r['serial'] for r in made]==[1,2,3])
 native_end={r['call_id']:r for r in event(publisher,'world_lifecycle_end')}
 shadow=event(publisher,'world_epoch_created')
 ok('publication_from_native_creation',len(made)==3 and all(r['phase']==1 and r['flags']==1 and r['pins']==0 and r.get('gameplay_connected') is False and any(x['call_id']==r['call_id'] and identity(x)==identity(r) and x['us']<r['us'] for x in shadow) for r in made))
 ok('separate_consumer_ready',len(ready)==1 and ready[0].get('separate_dll') is True and ready[0]['worlds']==3 and 30<=ready[0]['seconds']<=150 and len(pin)==3 and all(r['us']<ready[0]['us'] for r in pin))
 ok('unique_acquisition_handles',len({r['handle'] for r in pin})==3 and all(r['handle']>0 and r['phase']==1 and r['pins']==1 and r.get('ecs_access') is False for r in pin))
 ok('native_close_retains_real_consumer_pin',len(closing)==3 and all(r['phase']==2 and r['pins']==2 and r['flags']==(5 if early_admission else 1) and r['close_handle']>0 for r in closing))
 ok('native_children_use_closed_broker',len(children)==6 and all(r['phase']==2 and r['pins']==2 and any(key(x)==key(r) and identity(x)==identity(r) and x['call_id']==r['parent_call_id'] and x['us']<r['us'] for x in closing) for r in children))
 ok('native_root_end_preserves_external_pin',len(ended)==2 and {r['kind'] for r in ended}=={0,2} and all(r['phase']==3 and r['pins']==1 and r.get('native_returned') is True and r['call_id'] in native_end and native_end[r['call_id']]['us']<r['us'] for r in ended))
 ordered=True
 for k,m in maps[0].items():
  if any(k not in a for a in maps[1:]):ordered=False;continue
  c,p,d,l,r=(a[k] for a in maps[1:])
  ordered &= all(identity(x)==identity(m) for x in (c,p,d,l,r)) and m['us']<p['us']<c['us']<d['us']<l['us']<r['us']
  ordered &= c['call_id'] in native_end and native_end[c['call_id']]['us']<d['us']
  ordered &= l['handle']==r['handle']==p['handle']
 ok('exact_generation_full_order_and_identity',ordered)
 ok('consumer_rejects_new_work_after_end',len(dead)==3 and all(r['phase']==3 and r['pins']==1 and r.get('new_lease_rejected') is True for r in dead))
 ok('last_reference_releases_value',len(release)==3 and all(r['phase']==3 and r['pins']==0 for r in release) and release[-1]['worlds']==release[-1]['leases']==0)
 ok('duplicate_release_and_stale_tokens_rejected',len(reject)==3 and all(r.get('duplicate_release_rejected') is True and r.get('reclaimed_token_rejected') is True for r in reject))
 ok('consumer_completes_before_process_shutdown',len(done)==1 and done[0]['result']==0 and done[0]['worlds_retired']==3 and done[0].get('ecs_access') is False and all(r['us']<done[0]['us'] for r in release+reject))
 ok('publisher_stops_after_values_drain',len(stop)==1 and stop[0]['worlds']==stop[0]['leases']==stop[0]['closures']==stop[0]['flags']==0 and all(r['us']<stop[0]['us'] for r in dead))
 ok('no_broker_or_consumer_faults',not event(publisher,'world_broker_faulted') and not event(consumer,'world_consumer_failure'))
 if early_admission:
  stopped=event(publisher,'world_preclose_admission_closed');observed=event(consumer,'world_consumer_admission_closed')
  ok('three_exact_admission_observations',len(observed)==3 and {r['kind'] for r in observed}=={0,1,2} and all(r['flags']==5 and r['phase'] in (1,2,3) and r['new_lease_rejected'] and not r['ecs_access'] and any(key(r)==key(x) and identity(r)==identity(x) and x['us']<r['us'] for x in stopped) for r in observed))
  ok('server_new_admission_rejected_before_root_destruction',any(r['kind']==0 and r['phase']==1 and any(x['kind']==0 and r['us']<x['us'] for x in closing) for r in observed))
 return checks,dict(worlds=len(made),root_closes=len(ended),consumer_releases=len(release),scope='native value-only shared provider + separate DLL lease consumer; no gameplay writes, same-process re-entry or ECS/GPU pin guarantee')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=pathlib.Path);p.add_argument('consumer',type=pathlib.Path);p.add_argument('--preclose-admission',action='store_true');p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 pubraw=a.publisher/'view-resource-probe.jsonl';conraw=a.consumer/'world-epoch-consumer.jsonl'
 pub=[json.loads(s) for s in pubraw.read_text(encoding='utf-8').splitlines()];con=[json.loads(s) for s in conraw.read_text(encoding='utf-8').splitlines()]
 checks,metrics=audit(pub,con,a.preclose_admission)
 run=json.loads((a.publisher/'world-lifetimes-run.json').read_text(encoding='utf-8'));crun=json.loads((a.consumer/'world-consumer-run.json').read_text(encoding='utf-8'));post=json.loads((a.publisher/'probe-postcheck.json').read_text(encoding='utf-8'));ex=json.loads((a.publisher.parent/'process-exit.json').read_text(encoding='utf-8-sig'))
 checks['both_exports_returned']=run['status']==crun['status']=='PASS' and run['remote_thread_result']==crun['remote_thread_result']==0 and run['observer_completed'] and crun['export_completed']
 checks['normal_exit_no_new_crash']=ex['status']=='exited' and ex['exit_code']==0 and post['status']=='PASS' and post['expected_normal_exit'] and not post['new_crash_directories']
 loads=[json.loads((d/'loader-audit.json').read_text(encoding='utf-8')) for d in (a.publisher,a.consumer)]
 checks['distinct_same_process_dlls']=loads[0]['pid']==loads[1]['pid'] and loads[0]['expected_creation_filetime']==loads[1]['expected_creation_filetime'] and loads[0]['dll']!=loads[1]['dll'] and all(hashlib.sha256(pathlib.Path(x['binary_evidence']['archive']).read_bytes()).hexdigest()==x['binary_evidence']['sha256'] for x in loads)
 if a.preclose_admission:checks['explicit_admission_policies']=all((d/'world-preclose-admission.txt').read_text().strip()=='EVR_WORLD_PRECLOSE_ADMISSION_1' for d in (a.publisher,a.consumer))
 if a.negative_tests:
  qualified=all(checks.values())
  tests=[(0,'world_broker_enabled','single_process_provider',False),(0,'world_broker_created','provider',999),(0,'world_broker_created','serial',2),(0,'world_broker_created','world','0x1'),(0,'world_broker_closing','pins',1),(0,'world_broker_child','phase',1),(0,'world_broker_root_ended','pins',0),(0,'world_broker_root_ended','native_returned',False),(0,'world_broker_stopped','leases',1),(1,'world_consumer_ready','separate_dll',False),(1,'world_consumer_pinned','registry','0x1'),(1,'world_consumer_ended','us',0),(1,'world_consumer_ended','new_lease_rejected',False),(1,'world_consumer_released','handle',999),(1,'world_consumer_old_rejected','duplicate_release_rejected',False),(1,'world_consumer_old_rejected','reclaimed_token_rejected',False),(1,'world_consumer_complete','result',1)]
  if a.preclose_admission:tests.extend([(1,'world_consumer_admission_closed','flags',1),(0,'world_preclose_admission_closed','serial',999)])
  for n,(which,ev,k,v) in enumerate(tests):
   rows=[copy.deepcopy(pub),copy.deepcopy(con)];next(r for r in rows[which] if r['event']==ev)[k]=v
   try:rejected=qualified and not all(audit(*rows,early_admission=a.preclose_admission)[0].values())
   except (KeyError,ValueError,TypeError,IndexError):rejected=qualified
   checks[f'negative_{n:02}_{ev}_{k}']=rejected
 report=dict(checks=checks,metrics=metrics,status='PASS' if all(checks.values()) else 'FAIL',publisher_sha256=hashlib.sha256(pubraw.read_bytes()).hexdigest(),consumer_sha256=hashlib.sha256(conraw.read_bytes()).hexdigest())
 (a.consumer/'world-broker-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(metrics);return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
