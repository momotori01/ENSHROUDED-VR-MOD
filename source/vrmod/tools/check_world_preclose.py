"""Read-only entity deletion before World root teardown; no combat/gpu fence claim."""
import argparse,copy,hashlib,json,pathlib,struct
from check_world_lifetimes import audit as lifecycle_audit

def audit(rows,enforced=False):
 c={};ev=lambda n:[r for r in rows if r['event']==n]
 hooks=ev('world_preclose_hook');starts=ev('world_preclose_begin');ends=ev('world_preclose_end');roots=[r for r in ev('world_lifecycle_begin') if r['operation']=='root_destroy'];creates=ev('world_broker_created')
 b={r['preclose_call']:r for r in starts};e={r['preclose_call']:r for r in ends}
 c['explicit_exact_preclose_hook']=len(hooks)==1 and hooks[0]['rva']=='0x8b6990' and hooks[0]['pattern'].lower()=='48 89 5c 24 10 48 89 74 24 18 55 57 41 54 41 55 41 56' and hooks[0]['read_only']
 c['bounded_unique_pairs']=2<=len(b)==len(e)==len(starts)==len(ends)<=64 and set(b)==set(e)
 c['both_world_owners_observed']={r['kind'] for r in starts}=={0,2} and len(roots)==2
 pairs=[(v,e[k]) for k,v in b.items() if k in e]
 c['same_native_call_identity']=bool(pairs) and all(all(x[k]==y[k] for k in ('preclose_call','caller','thread','world','root','registry','kind','prediction_world')) and x['us']<y['us'] and x['caller']=={0:'0x3f448e',2:'0x327df9'}.get(x['kind']) for x,y in pairs)
 c['preserved_native_result']=bool(ends) and all(r['arguments_unchanged'] and r['return_unchanged'] and r['root_not_freed_by_function'] and r['completed']==bool(int(r['native_result'],16)&255) for r in ends)
 raw=True
 for r in starts+ends:
  h=bytes.fromhex(r['header_hex']);raw &= len(h)==16 and struct.unpack_from('<Q',h)[0]==int(r['registry'],16) and h[8]==r['kind'] and int(r['world'],16)==int(r['root'],16)+0xcc4218 and int(r['registry'],16)==int(r['root'],16)+0x928 and not r['quiescence_proven'] and r['read_only']
 c['independent_header_and_root_members']=bool(starts) and bool(raw)
 c['count_snapshots_linked']=bool(pairs) and all(x['entity_count']==y['before_count'] and x['prediction_count']==y['before_prediction_count'] for x,y in pairs)
 c['completed_when_main_and_prediction_empty']=bool(ends) and all(not r['completed'] or r['entity_count']==r['prediction_count']==0 for r in ends)
 c['actual_entities_removed']=any(r['before_count']>0 and r['entity_count']<r['before_count'] for r in ends)
 c['preclose_finishes_before_matching_root']=all(any(x['root']==root['root'] and x['completed'] and x['thread']==root['thread'] and x['us']<root['us'] for x in ends) and all(x['us']<root['us'] for x in ends if x['root']==root['root']) for root in roots) and len(roots)==2
 c['root_gate_still_alive_during_earlier_deletion']=bool(starts) and all(r['broker']['phase']==1 and r['broker']['flags'] in ((1,5) if enforced else (1,)) and any(all(r['broker'][k]==n[k] for k in ('provider','serial','slot','world','registry','owner_root','kind')) and r['world']==n['world'] and n['us']<r['us'] for n in creates) for r in starts)
 removed=ev('world_lifecycle_removed');complete=ev('world_lifecycle_observation_complete')
 c['all_hooks_drained_before_completion']=len(removed)==len(complete)==1 and removed[0]['callbacks']==removed[0]['failures']==0 and removed[0]['restored'] and removed[0]['disabled'] and complete[0]['result']==0 and all(r['us']<removed[0]['us']<=complete[0]['us'] for r in ends)
 if enforced:
  closed=ev('world_preclose_admission_closed')
  c['each_native_call_closes_admission_first']=bool(starts) and all({x['kind'] for x in closed if x['preclose_call']==r['preclose_call']}==({1,2} if r['kind']==2 else {0}) for r in starts)
  c['same_generation_closed_before_native_delete']=bool(closed) and all(x['phase']==1 and x['flags']==5 and x['changes_mod_admission'] and not x['writes_gameplay'] and x['native_operation_not_started'] and x['preclose_call'] in b and x['preclose_call'] in e and b[x['preclose_call']]['us']<x['us']<e[x['preclose_call']]['us'] and x['owner_root']==b[x['preclose_call']]['root'] and any(all(x[k]==n[k] for k in ('provider','serial','slot','world','registry','owner_root','kind')) for n in creates) for x in closed)
 return c,dict(calls=len(starts),before=[{'kind':r['kind'],'main':r['entity_count'],'prediction':r['prediction_count'],'broker_phase':r['broker']['phase']} for r in starts],after=[{'kind':r['kind'],'main':r['entity_count'],'prediction':r['prediction_count'],'completed':r['completed']} for r in ends])

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=pathlib.Path);p.add_argument('--enforced',action='store_true');p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 path=a.publisher/'view-resource-probe.jsonl';raw=path.read_bytes();rows=[json.loads(l) for l in raw.decode().splitlines()];c,m=audit(rows,a.enforced)
 base=lifecycle_audit(rows,'epoch')[0];c['existing_lifecycle_audit']=bool(base) and all(base.values())
 if a.negative_tests:
  mutations=[('caller','world_preclose_begin','caller','0x1','same_native_call_identity'),('count','world_preclose_end','before_count',-1,'count_snapshots_linked'),('return','world_preclose_end','completed',False,'preserved_native_result'),('early_root','world_preclose_end','us',10**30,'preclose_finishes_before_matching_root'),('memory_claim','world_preclose_end','quiescence_proven',True,'independent_header_and_root_members'),('restoration','world_lifecycle_removed','restored',False,'all_hooks_drained_before_completion')]
  if a.enforced:mutations.append(('admission_not_closed','world_preclose_admission_closed','flags',1,'same_generation_closed_before_native_delete'))
  for name,event,key,value,target in mutations:
   bad=copy.deepcopy(rows);changed=next(r for r in bad if r['event']==event);changed[key]=(not changed[key]) if name=='return' else value
   c['reject_'+name]=bool(c.get(target)) and audit(bad,a.enforced)[0].get(target) is False
 result=json.loads((a.publisher/'world-lifetimes-run.json').read_text());exit_data=json.loads((a.publisher.parent/'process-exit.json').read_text(encoding='utf-8-sig'))
 c['native_export_and_normal_exit']=result['status']=='PASS' and result['observer_completed'] and exit_data['exit_code']==0
 c['validation_clean']=(a.publisher.parent/'validation.log').stat().st_size==0
 if a.enforced:c['explicit_admission_policy']=(a.publisher/'world-preclose-admission.txt').read_text().strip()=='EVR_WORLD_PRECLOSE_ADMISSION_1'
 c['explicit_policy']=(a.publisher/'world-preclose-observe.txt').read_text().strip()=='EVR_WORLD_PRECLOSE_OBSERVE_1'
 static=json.loads(pathlib.Path('docs/analysis/world_preclose_barrier/index.json').read_text());c['same_executable_as_static_analysis']=static['exe_sha256']==result['exe_sha256']==hashlib.sha256(pathlib.Path('enshrouded.exe').read_bytes()).hexdigest()
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,source_sha256=hashlib.sha256(raw).hexdigest(),limit=__doc__);(a.publisher/'world-preclose-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
