"""Separate native shutdown evidence from a complete bounded observer run."""
import argparse,copy,hashlib,json,pathlib,struct

def audit(rows,mode):
 reentry=mode=='reentry';generations=2 if reentry else 1
 if reentry:mode='epoch'
 checks={}
 def ok(name, value):checks[name]=bool(value)
 hooks=[r for r in rows if r.get('event')=='world_lifecycle_hook']
 ok('four_native_hooks',len(hooks)==4 and {int(r['rva'],16) for r in hooks}=={0x898840,0x8b78c0,0x8b9470,0x875180} and all(r.get('read_only') is True for r in hooks))
 starts=[r for r in rows if r.get('event')=='world_lifecycle_observation_begin']
 ok('bounded_read_only',len(starts)==1 and ((300<=starts[0]['seconds']<=420 and starts[0].get('exit_trial') is True) if (starts[0].get('xr_trial') is True or reentry) else 60<=starts[0]['seconds']<=180) and starts[0].get('read_only') is True and starts[0].get('assigns_gameplay_generations') is False)
 begins=[r for r in rows if r.get('event')=='world_lifecycle_begin'];ends=[r for r in rows if r.get('event')=='world_lifecycle_end']
 b={r['call_id']:r for r in begins};e={r['call_id']:r for r in ends}
 ok('unique_complete_call_pairs',len(b)==len(begins)==len(e)==len(ends) and set(b)==set(e))
 ok('no_observer_errors',not any(r.get('event')=='world_lifecycle_observation_failure' for r in rows))
 pairs=[(v,e[k]) for k,v in b.items() if k in e]
 ok('entry_exit_identity',all(all(x.get(k)==y.get(k) for k in ('operation','caller','thread','parent_call_id')) and x['us']<=y['us'] for x,y in pairs))
 ok('preserved_abi_reports',all(y.get('arguments_unchanged') is True and y.get('return_unchanged') is True and y.get('reads_destroyed_memory') is False for x,y in pairs))
 snapshots=[r for r in begins+ends if r.get('snapshot_valid')]+[r for r in rows if r.get('event')=='world_lifecycle_prediction']
 header_ok=True
 for r in snapshots:
  try:
   raw=bytes.fromhex(r['header_hex']);header_ok &= len(raw)==16 and struct.unpack_from('<Q',raw)[0]==int(r['registry'],16) and raw[8]==r['kind'] and r['kind'] in (0,1,2) and r.get('quiescence_proven') is False
  except (KeyError,ValueError,struct.error):header_ok=False
 ok('independent_header_decode',header_ok)
 ok('destroy_completion_saved_values',all(all(x.get(k)==y.get(k) for k in ('root','world','registry','task_list','prediction_world','kind','tick','counter','header_hex')) for x,y in pairs if x['operation']!='create'))
 nesting=True
 for x,y in pairs:
  parent=x['parent_call_id']
  if parent:
   nesting &= parent in b and parent in e
   if parent in b and parent in e:nesting &= b[parent]['operation']=='root_destroy' and b[parent]['thread']==x['thread'] and b[parent]['us']<x['us']<=y['us']<e[parent]['us']
 ok('native_call_nesting',nesting)
 if mode in ('exit','epoch'):
  roots=[x for x in begins if x['operation']=='root_destroy']
  ok('both_root_owners',len(roots)==2*generations and {x['kind'] for x in roots}=={0,2})
  ok('owner_member_identity',all(int(x['world'],16)==int(x['root'],16)+0xcc4218 and int(x['registry'],16)==int(x['root'],16)+0x928 and x['arg0']==x['root'] and x['parent_call_id']==0 and x['caller']==({0:'0xc724d1',2:'0x77d9b8'}.get(x['kind'])) for x in roots))
  cleanup=[x for x in begins if x['operation']=='entity_cleanup'];storage=[x for x in begins if x['operation']=='storage_destroy']
  ok('three_worlds_destroyed',len(cleanup)==len(storage)==3*generations and {x['kind'] for x in cleanup}=={0,1,2} and {x['world'] for x in cleanup}=={x['world'] for x in storage})
  ok('cleanup_before_storage',all(any(y['world']==x['world'] and x['call_id'] in e and e[x['call_id']]['us']<y['us'] and x['parent_call_id']==y['parent_call_id'] for y in storage) for x in cleanup))
  ok('native_child_arguments',all(x['arg1']==x['world'] and x['caller']==('0x8b7aa9' if x['kind']==1 else '0x8b7a89') for x in cleanup) and all(x['arg0']==x['world'] and x['caller']==('0x8b7bb1' if x['kind']==1 else '0x8b7f7d') for x in storage))
  ok('root_owns_each_child',all(any(r['call_id']==x['parent_call_id'] and r['registry']==x['registry'] and x['world']==(r['prediction_world'] if x['kind']==1 else r['world']) for r in roots) for x in cleanup+storage))
  ok('task_destroyed_before_main_storage',all(int(x['task_list'],16)==0 for x in storage if x['kind'] in (0,2)) and all(int(x['task_list'],16)!=0 for x in roots))
  # Exact shutdown-only evidence: does NOT certify observer detach or recreation.
  if mode=='exit':ok('no_recreation_claim',not any(x['operation']=='create' for x in begins))
 if mode in ('detach','create','epoch'):
  removed=[r for r in rows if r.get('event')=='world_lifecycle_removed']
  complete=[r for r in rows if r.get('event')=='world_lifecycle_observation_complete']
  removed_us=removed[0].get('us',0) if len(removed)==1 else 0
  ok('all_hooks_restored_and_callbacks_drained',len(removed)==1 and removed[0].get('disabled') is True and removed[0].get('restored') is True and removed[0].get('trampolines_retained') is True and removed[0].get('callbacks')==removed[0].get('failures')==0)
  ok('bounded_observer_completed',len(complete)==1 and complete[0]['result']==0 and bool(removed) and complete[0]['us']>=removed_us)
  if mode=='epoch':ok('explicit_bounded_exit_trial',len(complete)==1 and len(starts)==1 and starts[0].get('exit_trial') is True and 0<complete[0]['us']-starts[0]['us']<(starts[0]['seconds']+2)*1000000)
  else:ok('observed_full_duration',len(complete)==1 and bool(starts) and complete[0]['us']-starts[0]['us']>=starts[0]['seconds']*1000000)
 elif mode!='exit':raise ValueError('unknown evidence mode')
 if mode in ('create','epoch'):
  created=[x for x in ends if x['operation']=='create'];predicted=[r for r in rows if r.get('event')=='world_lifecycle_prediction']
  ok('native_both_root_creations',len(created)==2*generations and len(begins)==(10*generations if mode=='epoch' else 2) and {x.get('kind') for x in created}=={0,2})
  ok('created_root_member_offsets',all(x['native_result']==x['root'] and int(x['world'],16)==int(x['root'],16)+0xcc4218 and int(x['registry'],16)==int(x['root'],16)+0x928 and int(x['task_list'],16)>0 for x in created))
  ok('create_native_callers',all(x['caller']=={0:'0xc74d02',2:'0x77c208'}.get(x['kind']) and x['parent_call_id']==0 for x in created))
  ok('created_initial_state',all(x['tick']==0 and x['counter']==1023 for x in created+predicted))
  ok('prediction_uses_client_registry',len(predicted)==generations and all(p['kind']==1 and any(x['kind']==2 and x['prediction_world']==p['world'] and x['registry']==p['registry'] and x['task_list']==p['task_list'] and x['call_id']==p['call_id'] for x in created) for p in predicted))
  ok('distinct_native_roots',len(created)==2*generations and all(len({x['root'] for x in created[i:i+2]})==len({x['registry'] for x in created[i:i+2]})==len({x['task_list'] for x in created[i:i+2]})==2 and {x['kind'] for x in created[i:i+2]}=={0,2} for i in range(0,len(created),2)))

 if mode=='epoch':
  enabled=[r for r in rows if r.get('event')=='world_epoch_shadow_enabled']
  issued=[r for r in rows if r.get('event')=='world_epoch_created'];closing=[r for r in rows if r.get('event')=='world_epoch_closing'];retired=[r for r in rows if r.get('event')=='world_epoch_retired'];children=[r for r in rows if r.get('event')=='world_epoch_child']
  key=lambda r:(r['epoch_slot'],r['epoch_serial'])
  ci={key(r):r for r in issued};cl={key(r):r for r in closing};cr={key(r):r for r in retired}
  ok('epoch_diagnostic_not_gameplay',len(enabled)==1 and enabled[0].get('adoption_sealed') is True and enabled[0].get('gameplay_connected') is False and all(r.get('shadow_only') is True for r in issued+closing+retired+children))
  ok('three_unique_complete_epochs',len(issued)==len(closing)==len(retired)==len(ci)==len(cl)==len(cr)==3*generations and set(ci)==set(cl)==set(cr) and len(children)==6*generations)
  ok('monotonic_native_generation_serials',[r['epoch_serial'] for r in issued]==list(range(1,3*generations+1)) and all(0<=r['epoch_slot']<8 for r in issued))
  identity=True
  for k,r in ci.items():
   native=e.get(r['call_id'],{});pred=next((p for p in predicted if p['call_id']==r['call_id']),{})
   expected=pred if r['kind']==1 else native
   identity &= native.get('operation')=='create' and r['owner_root']==native.get('root') and all(r.get(v)==expected.get(v) for v in ('world','registry','kind','header_hex')) and r['us']>=expected.get('us',10**30)
   identity &= k in cl and k in cr
   if k in cl and k in cr:identity &= all(r[v]==cl[k][v]==cr[k][v] for v in ('world','owner_root','kind')) and r['us']<cl[k]['us']<cr[k]['us']
  ok('epoch_binding_from_exact_native_creation',identity)
  ordered=True
  for k,r in cl.items():
   root=b.get(r['call_id'],{});root_end=e.get(r['call_id'],{});death=cr.get(k,{})
   ordered &= root.get('operation')=='root_destroy' and root.get('root')==r['owner_root'] and root.get('thread')==r['thread'] and r.get('pins')==1 and r.get('alive') is False
   ordered &= root.get('us',10**30)<r['us']<root_end.get('us',0)<death.get('us',0)<removed_us
   owned=[x for x in children if key(x)==k]
   ordered &= len(owned)==2 and {x['operation'] for x in owned}=={'entity_cleanup','storage_destroy'} and all(r['us']<x['us']<root_end.get('us',0) and x['parent_call_id']==r['call_id'] for x in owned)
  ok('root_entry_invalidates_before_native_children',ordered)
  ok('child_uses_same_closed_generation',all(key(r) in ci and ci[key(r)]['world']==r['world'] and ci[key(r)]['kind']==r['kind'] and r.get('alive') is False and r['call_id'] in b and r['call_id'] in e and b[r['call_id']]['world']==r['world'] and b[r['call_id']]['us']<r['us']<e[r['call_id']]['us'] for r in children))
  ok('native_return_and_actual_callback_pin_retirement',all(r.get('native_returned') is True and r.get('pin_blocked_early_reclaim') is True for r in retired) and [r['slots_remaining'] for r in retired]==[2,1,0]*generations)


 if reentry:
  gap=[r for r in rows if r['event']=='world_reentry_gap'];bc=[r for r in rows if r['event']=='world_broker_created'];stale=[r for r in rows if r['event']=='world_reentry_stale_token']
  ok('explicit_two_generation_policy',len(starts)==1 and starts[0].get('reentry_trial') is True and starts[0].get('expected_generations')==2 and not starts[0].get('xr_trial'))
  ok('first_generation_drained_without_reset',len(gap)==1 and gap[0].get('created')==3 and gap[0].get('closed_roots')==2 and gap[0].get('epochs_remaining')==0 and gap[0].get('broker_drained') and gap[0].get('provider_retained') and gap[0].get('native_roots_returned') and not gap[0].get('serials_reset',True) and not gap[0].get('claims_VR_reentry',True))
  ok('gap_between_real_native_generations',len(gap)==1 and len(retired)==6 and len(created)==4 and max(r['us'] for r in retired[:3])<gap[0]['us']<min(b[r['call_id']]['us'] for r in created[2:]) and gap[0]['us']<removed_us)
  ok('same_broker_six_distinct_generations',len(bc)==6 and len({r['provider'] for r in bc})==1 and [r['serial'] for r in bc]==list(range(1,7)) and all(r['phase']==1 and r['flags']==1 for r in bc))
  expected={(old['serial'],new['serial']) for old in bc[:3] for new in bc[3:]}
  ok('old_tokens_rejected_after_each_new_world',len(stale)==9 and {(r['old_serial'],r['new_serial']) for r in stale}==expected and all(r['status']==6 and not r['reads_native_memory'] and not r['serials_reset'] and r['old_provider']==r['new_provider']==bc[0]['provider'] and any(x['serial']==r['old_serial'] and x['world']==r['old_world'] and x['slot']==r['old_slot'] for x in bc[:3]) and any(x['serial']==r['new_serial'] and x['world']==r['new_world'] and x['us']<r['us'] for x in bc[3:]) for r in stale))

 return checks,dict(calls=len(begins),kinds=sorted({r.get('kind') for r in snapshots}),scope='two native World generations with ordinary UI reentry; no VR/input restart proof' if reentry else 'same-process native generation/teardown shadow only; no gameplay epoch integration or re-entry' if mode=='epoch' else 'normal process shutdown only; no detach, creation or re-entry proof' if mode=='exit' else 'native initial creation and bounded detach; not same-process re-entry' if mode=='create' else 'bounded observer detach; inspect call count separately from hook success')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('proof',type=pathlib.Path);p.add_argument('--mode',choices=['exit','detach','create','epoch','reentry'],required=True);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 raw=a.proof/'view-resource-probe.jsonl';rows=[json.loads(x) for x in raw.read_text(encoding='utf-8').splitlines()]
 checks,metrics=audit(rows,a.mode)
 exit_data=json.loads((a.proof.parent/'process-exit.json').read_text(encoding='utf-8-sig'))
 checks['normal_game_exit']=exit_data['status']=='exited' and exit_data['exit_code']==0
 post=json.loads((a.proof/'probe-postcheck.json').read_text(encoding='utf-8'))
 checks['no_new_crash_dump']=not post['new_crash_directories']
 if a.mode in ('detach','create'):checks['game_survived_observer']=post['status']=='PASS'
 if a.mode in ('epoch','reentry'):
  run=json.loads((a.proof/'world-lifetimes-run.json').read_text(encoding='utf-8'))
  checks['explicit_exit_runner_complete']=run.get('status')=='PASS' and run.get('remote_thread_result')==0 and run.get('observer_completed') is True and post.get('expected_normal_exit') is True and post.get('status')=='PASS'
 if a.negative_tests:
  mutations=[('missing_hook',lambda r:r.pop(next(i for i,x in enumerate(r) if x['event']=='world_lifecycle_hook'))),('write_mode',lambda r:next(x for x in r if x['event']=='world_lifecycle_observation_begin').update(read_only=False))]
  if a.mode=='exit':
   mutations += [('missing_end',lambda r:r.pop(next(i for i,x in enumerate(r) if x['event']=='world_lifecycle_end')))]
   for name,event,change in [('raw_header','world_lifecycle_begin',dict(header_hex='00'*16)),('wrong_parent','world_lifecycle_end',dict(parent_call_id=123)),('changed_result','world_lifecycle_end',dict(return_unchanged=False)),('freed_read','world_lifecycle_end',dict(reads_destroyed_memory=True)),('saved_identity_changed','world_lifecycle_end',dict(counter=99999)),('wrong_owner','world_lifecycle_begin',dict(registry='0x1234')),('wrong_caller','world_lifecycle_begin',dict(caller='0x0'))]:
    mutations.append((name,lambda r,event=event,change=change:next(x for x in r if x['event']==event).update(**change)))
  else:
   for name,change in [('callbacks_live',dict(callbacks=1)),('bytes_not_restored',dict(restored=False)),('observation_failed',dict(failures=1))]:mutations.append((name,lambda r,change=change:next(x for x in r if x['event']=='world_lifecycle_removed').update(**change)))
  for name,mutate in mutations:
   try:r=copy.deepcopy(rows);mutate(r);c,_=audit(r,a.mode);checks['negative_'+name]=not all(c.values())
   except (KeyError,ValueError,StopIteration,TypeError):checks['negative_'+name]=False
 if a.negative_tests and a.mode=='create':
  for name,event,changes in [('create_return_mismatch','world_lifecycle_end',dict(native_result='0x1')),('prediction_owner_mismatch','world_lifecycle_prediction',dict(registry='0x1')),('wrong_native_create_caller','world_lifecycle_end',dict(caller='0x0')),('creation_not_initial','world_lifecycle_end',dict(counter=9999))]:
   r=copy.deepcopy(rows);next(x for x in r if x['event']==event).update(**changes);checks['negative_'+name]=not all(audit(r,a.mode)[0].values())
 if a.negative_tests and a.mode in ('epoch','reentry'):
  for name,event,changes in [('epoch_alias','world_epoch_created',dict(epoch_serial=2)),('unknown_child_epoch','world_epoch_child',dict(epoch_serial=999)),('live_child','world_epoch_child',dict(alive=True)),('missing_root_pin','world_epoch_closing',dict(pins=0)),('early_retirement','world_epoch_retired',dict(us=0)),('reclaim_without_pin_fence','world_epoch_retired',dict(pin_blocked_early_reclaim=False)),('native_not_returned','world_epoch_retired',dict(native_returned=False)),('source_root_mismatch','world_epoch_created',dict(owner_root='0x1')),('native_world_mismatch','world_epoch_created',dict(world='0x1')),('unsealed_adoption','world_epoch_shadow_enabled',dict(adoption_sealed=False)),('gameplay_scope','world_epoch_shadow_enabled',dict(gameplay_connected=True)),('missing_retirement','world_epoch_retired',dict(slots_remaining=99))]:
   r=copy.deepcopy(rows);next(x for x in r if x['event']==event).update(**changes)
   try:rejected=not all(audit(r,a.mode)[0].values())
   except (KeyError,ValueError,StopIteration,TypeError,IndexError):rejected=True
   checks['negative_'+name]=rejected
 if a.negative_tests and a.mode=='reentry':
  for name,event,changes in [('gap_not_drained','world_reentry_gap',dict(broker_drained=False)),('gap_too_late','world_reentry_gap',dict(us=10**30)),('provider_reset','world_reentry_stale_token',dict(new_provider=999)),('stale_token_accepted','world_reentry_stale_token',dict(status=0)),('stale_native_read','world_reentry_stale_token',dict(reads_native_memory=True))]:
   r=copy.deepcopy(rows);target=next((x for x in r if x['event']==event),None)
   if target is None:checks['negative_'+name]=False;continue
   target.update(**changes);checks['negative_'+name]=not all(audit(r,a.mode)[0].values())
 report=dict(checks=checks,metrics=metrics,status='PASS' if all(checks.values()) else 'FAIL',raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest())
 (a.proof/f'world-lifetimes-{a.mode}-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(metrics);return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
