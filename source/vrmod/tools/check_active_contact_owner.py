"""Active stereo Scene exit joined with completed-action World/input retirement.
NOT attack-in-flight, re-entry, indefinite play, enemy HP or hardware comfort proof.
"""
import argparse,hashlib,json
from pathlib import Path
import check_joined_contact_owner as joined
from check_active_xr_exit import audit as render_audit
from check_motion_hands import check as hands_audit,independent_first_pose,independent_grips
from check_motion_visual import check as weapon_audit

def audit(source,rows,allowed,actor_allowed,xr,mock,gpu,sequence,profile,visuals=True,interrupted=False):
 c,m=joined.audit(source,rows,allowed,actor_allowed,interrupted=interrupted)
 # Those four checks describe the old finite renderer ending before native exit.
 # Replace them with a live-renderer boundary; do not modify any evidence row.
 for k in ('explicit_terminal_scope','explicit_completed_action_owner_policy','600_pairs_then_genuine_input_owner_pending','actor_after_completed_action_and_render_input_cleanup','joined_native_stage_order'):c.pop(k,None)
 ev=lambda name:[r for r in rows if r['event']==name]
 names=('contact_terminal_trial_enabled','contact_owner_exit_enabled','active_contact_owner_exit_armed',('contact_interrupted_actor_held' if interrupted else 'contact_terminal_retirement_held'),'controller_gameplay_readers_stopped','input_ui_observer_removed','world_active_xr_exit_begin','world_active_xr_exit_complete','contact_terminal_wait','contact_terminal_complete','contact_actor_owner_destroy_begin','contact_actor_owner_destroy_end')
 c['active_joined_stages_unique']=all(len(ev(n))==1 for n in names)
 if not c['active_joined_stages_unique'] or not c.get('all_joined_stages_unique'):return c,m
 en,own,armed,held,readers,ui,begin,end,wait,retire,ab,ae=[ev(n)[0] for n in names]
 # Input native destruction and MOD World ledger retirement are independent.
 # Both must finish before the SAME final proof; neither owner waits for the other.
 chains=[('contact_owner_exit_enabled','controller_fixture_connected','controller_gameplay_readers_stopped','input_ui_observer_removed','controller_backend_exit_handoff'),('contact_terminal_wait','contact_terminal_complete','combat_epoch_gate_complete','contact_owner_exit_ready','contact_owner_exit_complete'),('controller_backend_exit_handoff','controller_owner_native_original_begin','controller_owner_native_disconnect_begin','controller_owner_native_disconnect_end','controller_owner_native_original_end','controller_backend_exit_returned','controller_owner_exit_removed','contact_owner_exit_complete','view_resource_hook_complete','controller_owner_exit_complete')]
 c['independent_owners_complete_before_shared_final_proof']=all(all(ev(a)[0]['us']<ev(b)[0]['us'] for a,b in zip(chain,chain[1:])) for chain in chains)
 pairs=ev('live_native_pair_complete')
 c['explicit_completed_action_active_renderer_policy']=en['expected_actions']==1 and en.get('active_renderer') and en['pairs_before_exit']==3000 and en['native_world_exit_required'] and en['hold_after_normal_idle']==(not interrupted) and en.get('interrupted_actor_trial',False)==interrupted and not en['active_attack_exit_claim'] and own['completed_action_only']==(not interrupted) and own.get('interrupted_actor_trial',False)==interrupted and own.get('active_renderer') and own['pairs_before_exit']==3000 and own['hold_neutral_until_native_owner'] and not own['active_attack_exit_claim']
 # Actor, UI hook restoration and Scene disposal have independent owners.
 # Gameplay readers must stop before Actor deletion; UI restoration need only
 # finish before terminal cleanup, without blocking the native owner callback.
 c['reader_stop_before_actor_and_independent_UI_teardown']=((readers['us']<ab['us']<ae['us']<held['us']<begin['us']<end['us']<wait['us']<retire['us'] and readers['us']<ui['us']<armed['us']<wait['us']) if interrupted else held['us']<readers['us']<ui['us']<armed['us']<ab['us']<ae['us']<begin['us']<end['us']<wait['us']<retire['us']) and armed['input_readers_stopped'] and armed['completed_action_held']==(not interrupted) and armed.get('interrupted_trial',False)==interrupted and armed['render_still_active'] and not armed['active_attack_exit_claim']
 keep=ev('input_actor_appearance_only');released=ev('attachment_observer_removed');observed=ev('motion_visual_pose')
 c['fresh_appearance_retained_until_scene_exit']=len(keep)==len(released)==1 and ((keep[0]['us']<readers['us']<ab['us'] and keep[0]['us']<begin['us']<released[0]['us']) if interrupted else keep[0]['us']<armed['us']<begin['us']<released[0]['us']) and keep[0]['attachment_callback_only'] and keep[0]['iterator_hook_retained'] and not keep[0]['gameplay_accepting'] and not keep[0]['saved_actor_dereference'] and (interrupted or sum(armed['us']<r['us']<begin['us'] for r in observed)>=2)
 c['render_continues_after_input_stop_until_actual_scene_exit']=30<=len(pairs)<3000 and (interrupted or sum(armed['us']<r['us']<begin['us'] for r in pairs)>=2) and max(r['us'] for r in pairs)<begin['us'] and not wait['input_restored'] and wait['input_owner_pending'] and wait['suppression_retained'] and wait['requires_normal_world_exit']
 if interrupted:
  c['actual_active_actor_end_replaces_idle']=ab['active'] and ae['active'] and not held['normal_idle_observed'] and held['native_actor_returned_absent'] and held['actor_was_active'] and held['Impact_ended'] and held['late_event_suppressed'] and all(held[k]==ab[k]==ae[k] for k in ('world','sequence','item','trigger','owner_call','epoch_provider','epoch_serial')) and held['actor']==ab['entity'] and not ev('contact_terminal_retirement_held') and not wait['actor_idle_previously_observed'] and wait['actor_interrupted_end_observed']
  scopes=ev('motion_collider_apply_end')+ev('motion_collider_cancel_end')
  c['interrupted_after_last_owned_scope_and_suppression']=bool(scopes) and all(r['us']<ab['us'] and r['query_restored'] and r['original_components_unchanged'] and not r['ecs_written_by_mod'] for r in scopes) and any(r['event']=='motion_event_retime' and r['operation']=='suppress' and r['us']<ab['us'] for r in rows)
  boundaries=ev('contact_actor_native_boundary')
  c['interrupted_owner_value_hold_boundary']=len(boundaries)==1 and boundaries[0]['task_counts_before']==boundaries[0]['task_counts_after']==[0,0,0] and boundaries[0]['stable'] and all(boundaries[0][k]==0 for k in ('selector_callbacks','query_scopes','lookup_callbacks','flush_callbacks')) and (boundaries[0]['destructor_callbacks']>=1 and boundaries[0].get('global_drain_required_before_retire') is True if boundaries[0].get('value_hold_policy')==2 else boundaries[0]['destructor_callbacks']==1)
 if not visuals:return c,m
 rc,rm=render_audit(rows,xr,mock,gpu,source);c.update({'render_'+k:v for k,v in rc.items()})
 hc=hands_audit(rows,active_exit=True);c.update({'hands_'+k:v for k,v in hc.items()})
 wc,wm=weapon_audit(rows,gpu,xr,sequence,'calibrated-prefix',profile,active_exit=True);c.update({'weapon_'+k:v for k,v in wc.items()})
 m.update(scope=('Actual active Actor exit after owned Impact end and late suppression, with native World/input/Scene completion. Still not an outstanding physics-scope exit or reentry proof.' if interrupted else __doc__),renderer=rm,weapon_errors=wm)
 return c,m

