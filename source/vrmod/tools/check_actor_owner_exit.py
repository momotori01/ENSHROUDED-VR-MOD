"""Read-only native Actor destruction during a completed-action terminal trial.
Optional admission-stop audit verifies the finite watched-Actor gate.
This does not prove attack-in-flight exit or native callback drain.
"""
import argparse,hashlib,json
from pathlib import Path
from check_terminal_contact import load,verify_files,audit as terminal_audit
from combat_epoch_policy import IDENTITY,published
from verify_actor_owner_callers import validate as validate_callers

def audit(source,rows,allowed,interrupted=False):
 ev=lambda n:[r for r in rows if r['event']==n]
 begin=ev('contact_actor_owner_destroy_begin');end=ev('contact_actor_owner_destroy_end')
 watches=ev('contact_lifetime_watch');held=ev('contact_interrupted_actor_held' if interrupted else 'contact_terminal_retirement_held');wait=ev('contact_terminal_wait');removed=ev('contact_lifetime_removed');scope=ev('motion_event_retained_scope')
 _,server=published(source,live=False)
 pre=[r for r in source if r['event']=='world_preclose_admission_closed' and r.get('kind')==0]
 roots=[r for r in source if r['event']=='world_broker_root_ended' and r.get('kind')==0]
 c={}
 c['unique_actor_native_begin_return']=len(begin)==len(end)==len(watches)==len(scope)==len(held)==len(wait)==len(removed)==1
 if not c['unique_actor_native_begin_return']:return c,{'begins':len(begin),'ends':len(end)}
 b,e,w=begin[0],end[0],watches[0]
 stamp=lambda r:r['world']==server['world'] and all(r[k]==server[v] for k,v in [('epoch_provider','provider'),('epoch_serial','serial'),('epoch_kind','kind'),('epoch_registry','registry'),('epoch_root','owner_root')])
 c['exact_actor_and_watch_epoch']=all(stamp(r) for r in (b,e,w)) and b['entity']==e['entity']==w['actor'] and b['impact_entity']==e['impact_entity']==w['entity'] and all(b[k]==e[k]==w[k] for k in ('watch_slot','watch_serial','world'))
 c['actual_native_callback_and_allowed_caller']=b['reads_native_callback_owner'] and int(b['caller'],16) in allowed and b['caller']==e['caller'] and b['thread']==e['thread']>0 and b['reason']==e['reason'] and b['skip_notify']==e['skip_notify']
 c['native_record_present_only_before_original']=b['present'] and int(b['record'],16)>0 and int(b['actor_component'],16)>0 and b['actor_component']==scope[0]['actor_argument'] and not e['after_present'] and e['after_record']=='0x0' and e['native_result']==1 and not e['reads_destroyed_actor']
 c['read_only_no_retirement_or_result_change']=all(r['read_only'] for r in (b,e)) and e['arguments_unchanged'] and e['return_unchanged'] and not e['retires_ledger_entries']
 c['after_completed_action_and_render_input_cleanup']=held[0]['us']<wait[0]['us']<b['us']<e['us']<removed[0]['us']
 c['actor_gone_before_server_preclose']=bool(pre) and all(e['us']<r['us'] and all(r[k]==server[k] for k in IDENTITY) for r in pre)
 c['actor_gone_before_server_ended']=len(roots)==1 and e['us']<roots[0]['us'] and all(roots[0][k]==server[k] for k in IDENTITY)
 absentBegin=ev('contact_actor_absent_destroy_begin');absentEnd=ev('contact_actor_absent_destroy_end')
 c['absent_attempts_return_zero_without_actor_read']=len(roots)==1 and bool(absentBegin) and len(absentBegin)==len(absentEnd) and all(x['owner_call']==y['owner_call'] and x['entity']==y['entity']==b['entity'] and stamp(x) and stamp(y) and int(x['caller'],16) in allowed and x['caller']==y['caller'] and not x['present'] and not x['reads_native_callback_owner'] and x['record']==x['actor_component']=='0x0' and y['native_result']==0 and not y['after_present'] and not y['reads_destroyed_actor'] and y['arguments_unchanged'] and y['return_unchanged'] and not y['retires_ledger_entries'] and e['us']<x['us']<y['us']<roots[0]['us'] for x,y in zip(absentBegin,absentEnd))
 c['all_native_owner_calls_accounted']=removed[0].get('actor_absent_begins')==removed[0].get('actor_absent_ends')==len(absentBegin) and len({r.get('owner_call') for r in begin+absentBegin})==len(begin+absentBegin)
 c['observer_drained_and_no_failed_actor_evidence']=removed[0]['actor_owner_begins']==removed[0]['actor_owner_ends']==1 and removed[0]['callbacks']==removed[0]['failures']==0 and removed[0]['restored'] and not ev('contact_lifetime_failure')
 return c,{'caller':b['caller'],'actor_active_at_entry':b['active'],'native_destructor_us':e['us']-b['us'],'actor_return_to_server_preclose_us':pre[0]['us']-e['us'] if pre else None,'actor_return_to_world_ended_us':roots[0]['us']-e['us'] if roots else None,'thread':b['thread'],'scope':__doc__}

