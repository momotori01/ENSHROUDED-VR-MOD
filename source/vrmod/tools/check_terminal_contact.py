"""Completed native action retained until actual server World Ended. This bridge
is NOT attack-in-flight exit, backend-owner handoff or indefinite-play proof.
"""
import argparse,copy,hashlib,json
from pathlib import Path
from combat_epoch_policy import published,IDENTITY
from check_epoch_admission_cancel import audit as admission_audit
from check_worldless_input_drain import audit as input_audit

# Those earlier checks require ordinary Actor-idle erasure BEFORE World exit.
# Replace only that different lifetime contract with the terminal proof below.
REPLACED={'watch_and_fence_retire_same_epoch','probe_then_normal_world_shutdown',
          'old_event_retained_until_retirement','retire_one_cancel_action',
          'retire_exact_owner_token_cancel_destroy_suppress_and_clear_before_retire'}

def audit(source,rows,allowed,interrupted=False):
 c,m=admission_audit(source,rows,allowed)
 for k in REPLACED:c.pop(k)
 ic,at=input_audit(rows);c.update({'input_'+k:v for k,v in ic.items()})
 ev=lambda name:[r for r in rows if r['event']==name]
 _,server=published(source,live=False)
 exact=lambda r:all(r.get(k)==server[k] for k in IDENTITY)
 stamped=lambda r:r.get('world')==server['world'] and all(r.get(k)==server[v] for k,v in [('epoch_provider','provider'),('epoch_serial','serial'),('epoch_kind','kind'),('epoch_registry','registry'),('epoch_root','owner_root')])
 en=ev('contact_terminal_trial_enabled');held=ev('contact_interrupted_actor_held' if interrupted else 'contact_terminal_retirement_held');wait=ev('contact_terminal_wait');ended=ev('contact_terminal_world_ended');ret=ev('contact_terminal_recycled');done=ev('contact_terminal_complete')
 c['explicit_terminal_scope']=len(en)==1 and en[0]['expected_actions']==1 and en[0]['pairs_before_exit']==600 and en[0]['native_world_exit_required'] and en[0]['hold_after_normal_idle'] and en[0]['active_attack_exit_claim'] is False
 c['unique_terminal_stages']=len(held)==len(wait)==len(ended)==len(ret)==len(done)==1 and not ev('contact_recycled')
 watch=ev('contact_lifetime_watch');db=ev('contact_lifetime_destroy_begin');de=ev('contact_lifetime_destroy_end');sup=[r for r in ev('motion_event_retime') if r['operation']=='suppress'];app=[r for r in ev('motion_event_retime') if r['operation']=='append']
 c['one_actual_native_contact']=len(watch)==len(db)==len(de)==len(sup)==len(app)==1
 c['hold_completed_action_without_ecs_pin']=len(held)==1 and (not held[0]['normal_idle_observed'] and held[0].get('native_actor_returned_absent') and held[0].get('actor_was_active') if interrupted else held[0]['normal_idle_observed']) and held[0]['value_ledgers_retained'] and not held[0]['native_memory_pinned']
 key=lambda r:(r.get('watch_slot'),r.get('watch_serial'),r.get('world'),r.get('entity'))
 c['same_real_action_and_native_destruction']=c['unique_terminal_stages'] and c['one_actual_native_contact'] and all(key(r)==key(watch[0]) and stamped(r) for r in held+ret) and all(key(r)==key(watch[0]) for r in db+de) and all(ret[0][k]==watch[0][k] for k in ('actor','sequence','item','trigger')) and int(db[0]['caller'],16) in allowed and db[0]['present'] and int(db[0]['record'],16)!=0 and not de[0]['present'] and de[0]['record']=='0x0' and de[0]['native_result']==1 and de[0]['arguments_unchanged'] and de[0]['return_unchanged']
 c['hold_after_native_destroy_and_late_suppression']=c['one_actual_native_contact'] and len(held)==1 and app[0]['us']<watch[0]['us']<db[0]['us']<de[0]['us']<held[0]['us'] and app[0]['us']<sup[0]['us']<held[0]['us']
 fixed=ev('controller_fixture_restored');pairs=ev('calibration_pair_recorded')
 c['real_input_restored_and_600_pairs_before_wait']=len(wait)==len(fixed)==1 and len(pairs)==600 and max(r['us'] for r in pairs)<wait[0]['us'] and fixed[0]['us']<wait[0]['us'] and wait[0]['input_restored'] and wait[0]['suppression_retained'] and wait[0]['requires_normal_world_exit']
 roots=[r for r in source if r['event']=='world_broker_root_ended' and r['kind']==0]
 native=[r for r in source if r['event']=='world_lifecycle_end' and roots and r['call_id']==roots[0]['call_id']]
 pins=[r for r in ev('combat_epoch_gate_pinned') if r['kind']==0]
 c['exact_native_root_returned_before_terminal_proof']=len(roots)==len(native)==len(ended)==len(pins)==1 and exact(roots[0]) and roots[0]['native_returned'] and roots[0]['phase']==3 and native[0]['reads_destroyed_memory'] is False and native[0]['us']<roots[0]['us']<=ended[0]['us'] and exact(ended[0]) and ended[0]['phase']==3 and ended[0]['pins']>0 and ended[0]['handle']==0 and ended[0]['flags'] in (1,5) and ended[0]['saved_lease']==pins[0]['handle']>0 and ended[0]['before_consumer_shutdown'] and not ended[0]['reads_ecs']
 c['wait_then_actual_end_then_transaction']=c['unique_terminal_stages'] and held[0]['us']<wait[0]['us']<ended[0]['us']<ret[0]['us']<done[0]['us']
 consumers=('motion_event_selection_removed','motion_collider_apply_drained','motion_collider_lookup_removed','contact_lifetime_removed','contact_quiescence_removed')
 c['consumers_stopped_before_erasure']=len(ended)==len(ret)==len(wait)==1 and all(len(ev(name))==1 and ev(name)[0]['us']<ret[0]['us'] and (ev(name)[0]['us']<wait[0]['us'] if name.startswith('motion_collider_') else ended[0]['us']<ev(name)[0]['us']) for name in consumers)
 c['no_completed_actor_read_after_hold']=len(held)==1 and all(r['us']<held[0]['us'] for r in ev('contact_quiescence_sample'))
 c['terminal_zero_callbacks_and_owned_values_only']=len(ret)==len(done)==1 and ret[0]['native_phase']==3 and ret[0]['lease_pins']>0 and ret[0]['consumer_hooks_stopped'] and ret[0]['callbacks_zero'] and ret[0]['events_after']==ret[0]['colliders_after']==ret[0]['watches_after']==0 and not ret[0]['reads_ecs'] and not ret[0]['writes_gameplay'] and ret[0]['changes_mod_ledgers'] and done[0]['retired']==1 and not done[0]['reads_destroyed_memory'] and not done[0]['active_attack_exit_claim']
 for name in consumers:
  rr=ev(name)
  c['quiescent_'+name]=len(rr)==1 and all(rr[0].get(k,0)==0 for k in ('callbacks','failures','outstanding')) and rr[0].get('restored',True) is True
 release=ev('combat_epoch_gate_released');gate=ev('combat_epoch_gate_complete');complete=ev('view_resource_hook_complete')
 c['terminal_commit_before_release_and_finalizer']=len(done)==len(gate)==len(complete)==1 and len(release)==2 and done[0]['us']<min(r['us'] for r in release)<=max(r['us'] for r in release)<gate[0]['us']<complete[0]['us'] and complete[0]['result']==0
 c['no_late_actor_fence_after_server_preclose']=all(r['us']<min((q['us'] for q in source if q['event']=='world_preclose_admission_closed' and q.get('kind')==0),default=ended[0]['us'] if ended else 0) for r in ev('contact_quiescence_sample'))
 restricted=ev('motion_event_retained_scope')
 c['event_scope_retained_without_camera']=len(restricted)==len(held)==1 and restricted[0]['us']<held[0]['us'] and restricted[0]['native_arguments_only'] and not restricted[0]['reads_camera'] and not restricted[0]['dereferences_saved_actor'] and int(restricted[0]['actor_argument'],16)>0 and int(restricted[0]['native_registry_argument'],16)>0 and stamped(restricted[0]) and c['one_actual_native_contact'] and restricted[0]['entity']==watch[0]['actor'] and all(restricted[0][k]==watch[0][k] for k in ('world','sequence','trigger'))
 origin=ev('motion_origin_frame_snapshot');geometry=ev('motion_collider_shadow')
 c['completed_frame_publication']=len(origin)==1 and all(r['coherent'] and r['origin_valid'] and r['hand_frame']==r['pose_frame']>0 and r['age_limit_ms']==200 and 0<=r['now_ms']-r['hand_ms']<=200 and 0<=r['now_ms']-r['pose_ms']<=200 for r in origin) and bool(geometry) and all(r['hand_frame']==r['frame'] and r['hand_session']==r['session'] and r['hand_origin']==r['origin'] for r in geometry)
 m.update(scope=__doc__,input_stop_us=at,terminal_retired=len(ret),pairs=len(pairs))
 return c,m

