"""Audit finite native combat role shadow separately from actual enforcement."""
import argparse,copy,hashlib,json,pathlib,struct
from combat_epoch_policy import published,IDENTITY
STAGES={'input':(True,True),'pose':(True,False),'event':(False,True),'collider':(True,True),'flush':(False,True)}
def audit(source,rows):
 checks={};ev=lambda n:[r for r in rows if r['event']==n]
 try:c,s=published(source,live=False);checks['native_roles_valid']=True
 except (ValueError,KeyError,struct.error):return {'native_roles_valid':False},{}
 exact=lambda a,b:all(a.get(k)==b.get(k) for k in IDENTITY)
 pins=ev('combat_epoch_pinned');enabled=ev('combat_epoch_shadow_enabled');samples=ev('combat_epoch_shadow');end=ev('combat_epoch_shadow_complete');releases=ev('combat_epoch_released')
 checks['two_exact_unique_leases']=len(pins)==2 and all(exact(a,b) and a['phase']==a['flags']==a['pins']==1 and a['handle']>0 and a['shadow_only'] for a,b in zip(pins,(c,s))) and len({r['handle'] for r in pins})==2
 checks['explicit_read_only_scope']=len(enabled)==1 and enabled[0]['roles']==2 and enabled[0]['stages']==5 and enabled[0]['shadow_only'] and not enabled[0]['ecs_pinned'] and not enabled[0]['atomic_native_permission']
 checks['all_five_native_boundaries']=bool(samples) and {r['stage'] for r in samples}==set(STAGES)
 checks['no_shadow_failures']=not ev('combat_epoch_shadow_failure') and len(end)==1 and end[0]['failures']==0
 checks['every_sample_live_same_generation']=bool(samples) and all(exact(r['client'],c) and exact(r['server'],s) and all(r[role]['phase']==r[role]['flags']==r[role]['pins']==1 for role in ('client','server')) and r['would_permit'] and r['shadow_only'] for r in samples)
 headers=True;ordered=True
 for stage,roles in STAGES.items():
  stage_rows=[r for r in samples if r['stage']==stage]
  ordered &= bool(stage_rows) and [r['sample'] for r in stage_rows]==list(range(1,len(stage_rows)+1)) and len(stage_rows)<=32
  for r in stage_rows:
   for role,needed,expected in zip(('client','server'),roles,(c,s)):
    try:
     raw=bytes.fromhex(r[role+'_header']);observed=int(r['observed_'+role],16)
     headers &= (len(raw)==16 and observed==int(expected['world'],16) and struct.unpack_from('<Q',raw)[0]==int(expected['registry'],16) and raw[8]==expected['kind']) if needed else raw==b'' and observed==0
    except (KeyError,ValueError,struct.error):headers=False
 checks['independent_native_headers_at_each_role']=headers
 checks['bounded_logging_continues_counting']=ordered and len(end)==1 and all(end[0]['stages'].get(stage,0)>=len([r for r in samples if r['stage']==stage])>0 for stage in STAGES)
 checks['same_saved_handles_released']=len(releases)==len(pins)==2 and all(exact(r,p) and r['released_handle']==p['handle'] and r['handle']==p['handle'] and r['pins']==0 and r['phase']==r['flags']==1 for r,p in zip(releases,pins))
 checks['values_drained_before_complete']=len(end)==1 and len(releases)==2 and end[0]['leases_remaining']==0 and end[0]['shadow_only'] and max(r['us'] for r in releases)<end[0]['us']
 closures=[r for r in source if r['event']=='world_broker_closing'];stopped=[r for r in source if r['event']=='world_broker_stopped']
 checks['shadow_released_before_native_shutdown']=len(closures)==3 and {r['kind'] for r in closures}=={0,1,2} and len({r['close_handle'] for r in closures})==2 and len(end)==1 and end[0]['us']<min(r['us'] for r in closures)
 checks['publisher_drained']=len(stopped)==1 and all(stopped[0].get(k)==0 for k in ('worlds','closures','leases','flags'))
 complete=ev('view_resource_hook_complete')
 checks['render_export_completed_after_release']=len(complete)==len(end)==1 and complete[0]['result']==0 and complete[0]['us']>end[0]['us']
 return checks,dict(samples=len(samples),stages=end[0]['stages'] if end else {},scope='read-only native role shadow; gameplay epoch gates and ECS race exclusion remain unproven')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=pathlib.Path);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 read=lambda path:[json.loads(x) for x in path.read_text(encoding='utf-8-sig').splitlines()]
 source=read(a.publisher/'view-resource-probe.jsonl');rows=[r for r in read(a.probe/'view-resource-probe.jsonl') if r['event'].startswith('combat_epoch_') or r['event']=='view_resource_hook_complete'];checks,metrics=audit(source,rows)
 provenance=json.loads((a.probe/'combat-epoch-provenance.json').read_text());launch=json.loads((a.probe.parent/'launch.json').read_text(encoding='utf-8-sig'))
 checks['same_process_and_archived_source_prefix']=a.publisher.resolve().parent==a.probe.resolve().parent and pathlib.Path(provenance['publisher']).resolve()==a.publisher.resolve() and provenance['pid']==launch['pid'] and provenance['creation_filetime']==launch['creation_filetime'] and hashlib.sha256((a.publisher/'view-resource-probe.jsonl').read_bytes()[:provenance['source_prefix_bytes']]).hexdigest()==provenance['source_prefix_sha256']
 exit_data=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'));post=json.loads((a.probe/'probe-postcheck.json').read_text());status=json.loads((a.probe/'probe-status.json').read_text())
 checks['normal_exit_and_survived_probe']=exit_data.get('exit_code')==0 and post['status']=='PASS' and status['status']=='PASS' and not post['new_crash_directories']
 if a.negative_tests:
  mutations=[('wrong_generation','combat_epoch_shadow',lambda r:r['client'].update(serial=999)),('wrong_provider','combat_epoch_shadow',lambda r:r['server'].update(provider=999)),('prediction_role','combat_epoch_shadow',lambda r:r['server'].update(kind=1)),('closed_generation','combat_epoch_shadow',lambda r:r['server'].update(phase=2)),('header_corrupt','combat_epoch_shadow',lambda r:r.update(client_header='00'*16)),('missing_role','combat_epoch_shadow',lambda r:r.update(stage='unknown')),('release_handle','combat_epoch_released',lambda r:r.update(released_handle=999)),('lease_leak','combat_epoch_shadow_complete',lambda r:r.update(leases_remaining=1)),('shadow_failure','combat_epoch_shadow_complete',lambda r:r.update(failures=1)),('missing_stages','combat_epoch_shadow_complete',lambda r:r.update(stages={})),('ecs_pin_claim','combat_epoch_shadow_enabled',lambda r:r.update(ecs_pinned=True))]
  for name,event,change in mutations:
   altered=copy.deepcopy(rows)
   try:change(next(r for r in altered if r['event']==event));checks['negative_'+name]=not all(audit(source,altered)[0].values())
   except (KeyError,StopIteration,TypeError,ValueError):checks['negative_'+name]=False
 report=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,metrics=metrics,source_sha256=hashlib.sha256((a.probe/'view-resource-probe.jsonl').read_bytes()).hexdigest())
 (a.probe/'combat-epoch-shadow-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(metrics);return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
