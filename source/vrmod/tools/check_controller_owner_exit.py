"""Audit exact native input owner handoff. Neutral controller only; actual
attack-held shutdown, XR/combat joining and re-entry remain unproven.
"""
import argparse,hashlib,json
from pathlib import Path

def audit(rows,source,reader_stop=False):
 ev=lambda name:[r for r in rows if r['event']==name]
 names=['controller_owner_exit_enabled','controller_fixture_connected','controller_fixture_packet_polled','input_ui_observer_removed','controller_owner_exit_wait','controller_backend_exit_handoff','controller_owner_native_original_begin','controller_owner_native_disconnect_begin','controller_owner_native_disconnect_end','controller_owner_native_original_end','controller_backend_exit_returned','controller_owner_exit_removed','controller_owner_exit_complete']
 c={'all_unique_native_stages':all(len(ev(n))==1 for n in names)}
 if not c['all_unique_native_stages']:return c,{'stage_counts':{n:len(ev(n)) for n in names}}
 enabled,connected,polled,ui,wait,hand,begin,db,de,end,returned,removed,complete=[ev(n)[0] for n in names]
 c['strict_stage_order']=all(a['us']<b['us'] for a,b in zip([ev(n)[0] for n in names],[ev(n)[0] for n in names][1:]))
 c['neutral_fixture_no_physical_pad_override']=enabled['neutral_only'] and enabled['system_settings_unchanged'] and connected['synthetic'] and connected['port']==0 and connected['neutral_only'] and connected['all_native_ports_disconnected'] and all(not connected[k] for k in ('navigation_sequence','altar_sequence','motion_sequence','double_jump','glide_sequence')) and polled['buttons']==0
 c['all_owned_backend_identities']=connected['backend']==hand['backend']==begin['backend']==end['backend']==returned['backend'] and hand['manager']==begin['manager']==db['manager']==de['manager'] and begin['device']==db['device']==de['device'] and hand['owner_serial']==returned['owner_serial']>0
 c['one_native_owner_thread']=len({r['thread'] for r in [connected,polled,hand,begin,db,de,end,returned,complete]})==1 and connected['thread']>0
 c['actual_native_callers']=hand['caller']=='0xdacddd' and db['caller']=='0xdac796' and enabled['destroy_rva']=='0xdac6c0' and enabled['disconnect_rva']=='0xdad060'
 c['frontend_stopped_before_exit']=ui['restored'] and ui['callbacks']==ui['read_failures']==0 and wait['ui_hook_removed'] and wait['requires_normal_exit'] and wait['neutral_only'] and wait['native_connected_seen'] and not wait['world_readers_active'] and all(r['us']<ui['us'] for r in ev('input_ui_native_sample'))
 c['exact_table_restored_before_native_original']=hand['pointers_restored'] and hand.get('original_api')==hand.get('restored_api') and len(hand.get('original_api',[]))==3 and all(int(x,16)>0 for x in hand.get('original_api',[])) and hand['native_original_not_called_yet'] and hand['callbacks']==0 and hand['ticket_revoked']
 c['pinned_native_module_and_connected_owner']=hand['module_pinned'] and int(hand['module'],16)>0 and wait['module_pinned_until_process_exit'] and hand['native_connected_before']==1 and not hand['writes_connection_flag'] and not hand['reads_world']
 c['correct_neutral_packet_representation']=hand.get('log_schema')==2 and hand.get('packet_kind')=='buttons' and hand['neutral_packet']==polled['packet'] and hand['neutral_was_polled']
 c['original_native_disconnect_and_return_preserved']=db['read_only'] and de['arguments_unchanged'] and de['return_unchanged'] and end['arguments_unchanged'] and not end['reads_destroyed_memory']
 c['returned_owner_does_not_read_or_write_freed_memory']=returned['native_original_returned'] and returned['pointers_restored_before_original'] and returned['callbacks']==0 and not returned['reads_destroyed_memory'] and not returned['writes_destroyed_memory']
 c['distinct_completion_does_not_invent_polling']=not returned['claims_neutral_poll'] and not returned['claims_polled_disconnect'] and not ev('controller_fixture_disconnected') and not ev('controller_fixture_restored')
 c['all_hooks_drained_and_finalizer_on_owner']=removed['restored'] and removed['callbacks']==removed['failures']==0 and removed['trampolines_retained'] and complete['result']==0 and complete['synchronous_owner_thread'] and not complete['world_reads_after_handoff'] and not complete['claims_xr_combat_exit']
 roots=[r for r in source if r['event']=='world_broker_root_ended']
 c['both_native_world_roots_finished_before_backend']=len(roots)==2 and {r['kind'] for r in roots}=={0,2} and all(r['native_returned'] and r['phase']==3 and r['us']<hand['us'] for r in roots)
 if reader_stop:
  readers=ev('controller_gameplay_readers_stopped')
  c['unique_value_only_reader_stop']=len(readers)==1
  if readers:
   reader=readers[0]
   c['reader_stop_precedes_frontend_removal_and_owner']=polled['us']<reader['us']<ui['us']<hand['us'] and reader['thread']!=hand['thread']
   c['reader_stop_no_native_access_or_fake_completion']=not reader['motion_input'] and reader['neutral_packet']==polled['packet'] and reader['ticket_revoked'] and all(reader.get(k) is False for k in ('reads_native_memory','writes_native_memory','claims_neutral_poll','claims_completion'))
 c['no_failed_observations']=not any('failure' in r['event'] for r in rows)
 return c,{'native_original_us':end['us']-begin['us'],'handoff_to_completion_us':complete['us']-hand['us'],'ui_removed_to_owner_us':hand['us']-ui['us'],'thread':hand['thread'],'device':begin['device'],'scope':__doc__}

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--reader-stop',action='store_true');a=p.parse_args();probe=a.probe
 raw=(probe/'view-resource-probe.jsonl').read_bytes();rows=[json.loads(l) for l in raw.splitlines()]
 provenance=json.loads((probe/'owner-exit-provenance.json').read_text());publisher=Path(provenance['publisher']);sourceRaw=(publisher/'view-resource-probe.jsonl').read_bytes();source=[json.loads(l) for l in sourceRaw.splitlines()]
 c,m=audit(rows,source,a.reader_stop)
 launch=json.loads((probe.parent/'launch.json').read_text(encoding='utf-8-sig'));exitData=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'));run=json.loads((probe/'controller-owner-run.json').read_text())
 c['exact_process_and_publisher_provenance']=publisher.resolve().parent==probe.resolve().parent and launch['pid']==provenance['pid']==exitData['pid'] and launch['creation_filetime']==provenance['creation_filetime']==exitData['creation_filetime'] and hashlib.sha256(sourceRaw[:provenance['source_prefix_bytes']]).hexdigest()==provenance['source_prefix_sha256'] and hashlib.sha256(Path('enshrouded.exe').read_bytes()).hexdigest()==provenance['exe_sha256']
 c['normal_exit_and_real_native_finalizer']=run['status']=='PASS' and run['remote_thread_result']==0 and run['owner_finalizer_completed'] and not run['new_crash_directories'] and exitData['status']=='exited' and exitData['exit_code']==0 and (probe.parent/'validation.log').stat().st_size==0
 if a.negative_tests:
  cases=[('wrong_owner','controller_backend_exit_handoff',{'backend':'0x0'},'all_owned_backend_identities'),('wrong_serial','controller_backend_exit_returned',{'owner_serial':999},'all_owned_backend_identities'),('worker_finalizer','controller_owner_exit_complete',{'thread':0},'one_native_owner_thread'),('wrong_caller','controller_owner_native_disconnect_begin',{'caller':'0x0'},'actual_native_callers'),('late_frontend','input_ui_observer_removed',{'callbacks':1},'frontend_stopped_before_exit'),('table_conflict','controller_backend_exit_handoff',{'pointers_restored':False},'exact_table_restored_before_native_original'),('unload_race','controller_backend_exit_handoff',{'module_pinned':False},'pinned_native_module_and_connected_owner'),('changed_connection','controller_backend_exit_handoff',{'writes_connection_flag':True},'pinned_native_module_and_connected_owner'),('wrong_packet_encoding','controller_backend_exit_handoff',{'packet_kind':'attack'},'correct_neutral_packet_representation'),('invented_poll','controller_backend_exit_returned',{'claims_polled_disconnect':True},'distinct_completion_does_not_invent_polling'),('use_after_free','controller_backend_exit_returned',{'reads_destroyed_memory':True},'returned_owner_does_not_read_or_write_freed_memory'),('missing_native_disconnect','controller_owner_native_disconnect_end',{'return_unchanged':False},'original_native_disconnect_and_return_preserved'),('not_finalized','controller_owner_exit_complete',{'result':1},'all_hooks_drained_and_finalizer_on_owner')]
  for label,event,change,target in cases:
   bad=list(rows);index=next((i for i,r in enumerate(rows) if r['event']==event),None)
   if index is not None:bad[index]=dict(rows[index],**change)
   c['reject_'+label]=index is not None and c.get(target) is True and audit(bad,source,a.reader_stop)[0].get(target) is False
 if a.negative_tests and a.reader_stop:
  for label,change,target in [('late_reader_stop',{'us':10**30},'reader_stop_precedes_frontend_removal_and_owner'),('worker_native_read',{'reads_native_memory':True},'reader_stop_no_native_access_or_fake_completion'),('fake_reader_completion',{'claims_completion':True},'reader_stop_no_native_access_or_fake_completion')]:
   bad=list(rows);index=next((i for i,r in enumerate(rows) if r['event']=='controller_gameplay_readers_stopped'),None)
   if index is not None:bad[index]=dict(rows[index],**change)
   c['reject_'+label]=index is not None and c.get(target) is True and audit(bad,source,True)[0].get(target) is False
  c['reject_missing_reader_stop']=c.get('unique_value_only_reader_stop') is True and audit([r for r in rows if r['event']!='controller_gameplay_readers_stopped'],source,True)[0].get('unique_value_only_reader_stop') is False
 report={'status':'PASS' if all(c.values()) else 'FAIL','checks':c,'metrics':m,'source_sha256':hashlib.sha256(raw).hexdigest(),'publisher_sha256':hashlib.sha256(sourceRaw).hexdigest()}
 (probe/'controller-owner-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(sum(c.values()),'PASS /',sum(not v for v in c.values()),'FAIL');print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