def load(probe,publisher=None):
 raw=(probe/'view-resource-probe.jsonl').read_bytes();rows=[json.loads(x) for x in raw.decode().splitlines()]
 provenance=json.loads((probe/'combat-epoch-provenance.json').read_text());publisher=publisher or Path(provenance['publisher']);source_raw=(publisher/'view-resource-probe.jsonl').read_bytes();source=[json.loads(x) for x in source_raw.decode().splitlines()]
 graph=json.loads(Path('docs/analysis/entity_lifetime_destroy/index.json').read_text())
 if graph['exe_sha256']!=hashlib.sha256(Path('enshrouded.exe').read_bytes()).hexdigest():raise ValueError('native caller graph executable mismatch')
 allowed={int(e['site'],16)+5 for e in graph['edges'] if e['target']=='0x8b83b0'}
 return raw,rows,source,allowed,provenance,publisher,source_raw

def verify_files(probe,data):
 raw,rows,source,allowed,provenance,publisher,source_raw=data
 launch=json.loads((probe.parent/'launch.json').read_text(encoding='utf-8-sig'))
 post=json.loads((probe/'probe-postcheck.json').read_text());status=json.loads((probe/'probe-status.json').read_text());ex=json.loads((probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))
 return {'dedicated_terminal_policy':(probe/'world-contact-end-retire.txt').read_text().strip()=='EVR_CONTACT_END_RETIRE_1' and (probe/'world-combat-epoch-cancel.txt').read_text().strip()=='EVR_COMBAT_EPOCH_CANCEL_1',
 'same_native_process_and_archived_source_prefix':publisher.resolve().parent==probe.resolve().parent and Path(provenance['publisher']).resolve()==publisher.resolve() and provenance['pid']==launch['pid'] and provenance['creation_filetime']==launch['creation_filetime'] and provenance['shadow_only'] is False and hashlib.sha256(source_raw[:provenance['source_prefix_bytes']]).hexdigest()==provenance['source_prefix_sha256'],
 'normal_exit_full_finalizer_validation_clean':status['status']==post['status']=='PASS' and status['native_result']==0 and post['terminal_contact_completed'] and post['expected_normal_exit'] and not post['new_crash_directories'] and ex['status']=='exited' and ex['exit_code']==0 and (probe.parent/'validation.log').stat().st_size==0}