def batch_audit(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 a=ev('render_hook_batch_applied');b=ev('render_hook_batch_complete');scene=ev('world_active_xr_exit_complete');gpu=ev('water_deferred_retired');ready=ev('contact_owner_exit_ready');renderer=ev('view_resource_hook_removed')
 c={'unique_batch_stages':len(a)==len(b)==len(scene)==len(gpu)==len(ready)==len(renderer)==1}
 if not c['unique_batch_stages']:return c,{}
 a,b=a[0],b[0];expected={0x39c20,0xd647e0,0x273e0,0x7b680,0x1753a0,0x104760,0xf5bcd,0x10f3a7,0x188320,0x7c750,0x7fd10,0x7ccb0};actual=[int(t,16) for t in a['targets']]
 c['exact_12_render_hooks_only']=len(actual)==len(set(actual))==a['count']==b['count']==12 and set(actual)==expected and a['preserves_combat_and_input_hooks']
 c['after_scene_GPU_and_render_drain']=scene[0]['us']<gpu[0]['us']<renderer[0]['us']<a['us']<b['us']<ready[0]['us'] and a['scene_returned'] and a['render_callbacks']==renderer[0]['callbacks']==0
 removals=[r for r in rows if r['event'] in ('shared_point_lights_hook_removed','shared_stable_frustum_hook_removed','vfx_transparent_render_hook_removed','vfx_transparent_observer_removed','culling_history_hook_removed')]
 c['all_exact_receipts_consumed_and_code_restored']=len(removals)==12 and b['pending']==0 and b['original_bytes_restored'] and b['trampolines_retained'] and a['thread']==b['thread']>0 and all(a['us']<r['us']<b['us'] and r.get('status',0)==0 and r.get('original_bytes_restored',True) for r in removals)
 return c,dict(batch_ms=b['elapsed_ms'],remaining_cleanup_until_ready_us=ready[0]['us']-b['us'])

def retained_filter_audit(rows):
 summaries=[r for r in rows if r['event']=='motion_event_retained_filter_summary']
 c={'unique_drained_filter_summary':len(summaries)==1}
 if len(summaries)!=1:return c,{}
 r=summaries[0]
 c['foreign_calls_skip_sample']=r['calls']>0 and r['foreign_skipped']>0 and 0<=r['fresh_epoch_samples']<r['calls'] and r['calls']==r['fresh_epoch_samples']+r['foreign_skipped']
 c['fresh_generations_no_saved_actor_read']=not r['cached_epoch'] and not r['dereferences_saved_actor'] and r['callbacks']==0
 complete=[x for x in rows if x['event']=='combat_epoch_gate_complete']
 c['fresh_samples_accounted_by_epoch_gate']=len(complete)==1 and complete[0]['checks']['scope']>=r['fresh_epoch_samples'] and r['us']<complete[0]['us']
 return c,{k:r[k] for k in ('calls','fresh_epoch_samples','foreign_skipped')}

def actor_boundary_audit(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 names=('contact_actor_owner_destroy_begin','combat_actor_admission_stopped','contact_actor_native_boundary','contact_actor_owner_destroy_end')
 c={'one_actual_owner_boundary':all(len(ev(n))==1 for n in names)}
 if not c['one_actual_owner_boundary']:return c,{}
 begin,stop,b,end=[ev(n)[0] for n in names]
 c['stopped_then_sampled_before_native_original']=begin['us']<stop['us']<b['us']<end['us'] and b['before_original'] and b['admission_stopped']
 c['exact_native_owner_identity']=all(begin[k]==b[k]==end[k] for k in ('owner_call','watch_slot','watch_serial','world','entity','actor_component','epoch_provider','epoch_serial','thread','caller')) and b['present'] and b['reads_native_callback_owner']
 c['stable_bounded_snapshot']=b['stable'] and b['task_list']!='0x0' and b['task_counts_before']==b['task_counts_after'] and len(b['task_counts_before'])==3 and all(isinstance(v,int) and 0<=v<2**64 for v in b['task_counts_before']) and b['destructor_callbacks']>=1
 c['no_wait_or_false_native_drain_claim']=b['read_only'] and not b['writes_gameplay'] and not b['waited_for_worker'] and not b['claims_native_drain']
 counts={k:b[k] for k in ('selector_callbacks','query_scopes','lookup_callbacks','flush_callbacks','destructor_callbacks')}
 return c,dict(task_counts=b['task_counts_before'],mod_counts=counts,actor_active=b['active'],scope='Instantaneous callback snapshot only; no native drain or attack-in-flight proof.')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--interrupted-actor',action='store_true');a=p.parse_args()
 data=joined.load(a.probe);raw,rows,source,allowed,provenance,publisher,source_raw=data;actor_allowed,proof=joined.validate_callers(rows)
 paths=[a.probe.parent/'native-xr/openxr.jsonl',a.probe.parent/'mock.jsonl',a.probe.parent/'vulkan.jsonl'];extra_raw=[p.read_bytes() for p in paths];extra=[[json.loads(l) for l in b.splitlines()] for b in extra_raw]
 seq_file=a.probe.parent/'mock-hands.csv';profile_file=a.probe/'grip-preview.json';sequence=[[float(v) for v in l.split(',')] for l in seq_file.read_text().splitlines() if l and not l.startswith('#')];profile=json.loads(profile_file.read_text())
 c,m=audit(source,rows,allowed,actor_allowed,*extra,sequence,profile,interrupted=a.interrupted_actor);c.update(joined.verify_files(a.probe,data))
 policies={'world-contact-owner-exit.txt':'EVR_CONTACT_OWNER_EXIT_1','world-active-contact-owner.txt':'EVR_ACTIVE_CONTACT_OWNER_1','world-active-xr-exit.txt':'EVR_WORLD_ACTIVE_XR_EXIT_1'}
 if a.interrupted_actor:policies['world-interrupted-actor-owner.txt']='EVR_INTERRUPTED_ACTOR_OWNER_1'
 c['explicit_active_join_policy']=all((a.probe/k).exists() and (a.probe/k).read_text().strip()==v for k,v in policies.items())
 hand_ok,hand_geometry=independent_first_pose(a.probe,rows);c['independent_hand_skeleton']=hand_ok
 grip_ok,grip_geometry=independent_grips(rows,extra[0],sequence);c['independent_hand_grips']=grip_ok;m.update(hand_geometry=hand_geometry,grip_geometry=grip_geometry)
 c['exact_melee_sequence']=seq_file.read_bytes()==Path('vrmod/test_env/melee-repeat-hands.csv').read_bytes()
 batch_policy=a.probe/'world-render-hook-batch.txt'
 if batch_policy.exists():
  bc,bm=batch_audit(rows);c.update({'batch_'+k:v for k,v in bc.items()});m['batch']=bm;c['explicit_batch_policy']=batch_policy.read_text().strip()=='EVR_RENDER_HOOK_BATCH_1'
 filtered=any(r['event']=='motion_event_retained_filter_summary' for r in rows)
 if filtered:
  fc,fm=retained_filter_audit(rows);c.update({'filter_'+k:v for k,v in fc.items()});m['filter']=fm
 boundary_requested=any(r['event']=='contact_terminal_trial_enabled' and r.get('actor_boundary_observation') for r in rows)
 if boundary_requested:
  bc,bm=actor_boundary_audit(rows);c.update({'actor_boundary_'+k:v for k,v in bc.items()});m['actor_boundary']=bm
 timing_sources=[]
 if a.interrupted_actor:
  from check_attack_exit_request import load as load_timed_exit
  tc,tm,tp,tb,*_=load_timed_exit(a.probe);c.update({'timing_'+k:v for k,v in tc.items()});m['UI_exit_timing']=tm
  timing_sources=[dict(path=str(p),sha256=hashlib.sha256(b).hexdigest()) for p,b in zip(tp[1:],tb[1:])]
 c['publisher_completed_normally']=json.loads((publisher/'world-lifetimes-run.json').read_text())['status']=='PASS'
 if a.negative_tests:
  # All changes are in private copies, never archived raw files.
  faults=[(0,'active_contact_owner_exit_armed',{'us':10**30},'reader_stop_before_actor_and_independent_UI_teardown'),(0,'contact_owner_exit_enabled',{'active_renderer':False},'explicit_completed_action_active_renderer_policy'),(0,'contact_terminal_wait',{'input_restored':True},'render_continues_after_input_stop_until_actual_scene_exit'),(0,'controller_backend_exit_handoff',{'backend':'0x0'},'exact_native_input_owner'),(0,'contact_owner_exit_complete',{'thread':0},'actual_input_owner_thread_and_callers'),(0,'controller_backend_exit_handoff',{'pointers_restored':False},'table_restored_before_native_original'),(0,'controller_backend_exit_handoff',{'module_pinned':False},'pinned_module_no_native_connection_write'),(0,'controller_backend_exit_returned',{'reads_destroyed_memory':True},'no_freed_owner_read_or_write'),(0,'contact_owner_exit_ready',{'world_leases_released':False},'all_consumers_ready_before_backend_return'),(0,'controller_gameplay_readers_stopped',{'reads_native_memory':True},'worker_reader_stop_not_fake_completion'),(0,'controller_attack_packet_polled',{'buttons':16384,'us':10**30},'real_neutral_poll_and_no_new_attack'),(0,'combat_actor_admission_stopped',{'epoch_serial':999},'stop_identifies_actual_actor_entry'),(0,'contact_owner_exit_complete',{'result':1},'joined_owner_synchronous_completion'),(1,'stereo_frame',{'right_draws':0},'render_same_frame_both_eyes_submitted'),(2,'swapchain_destroyed',{'pending_phase':1},'render_mock_swapchains_destroyed_empty'),(0,'motion_visual_disposed',{'reference_released':False},'render_private_hands_and_weapon_released'),(0,'water_deferred_retired',{'us':0},'render_water_retired_after_original_scene_return'),(0,'water_deferred_retired',{'native_serials_modified':True},'render_water_retired_after_original_scene_return'),(3,'present_retirement_poll',{'thread':0},'render_callback_completes_on_original_thread'),(0,'motion_visual_pose',{'same_frame_hand':False},'weapon_pose_uses_same_XR_frame_and_time')]
  faults.extend([(0,'contact_owner_exit_ready',{'us':10**30},'independent_owners_complete_before_shared_final_proof'),(0,'contact_owner_exit_complete',{'us':0},'independent_owners_complete_before_shared_final_proof'),(0,'input_actor_appearance_only',{'gameplay_accepting':True},'fresh_appearance_retained_until_scene_exit')])
  if a.interrupted_actor:faults.extend([(0,'contact_actor_owner_destroy_begin',{'active':False},'actual_active_actor_end_replaces_idle'),(0,'contact_interrupted_actor_held',{'owner_call':999},'actual_active_actor_end_replaces_idle'),(0,'contact_terminal_wait',{'actor_idle_previously_observed':True},'actual_active_actor_end_replaces_idle'),(0,'contact_actor_native_boundary',{'query_scopes':1},'interrupted_owner_value_hold_boundary'),(0,'controller_gameplay_readers_stopped',{'us':10**30},'reader_stop_before_actor_and_independent_UI_teardown'),(0,'input_ui_observer_removed',{'us':10**30},'reader_stop_before_actor_and_independent_UI_teardown'),(0,'attachment_observer_removed',{'us':0},'fresh_appearance_retained_until_scene_exit')])
  renderer_targets={'render_'+k for k in render_audit(rows,*extra,source)[0]}
  for i,(which,event,change,target) in enumerate(faults):
   sets=[rows,*extra];bad=[list(v) for v in sets];idx=next((n for n,r in enumerate(sets[which]) if r['event']==event),None)
   if idx is not None:bad[which][idx]=dict(sets[which][idx],**change)
   if target in renderer_targets:actual=render_audit(bad[0],*bad[1:],source)[0].get(target[7:],True)
   elif target.startswith('weapon_'):actual=weapon_audit(bad[0],bad[3],bad[1],sequence,'calibrated-prefix',profile,active_exit=True)[0].get(target[7:],True)
   else:actual=audit(source,bad[0],allowed,actor_allowed,*bad[1:],sequence,profile,visuals=False,interrupted=a.interrupted_actor)[0].get(target,True)
   c['reject_'+str(i)+'_'+target]=idx is not None and bool(c.get(target)) and not actual
 if a.negative_tests and a.interrupted_actor:
  for n,event,change,target in [('hold_not_drain','contact_actor_native_boundary',{'global_drain_required_before_retire':False},'interrupted_owner_value_hold_boundary'),('owner_missing','contact_actor_native_boundary',{'destructor_callbacks':0},'interrupted_owner_value_hold_boundary'),('erase_while_destroying','contact_lifetime_removed',{'callbacks':1},'quiescent_contact_lifetime_removed')]:
   bad=list(rows);idx=next((i for i,r in enumerate(rows) if r['event']==event),None)
   if idx is not None:bad[idx]=dict(rows[idx],**change)
   if n=='hold_not_drain' and not any(r.get('value_hold_policy')==2 for r in rows):continue
   c['reject_hold_'+n]=idx is not None and bool(c.get(target)) and not audit(source,bad,allowed,actor_allowed,*extra,sequence,profile,visuals=False,interrupted=True)[0].get(target,True)
 if a.negative_tests and batch_policy.exists():
  for n,event,change,target in [('foreign_hook','render_hook_batch_applied',{'targets':['0xdac6c0']*12},'exact_12_render_hooks_only'),('early_disposal','render_hook_batch_applied',{'us':0},'after_scene_GPU_and_render_drain'),('lost_receipt','render_hook_batch_complete',{'pending':1},'all_exact_receipts_consumed_and_code_restored'),('lost_stage','render_hook_batch_applied',{'event':'missing_batch'},'unique_batch_stages')]:
   bad=list(rows);idx=next((i for i,r in enumerate(rows) if r['event']==event),None)
   if idx is not None:bad[idx]=dict(rows[idx],**change)
   c['reject_batch_'+n]=idx is not None and bool(c.get('batch_'+target)) and not batch_audit(bad)[0].get(target,True)
 if a.negative_tests and filtered:
  for name,change,target in [('cached',{'cached_epoch':True},'fresh_generations_no_saved_actor_read'),('saved_actor',{'dereferences_saved_actor':True},'fresh_generations_no_saved_actor_read'),('lost_call',{'calls':0},'foreign_calls_skip_sample'),('undrained',{'callbacks':1},'fresh_generations_no_saved_actor_read')]:
   bad=list(rows);idx=next(i for i,r in enumerate(rows) if r['event']=='motion_event_retained_filter_summary');bad[idx]=dict(rows[idx],**change)
   c['reject_filter_'+name]=bool(c.get('filter_'+target)) and not retained_filter_audit(bad)[0].get(target,True)
 if a.negative_tests and boundary_requested:
  for name,change,target in [('late',{'us':10**30},'stopped_then_sampled_before_native_original'),('foreign',{'entity':0},'exact_native_owner_identity'),('drain_claim',{'claims_native_drain':True},'no_wait_or_false_native_drain_claim'),('unstable',{'stable':False},'stable_bounded_snapshot')]:
   bad=list(rows);idx=next((i for i,r in enumerate(rows) if r['event']=='contact_actor_native_boundary'),None)
   if idx is not None:bad[idx]=dict(rows[idx],**change)
   c['reject_actor_boundary_'+name]=idx is not None and bool(c.get('actor_boundary_'+target)) and not actor_boundary_audit(bad)[0].get(target,True)
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,sources=[dict(path=str(p),sha256=hashlib.sha256(b).hexdigest()) for p,b in zip([a.probe/'view-resource-probe.jsonl',publisher/'view-resource-probe.jsonl',*paths,seq_file,profile_file],[raw,source_raw,*extra_raw,seq_file.read_bytes(),profile_file.read_bytes()])])
 report['sources'].extend(timing_sources)
 (a.probe/'active-contact-owner-checks.json').write_text(json.dumps(report,indent=2)+'\n');(a.probe/'actor-owner-callers.json').write_text(json.dumps(proof,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(sum(bool(v) for v in c.values()),'PASS /',sum(not v for v in c.values()),'FAIL');print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
