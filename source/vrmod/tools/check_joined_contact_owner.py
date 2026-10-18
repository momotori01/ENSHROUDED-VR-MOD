"""Completed-action 600-pair bridge + real native input owner completion.
Not active attack/renderer shutdown, re-entry, indefinite play or enemy HP proof.
"""
import argparse,hashlib,json
from pathlib import Path
from check_terminal_contact import audit as terminal_audit,load,verify_files
from check_actor_owner_exit import audit as actor_audit,admission_audit
from verify_actor_owner_callers import validate as validate_callers
# Replace only the ordinary polled-disconnect contract. All World/contact/Actor
# checks remain. No raw row is rewritten to pretend this was a polled disconnect.
POLLED_ONLY={'neutral_consumed_before_disconnect','no_late_nonzero_packet','input_ui_skipped_before_backend_restore','input_owned_backend_restored_and_real_disconnect','real_input_restored_and_600_pairs_before_wait'}
def audit(source,rows,allowed,actor_allowed,interrupted=False):
 c,m=terminal_audit(source,rows,allowed,interrupted=interrupted)
 for k in POLLED_ONLY:c.pop(k)
 ac,_=actor_audit(source,rows,actor_allowed,interrupted=interrupted);c.update({'actor_'+k:v for k,v in ac.items()});c.update(admission_audit(source,rows,actor_allowed))
 ev=lambda name:[r for r in rows if r['event']==name]
 names=['contact_owner_exit_enabled','controller_fixture_connected','controller_gameplay_readers_stopped','input_ui_observer_removed','contact_terminal_wait','contact_terminal_complete','combat_epoch_gate_complete','contact_owner_exit_ready','controller_backend_exit_handoff','controller_owner_native_original_begin','controller_owner_native_disconnect_begin','controller_owner_native_disconnect_end','controller_owner_native_original_end','controller_backend_exit_returned','controller_owner_exit_removed','contact_owner_exit_complete','view_resource_hook_complete','controller_owner_exit_complete']
 c['all_joined_stages_unique']=all(len(ev(n))==1 for n in names)
 if not c['all_joined_stages_unique']:return c,m
 enabled,connected,readers,ui,wait,terminal,gate,ready,hand,begin,db,de,end,returned,removed,joined,complete,owner=[ev(n)[0] for n in names]
 c['joined_native_stage_order']=all(a['us']<b['us'] for a,b in zip([ev(n)[0] for n in names],[ev(n)[0] for n in names][1:]))
 c['explicit_completed_action_owner_policy']=enabled['completed_action_only'] and enabled['pairs_before_exit']==600 and enabled['hold_neutral_until_native_owner'] and not enabled['active_attack_exit_claim'] and not connected['neutral_only'] and connected['synthetic'] and connected['all_native_ports_disconnected']
 pairs=ev('calibration_pair_recorded')
 c['600_pairs_then_genuine_input_owner_pending']=len(pairs)==600 and max(r['us'] for r in pairs)<wait['us'] and not wait['input_restored'] and wait['input_owner_pending'] and wait['suppression_retained'] and wait['requires_normal_world_exit']
 c['worker_reader_stop_not_fake_completion']=readers['motion_input'] and readers['ticket_revoked'] and all(readers.get(k) is False for k in ('reads_native_memory','writes_native_memory','claims_neutral_poll','claims_completion')) and readers['thread']!=hand['thread']
 c['frontend_gone_before_backend']=ui['restored'] and ui['callbacks']==ui['read_failures']==0 and all(r['us']<ui['us'] for r in ev('input_ui_native_sample'))
 c['exact_native_input_owner']=connected['backend']==hand['backend']==begin['backend']==end['backend']==returned['backend'] and hand['manager']==begin['manager']==db['manager']==de['manager'] and begin['device']==db['device']==de['device'] and hand['owner_serial']==returned['owner_serial']>0
 c['actual_input_owner_thread_and_callers']=len({r['thread'] for r in (connected,hand,begin,db,de,end,returned,joined,owner)})==1 and connected['thread']>0 and hand['caller']=='0xdacddd' and db['caller']=='0xdac796'
 c['table_restored_before_native_original']=hand['pointers_restored'] and hand['original_api']==hand['restored_api'] and len(hand['original_api'])==3 and all(int(x,16)>0 for x in hand['original_api']) and hand['native_original_not_called_yet'] and hand['callbacks']==0 and hand['ticket_revoked']
 c['pinned_module_no_native_connection_write']=hand['module_pinned'] and int(hand['module'],16)>0 and hand['native_connected_before']==1 and not hand['writes_connection_flag'] and not hand['reads_world']
 stops=ev('controller_world_epoch_stopped');packets=ev('controller_attack_packet_polled')
 c['real_neutral_poll_and_no_new_attack']=len(stops)==1 and hand.get('log_schema')==2 and hand.get('packet_kind')=='attack' and hand['neutral_was_polled'] and hand['neutral_packet']==readers['neutral_packet']==stops[0]['neutral_packet'] and any(r['packet']==hand['neutral_packet'] and not r['buttons'] and not r['right_trigger'] and r['us']<hand['us'] for r in packets) and all(not r['buttons'] and not r['right_trigger'] for r in packets if r['us']>stops[0]['us'])
 c['genuine_native_disconnect_not_polled_disconnect']=db['read_only'] and de['arguments_unchanged'] and de['return_unchanged'] and end['arguments_unchanged'] and not end['reads_destroyed_memory'] and not ev('controller_fixture_disconnected') and not ev('controller_fixture_restored') and not returned['claims_neutral_poll'] and not returned['claims_polled_disconnect']
 c['no_freed_owner_read_or_write']=returned['native_original_returned'] and returned['pointers_restored_before_original'] and returned['callbacks']==0 and not returned['reads_destroyed_memory'] and not returned['writes_destroyed_memory']
 c['all_consumers_ready_before_backend_return']=ready['terminal_retired']==1 and ready['world_leases_released'] and ready['input_owner_pending'] and not ready['reads_ecs'] and gate['retained']==0 and not gate['faulted'] and terminal['retired']==1 and removed['restored'] and removed['callbacks']==removed['failures']==0
 c['joined_owner_synchronous_completion']=joined['result']==owner['result']==complete['result']==0 and joined['terminal_retired']==1 and joined['scene_disposed'] and joined['world_leases_released'] and joined['input_owner_returned'] and joined['synchronous_owner_thread'] and owner['synchronous_owner_thread'] and complete['native_input_owner_thread'] and not joined['reads_destroyed_memory'] and not joined['active_attack_exit_claim'] and not owner['world_reads_after_handoff']
 c['no_failure_events']=not any('failure' in r['event'] for r in rows)
 m.update(scope=__doc__,reader_stop_to_owner_us=hand['us']-readers['us'],world_ready_to_owner_us=hand['us']-ready['us'],owner_finalization_us=owner['us']-hand['us']);return c,m