def validated_stop(probe,rows):
 data=load(probe);checks,m=audit(data[2],rows,data[3]);checks.update(verify_files(probe,data));r=json.loads((probe/'terminal-contact-checks.json').read_text())
 if not all(checks.values()) or r['status']!='PASS' or not r['checks'] or not all(v is True for v in r['checks'].values()) or r['source_sha256']!=hashlib.sha256(data[0]).hexdigest():raise ValueError('Terminal bridge lacks exact native end/drain/retirement proof')
 return m['input_stop_us']

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=Path);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();data=load(a.probe,a.publisher);raw,rows,source,allowed,*_=data
 c,m=audit(source,rows,allowed);c.update(verify_files(a.probe,data))
 if a.negative_tests:
  cases=[('wrong_epoch','contact_terminal_world_ended',{'serial':999},'exact_native_root_returned_before_terminal_proof'),('unretained_world','contact_terminal_world_ended',{'pins':0},'exact_native_root_returned_before_terminal_proof'),('faulted_world','contact_terminal_world_ended',{'flags':3},'exact_native_root_returned_before_terminal_proof'),('wrong_native_phase','contact_terminal_world_ended',{'phase':2},'exact_native_root_returned_before_terminal_proof'),('early_erasure','contact_terminal_recycled',{'us':0},'wait_then_actual_end_then_transaction'),('nonzero_callbacks','contact_lifetime_removed',{'callbacks':1},'quiescent_contact_lifetime_removed'),('unrestored_hook','contact_quiescence_removed',{'restored':False},'quiescent_contact_quiescence_removed'),('stale_watch','contact_terminal_recycled',{'watch_serial':999},'same_real_action_and_native_destruction'),('event_left','contact_terminal_recycled',{'events_after':1},'terminal_zero_callbacks_and_owned_values_only'),('missing_real_disconnect','controller_fixture_restored',{'native_disconnection_seen':False},'input_owned_backend_restored_and_real_disconnect'),('missing_finalizer','view_resource_hook_complete',{'result':1},'terminal_commit_before_release_and_finalizer'),('late_aim','input_head_walking_sample',{'applied':True,'us':10**29},'input_no_new_vr_aim_after_close')]
  cases.append(('mixed_frame_publication','motion_origin_frame_snapshot',{'hand_frame':0},'completed_frame_publication'))
  for label,event,change,target in cases:
   bad=list(rows);index=next((i for i,r in enumerate(rows) if r['event']==event),None)
   row=None if index is None else dict(rows[index],**change)
   if index is not None:bad[index]=row
   c['reject_'+label]=row is not None and bool(c.get(target)) and audit(source,bad,allowed)[0].get(target) is False
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,source_sha256=hashlib.sha256(raw).hexdigest(),publisher_sha256=hashlib.sha256(data[-1]).hexdigest());(a.probe/'terminal-contact-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',sum(c.values()),'FAIL',sum(not v for v in c.values()));print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