def admission_audit(source,rows,allowed):
 ev=lambda name:[r for r in rows if r['event']==name]
 stop=ev('combat_actor_admission_stopped');begin=ev('contact_actor_owner_destroy_begin');end=ev('contact_actor_owner_destroy_end');retired=ev('contact_world_end_retired')
 c={'one_actor_admission_stop':len(stop)==len(begin)==len(end)==1}
 if not c['one_actor_admission_stop']:return c
 s,b,e=stop[0],begin[0],end[0]
 c['stop_identifies_actual_actor_entry']=all(s.get(k)==b.get(k) for k in ('owner_call','world','entity','thread','epoch_provider','epoch_serial','epoch_kind','epoch_registry','epoch_root')) and int(b['caller'],16) in allowed
 c['stop_precedes_native_actor_return']=b['us']<s['us']<e['us'] and not s.get('native_original_called',True)
 c['actor_stop_closes_all_new_admission']=all(s.get(k) is True for k in ('input_gate_closed','pose_gate_closed','ticket_gate_closed','actor_fence_closed','changes_mod_admission'))
 c['actor_stop_keeps_history_without_claiming_drain']=s.get('server_history_permitted') is True and all(s.get(k) is False for k in ('retires_ledger_entries','inflight_callbacks_drained','ecs_pinned'))
 c['no_later_saved_actor_fence_or_new_append']=not any(r['us']>s['us'] and (r['event']=='contact_quiescence_sample' or (r['event']=='motion_event_retime' and r.get('operation')=='append') or (r['event']=='combat_epoch_gate_check' and r.get('stage') in ('input','poll','ticket','pose','event','append','collider') and r.get('allowed'))) for r in rows)
 return c

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=Path);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--admission-stop',action='store_true');a=p.parse_args()
 data=load(a.probe,a.publisher);raw,rows,source,allowed,*_=data
 actorAllowed,callers=validate_callers(rows);(a.probe/'actor-owner-callers.json').write_text(json.dumps(callers,indent=2)+'\n')
 c,m=audit(source,rows,actorAllowed)
 if a.admission_stop:c.update(admission_audit(source,rows,actorAllowed))
 prior,_=terminal_audit(source,rows,allowed);c['completed_terminal_regression']=bool(prior) and all(prior.values());c.update(verify_files(a.probe,data))
 if a.negative_tests:
  cases=[('foreign_actor','contact_actor_owner_destroy_begin',{'entity':999},'exact_actor_and_watch_epoch'),('stale_epoch','contact_actor_owner_destroy_begin',{'epoch_serial':999},'exact_actor_and_watch_epoch'),('unproven_caller','contact_actor_owner_destroy_begin',{'caller':'0x0'},'actual_native_callback_and_allowed_caller'),('wrong_component','contact_actor_owner_destroy_begin',{'actor_component':'0x0'},'native_record_present_only_before_original'),('not_erased','contact_actor_owner_destroy_end',{'after_present':True},'native_record_present_only_before_original'),('freed_actor_read','contact_actor_owner_destroy_end',{'reads_destroyed_actor':True},'native_record_present_only_before_original'),('result_changed','contact_actor_owner_destroy_end',{'return_unchanged':False},'read_only_no_retirement_or_result_change'),('early_ledger_erasure','contact_actor_owner_destroy_end',{'retires_ledger_entries':True},'read_only_no_retirement_or_result_change'),('late_actor','contact_actor_owner_destroy_end',{'us':10**30},'actor_gone_before_server_preclose'),('undrained_observer','contact_lifetime_removed',{'callbacks':1},'observer_drained_and_no_failed_actor_evidence')]
  cases += [('absent_false_success','contact_actor_absent_destroy_end',{'native_result':1},'absent_attempts_return_zero_without_actor_read'),('absent_actor_read','contact_actor_absent_destroy_begin',{'reads_native_callback_owner':True},'absent_attempts_return_zero_without_actor_read')]
  for label,event,change,target in cases:
   bad=list(rows);index=next((i for i,r in enumerate(rows) if r['event']==event),None)
   if index is not None:bad[index]=dict(rows[index],**change)
   c['reject_'+label]=index is not None and c.get(target) is True and audit(source,bad,actorAllowed)[0].get(target) is False
 if a.negative_tests and a.admission_stop:
  faults=[('foreign_stop',{'entity':999},'stop_identifies_actual_actor_entry'),('stale_stop',{'epoch_serial':999},'stop_identifies_actual_actor_entry'),('wrong_owner_call',{'owner_call':999},'stop_identifies_actual_actor_entry'),('too_late_stop',{'us':10**30},'stop_precedes_native_actor_return'),('still_accepts_pose',{'pose_gate_closed':False},'actor_stop_closes_all_new_admission'),('erased_history',{'retires_ledger_entries':True},'actor_stop_keeps_history_without_claiming_drain'),('false_callback_drain',{'inflight_callbacks_drained':True},'actor_stop_keeps_history_without_claiming_drain')]
  for label,change,target in faults:
   bad=list(rows);index=next((i for i,r in enumerate(rows) if r['event']=='combat_actor_admission_stopped'),None)
   if index is not None:bad[index]=dict(rows[index],**change)
   c['reject_'+label]=index is not None and c.get(target) is True and admission_audit(source,bad,actorAllowed).get(target) is False
  absent=[r for r in rows if r['event']!='combat_actor_admission_stopped']
  c['reject_missing_actor_stop']=c.get('one_actor_admission_stop') is True and admission_audit(source,absent,actorAllowed)['one_actor_admission_stop'] is False
 report={'status':'PASS' if all(c.values()) else 'FAIL','checks':c,'metrics':m,'source_sha256':hashlib.sha256(raw).hexdigest(),'publisher_sha256':hashlib.sha256(data[-1]).hexdigest()}
 (a.probe/'actor-owner-exit-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(c.values()),'FAIL',sum(not v for v in c.values()));print(m)
 return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