def evaluated(probe,rows=None):
 data=load(probe);actor_allowed,proof=validate_callers(data[1]);checks,metrics=audit(data[2],rows if rows is not None else data[1],data[3],actor_allowed);checks.update(verify_files(probe,data))
 checks['explicit_owner_policy']=(probe/'world-contact-owner-exit.txt').read_text().strip()=='EVR_CONTACT_OWNER_EXIT_1'
 return checks,metrics,data,actor_allowed,proof

def validated_stop(probe,rows):
 checks,metrics,data,_,_=evaluated(probe,rows);report=json.loads((probe/'joined-contact-owner-checks.json').read_text())
 if not all(checks.values()) or report['status']!='PASS' or not report['checks'] or not all(v is True for v in report['checks'].values()) or report['source_sha256']!=hashlib.sha256(data[0]).hexdigest():raise ValueError('Joined contact/input owner has no complete same-raw native proof')
 return metrics['input_stop_us']

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 c,m,data,actor_allowed,proof=evaluated(a.probe);raw,rows,source,allowed,*_=data
 if a.negative_tests:
  faults=[('wrong_input_owner','controller_backend_exit_handoff',{'backend':'0x0'},'exact_native_input_owner'),('foreign_thread','contact_owner_exit_complete',{'thread':0},'actual_input_owner_thread_and_callers'),('table_conflict','controller_backend_exit_handoff',{'pointers_restored':False},'table_restored_before_native_original'),('unload_race','controller_backend_exit_handoff',{'module_pinned':False},'pinned_module_no_native_connection_write'),('fake_disconnect','controller_backend_exit_returned',{'claims_polled_disconnect':True},'genuine_native_disconnect_not_polled_disconnect'),('read_freed','controller_backend_exit_returned',{'reads_destroyed_memory':True},'no_freed_owner_read_or_write'),('unreleased_world','contact_owner_exit_ready',{'world_leases_released':False},'all_consumers_ready_before_backend_return'),('fake_input_restored','contact_terminal_wait',{'input_restored':True},'600_pairs_then_genuine_input_owner_pending'),('worker_reads_owner','controller_gameplay_readers_stopped',{'reads_native_memory':True},'worker_reader_stop_not_fake_completion'),('late_attack','controller_attack_packet_polled',{'buttons':16384,'us':10**30},'real_neutral_poll_and_no_new_attack'),('wrong_actor_generation','combat_actor_admission_stopped',{'epoch_serial':999},'stop_identifies_actual_actor_entry'),('callback_leak','controller_owner_exit_removed',{'callbacks':1},'all_consumers_ready_before_backend_return'),('failed_join','contact_owner_exit_complete',{'result':1},'joined_owner_synchronous_completion')]
  for label,event,change,target in faults:
   bad=list(rows);index=next((i for i,r in enumerate(rows) if r['event']==event),None)
   if index is not None:bad[index]=dict(rows[index],**change)
   c['reject_'+label]=index is not None and c.get(target) is True and audit(source,bad,allowed,actor_allowed)[0].get(target) is False
 report={'status':'PASS' if all(c.values()) else 'FAIL','checks':c,'metrics':m,'source_sha256':hashlib.sha256(raw).hexdigest(),'publisher_sha256':hashlib.sha256(data[-1]).hexdigest()}
 (a.probe/'joined-contact-owner-checks.json').write_text(json.dumps(report,indent=2)+'\n');(a.probe/'actor-owner-callers.json').write_text(json.dumps(proof,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(sum(c.values()),'PASS /',sum(not v for v in c.values()),'FAIL');print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
