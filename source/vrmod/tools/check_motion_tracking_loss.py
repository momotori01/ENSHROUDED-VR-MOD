"""Independent audit of one actual mock-runtime tracking loss during a native attack.
Does not establish target damage or production multi-attack lifecycle.
"""
import argparse,copy,json,pathlib,struct
from check_motion_collider_apply import physics_transform
from check_action_sequences import decode

def audit(rows,sequence,offhand_release=False):
 ev=lambda n:[r for r in rows if r['event']==n]
 lost={int(r[0]) for r in sequence if (r[0]>440 and not r[11])} if offhand_release else {int(r[0]) for r in sequence if not r[26]};first=min(lost);last=max(lost)
 flags=ev('motion_tracking_sample');abort=ev('motion_offhand_release_abort' if offhand_release else 'motion_tracking_loss_abort');starts=[r for r in ev('motion_attack_sample') if r['gesture_request']]
 pairs=ev('live_native_pair_complete');serials={r['serial']:r['frame'] for r in pairs};eyes=ev('head_visibility_payload')
 weapon=ev('motion_visual_pose');hands=ev('motion_hand_pose');restore=ev('controller_fixture_restored')
 begin=ev('motion_collider_apply_begin');end=ev('motion_collider_apply_end');cancel=ev('motion_collider_cancel_begin');cancelEnd=ev('motion_collider_cancel_end');over=ev('motion_collider_lookup_suppressed')
 native={r['ready_sample']:r for r in ev('sequence_collider_sample') if r.get('ready_sample')};shadow={r['ready_sample']:r for r in ev('motion_collider_shadow')}
 app=[r for r in ev('motion_event_retime') if r['operation']=='append'];sup=[r for r in ev('motion_event_retime') if r['operation']=='suppress'];block=ev('motion_event_blocked')
 c=dict(explicit_runtime_loss_policy=len(ev('motion_tracking_loss_expected'))==1 and ev('motion_tracking_loss_expected')[0]['runtime_tracking_flags'] and ev('motion_tracking_loss_expected')[0]['no_forced_collider_counter'],
  one_swing_request=len(starts)==1 and starts[0]['request']==starts[0]['gesture_request']==1 and starts[0]['frame']==440,
  actual_loss_and_recovery_flags=len(flags)==3 and [r['tracked'] for r in flags]==[True,False,True] and flags[1]['frame']==first and flags[2]['frame']==last+1 and flags[1]['grip_flags']==0 and all(r['focused'] and r['grip_active'] for r in flags),
  abort_releases_and_revokes=len(abort)==1 and abort[0]['expected'] and abort[0]['request']==1 and abort[0]['frame'] in lost and abort[0]['ticket_revoked'] and abort[0]['pulse_released'] and not abort[0]['right_tracked'],
  one_native_press_release=[r['buttons'] for r in ev('controller_attack_packet_polled')]==[0,16384,0],
  original_input_restored=len(restore)==1 and restore[0]['pointers_restored'] and restore[0]['native_connection_seen'] and restore[0]['native_disconnection_seen'] and not restore[0]['native_connected'] and not restore[0]['rescan_pending'],
  no_restart_after_recovery=bool(abort) and not any(r['gesture_request'] for r in ev('motion_attack_sample') if r['us']>abort[0]['us']),
  six_hundred_same_frame_pairs=len(pairs)==600 and len(serials)==600,
  no_stale_weapon_or_hands=bool(weapon and hands) and not any(r['frame'] in lost for r in weapon+hands),
  models_resume_after_tracking=any(r['frame']==last+1 for r in weapon) and any(r['frame']==last+1 for r in hands),
  same_eye_visibility=all(all(len([r for r in eyes if r['serial']==s and r['eye']==eye])==2 for eye in (0,1)) for s in serials),
  lost_tracking_hides_owned_models=all(r['hand_instance']==r['extra_instance']==2**32-1 for r in eyes if serials.get(r['serial']) in lost) and sum(serials.get(r['serial']) in lost for r in eyes)==4*len(lost),
  restored_tracking_shows_models=all(r['hand_instance']!=2**32-1 and r['extra_instance']!=2**32-1 for r in eyes if serials.get(r['serial'],0)>last),
  source_models_unchanged=all(r['source_unchanged'] for r in weapon+hands),
  no_failure_events=not any('failure' in r['event'] or 'failed' in r['event'] for r in rows),
  exactly_one_authorized_event=len(app)+len(block)==1 and len(sup)==1,
  original_actor_and_assets_unchanged=all(r['actor_and_storage_unchanged'] and not r['asset_written'] for r in app+sup),
 )
 if offhand_release:
  for key in ('explicit_runtime_loss_policy','actual_loss_and_recovery_flags','abort_releases_and_revokes','no_stale_weapon_or_hands','models_resume_after_tracking','lost_tracking_hides_owned_models','restored_tracking_shows_models'):c.pop(key)
  support=ev('motion_support_sample');policy=ev('motion_offhand_release_expected')
  c['explicit_runtime_grab_release']=len(policy)==1 and policy[0]['runtime_grab_action'] and policy[0]['no_forced_collider_counter'] and not ev('motion_tracking_loss_expected')
  c['actual_grab_transitions']=len(support)==4 and [r['left_grab'] for r in support]==[False,True,False,True] and [r['frame'] for r in support][1:]==[415,first,last+1] and all(r['left_tracked'] and r['right_tracked'] and r['left_grab_active'] and r['focused'] for r in support)
  c['tracking_never_lost']=len(flags)==1 and flags[0]['tracked'] and flags[0]['focused'] and flags[0]['grip_active'] and all(r[12]==r[13]==r[25]==r[26]==r[27]==1 and r[28]==0 for r in sequence)
  c['offhand_abort_revokes_releases']=len(abort)==1 and abort[0]['expected'] and abort[0]['request']==1 and abort[0]['frame'] in lost and abort[0]['ticket_revoked'] and abort[0]['pulse_released'] and abort[0]['right_tracked'] and abort[0]['left_tracked'] and not abort[0]['left_grab']
  c['released_hand_weapon_remain_rendered']=all(any(r['frame']==f for r in hands) and any(r['frame']==f for r in weapon) for f in lost) and all(r['hand_instance']!=2**32-1 and r['extra_instance']!=2**32-1 for r in eyes if serials.get(r['serial']) in lost) and sum(serials.get(r['serial']) in lost for r in eyes)==4*len(lost)
  c['regrip_keeps_models']=any(r['frame']==last+1 for r in hands) and any(r['frame']==last+1 for r in weapon)
 decoded=[decode(bytes.fromhex(r['base_actor_bytes'])) for r in ev('action_sequence_snapshot')];active=[r for r in decoded if r['state']&1]
 c['native_action_remains_one']=bool(active) and len({(r['trigger_sequenceId'],r['trigger_triggerTime']) for r in active})==1
 c['native_action_finishes']=bool(active) and len(decoded)>=2 and not decoded[-1]['state']&1
 if block:
  release=ev('controller_attack_release_confirmed');disconnect=ev('controller_fixture_disconnected');packets=ev('controller_attack_packet_polled')
  c['neutral_consumed_before_disconnect']=len(release)==len(disconnect)==1 and len(packets)==3 and release[0]['release_before_disconnect'] and release[0]['neutral_packet']==release[0]['observed_packet']==packets[-1]['packet'] and packets[-1]['buttons']==0 and packets[-1]['us']<release[0]['us']<disconnect[0]['us']
  selection=ev('motion_event_selection')
  c['blocked_identity_matches_native_actor']=len(block)==1 and any(all(r[k]==block[0][k] for k in ('world','entity','item','sequence','trigger')) and r['range'][0]==0 and r['actor_unchanged'] for r in selection)
  c['no_impact_after_early_loss']=not begin and not cancel and not ev('motion_owned_vm') and not ev('motion_owned_damage_callback') and not shadow
  c['early_block_is_owned_and_latched']=len(block)==1 and block[0]['ticket_identified'] and block[0]['latched'] and not block[0]['origin_valid'] and not block[0]['asset_written']
  c['later_authored_event_suppressed']=len(sup)==1 and block[0]['variant'] in sup[0]['before'] and sup[0]['after']==[v for v in sup[0]['before'] if v!=block[0]['variant']] and len(sup[0]['before'])-len(sup[0]['after'])==1 and sup[0]['us']>block[0]['us']
 else:
  c['cancellation_observed']=len(cancel)==len(cancelEnd)==len(over)>=1
  c['live_loss_not_forced_counter']=bool(cancel) and not any(r['forced_fixture'] for r in cancel) and not cancel[0]['pose_eligible']
  c['no_colliders_after_lost_pose']=bool(cancel) and all(r['us']<cancel[0]['us'] for r in begin)
  c['retimed_event_suppressed']=len(app)==len(sup)==1 and app[0]['after'][-1] in sup[0]['before'] and app[0]['after'][-1] not in sup[0]['after']
  c['initial_or_adopted_native_cancellation']=bool(cancel) and ((bool(end) and cancel[0]['collider_before']==end[-1]['collider_after']!=0) or (not end and cancel[0]['collider_before']==0 and cancel[0]['owned_intent']))
  c['cancellation_never_resurrects']=bool(cancel) and all(r['latched'] and not r['collider_before'] for r in cancel[1:])
  c['owned_world_entity_only']=bool(cancel) and len({(shadow[r['ready_sample']]['impact_world'],r['entity']) for r in begin+cancel})==1 and all(r['world']==shadow[r['ready_sample']]['impact_world'] and r['owned_intent'] for r in cancel)
  c['native_lookup_unchanged_then_suppressed']=bool(over) and all(r['caller']=='0x36bb02' and r['original_resource_present'] and r['event_matches'] and r['returned_null'] for r in over)
  c['native_handle_zero_without_physics_update']=bool(cancelEnd) and all(not r['collider_after'] and r['lookup_suppressed'] and not native[r['ready_sample']]['collider_id'] and not native[r['ready_sample']]['physics_updated'] for r in cancelEnd)
  c['tracked_physics_exact']=all(native[r['ready_sample']]['physics_updated'] and bytes.fromhex(native[r['ready_sample']]['physics_current_bytes'])==physics_transform(r['applied_current']) and r['applied_current']==shadow[r['ready_sample']]['proposed_transform'] for r in begin)
  c['scope_restoration']=len(begin)==len(end) and len(cancel)==len(cancelEnd) and all(r['query_restored'] and r['original_components_unchanged'] and not r['ecs_written_by_mod'] for r in end+cancelEnd)
  c['exact_cancel_order']=len(cancel)==len(over)==len(cancelEnd) and all(a['ready_sample']==b['ready_sample']==d['ready_sample'] and a['us']<b['us']<native[a['ready_sample']]['us']<d['us'] for a,b,d in zip(cancel,over,cancelEnd))
 drain=ev('motion_collider_apply_drained');lookup=ev('motion_collider_lookup_removed');retime=ev('motion_event_retime_removed');hit=ev('motion_hit_observer_removed')
 c['scope_cleanup']=len(drain)==1 and drain[0]['opened']==drain[0]['closed']==len(begin+cancel) and drain[0]['applied']==len(begin) and drain[0]['cancelled']==len(cancel) and drain[0]['outstanding']==drain[0]['failures']==0
 c['lookup_hook_restored']=len(lookup)==1 and lookup[0]['restored'] and lookup[0]['callbacks']==0
 c['retime_drained']=len(retime)==1 and retime[0]['appended']==len(app) and retime[0]['blocked']==len(block) and retime[0]['suppressed']==1
 c['hit_observer_drained']=len(hit)==1 and hit[0]['restored'] and hit[0]['callbacks']==hit[0]['failures']==0
 return c,dict(loss=[first,last],paired_frames=len(pairs),tracked_colliders=len(begin),cancelled_colliders=len(cancel),blocked_events=len(block),target_damage_proven=False)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 rows=[json.loads(l) for l in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()]
 sequence=[[float(v) for v in l.split(',')] for l in (a.probe.parent/'mock-hands.csv').read_text().splitlines() if l and not l.startswith('#')];c,m=audit(rows,sequence)
 if a.negative_tests:
  variants=[('wrong_loss','motion_tracking_sample','grip_flags',15,1),('no_abort_release','motion_tracking_loss_abort','pulse_released',False,0),('source_model_changed','motion_visual_pose','source_unchanged',False,0),('no_input_restore','controller_fixture_restored','pointers_restored',False,0),('hook_not_restored','motion_collider_lookup_removed','restored',False,0)]
  if m['blocked_events']:variants += [('unowned_block','motion_event_blocked','ticket_identified',False,0),('block_not_latched','motion_event_blocked','latched',False,0),('wrong_actor_world','motion_event_blocked','world','0x0',0),('unobserved_release','controller_attack_release_confirmed','observed_packet',0,0)]
  else:variants += [('no_native_cancel','motion_collider_cancel_end','collider_after',999,0),('wrong_owned_world','motion_collider_cancel_begin','world','0x0',0),('false_fixture','motion_collider_cancel_begin','forced_fixture',True,0)]
  for name,event,key,value,index in variants:
   bad=copy.deepcopy(rows);hits=[r for r in bad if r['event']==event];exists=len(hits)>index
   if exists:hits[index][key]=value
   c['reject_'+name]=exists and not all(audit(bad,sequence)[0].values())
 c['runner_pass']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS'
 c['normal_exit']=json.loads((a.probe.parent/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
 c['Core_Sync_clean']=(a.probe.parent/'validation.log').stat().st_size==0
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'motion-tracking-loss-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
