"""Acceptance of process-local XInput fixture. Synthetic, not a physical pad test."""
import argparse,json,pathlib

def check(probe):
 rows=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 events=lambda n:[x for x in rows if x['event']==n]
 begin=events('controller_fixture_connected');end=events('controller_fixture_restored');connections=events('controller_fixture_native_connection');complete=events('input_observation_complete')
 world=bool(events('controller_fixture_armed'))
 retained=events('calibration_retained_until_scene_disposal')
 completed=(len(retained)==1 and retained[0]['pairs']>=300 and len(events('live_native_pair_complete'))==retained[0]['pairs']) if world else (len(complete)==1 and complete[0]['result']==0)
 checks={
 'explicit_process_fixture':len(begin)==1 and begin[0]['synthetic'] and begin[0]['all_native_ports_disconnected'],
 'fixture_no_errors':not any('failure' in x['event'] for x in rows),
 'observation_completed':completed,
 'native_connect_then_disconnect':[x['connected'] for x in connections]==[True,False],
 'native_rescan_consumed':bool(connections) and all(x['rescan_pending']==0 for x in connections),
 'native_connection_requires_actual_poll':bool(connections) and all(x['state_calls']>0 for x in connections),
 'pointer_table_restored':len(end)==1 and end[0]['pointers_restored'],
 'positive_state_and_disconnect_polls':len(end)==1 and end[0]['states']>2 and end[0]['disconnect_results']>0,
 'both_native_transitions_seen':len(end)==1 and end[0].get('native_connection_seen') and end[0].get('native_disconnection_seen'),
 'no_pending_device_at_restore':len(end)==1 and end[0].get('rescan_pending')==0 and end[0].get('native_connected')==0,
 'unchanged_host_claim_explicit':len(end)==1 and end[0]['system_settings_unchanged'],
 }
 for n in ['input_ui_observer_removed','input_direction_observer_removed','input_sender_observer_removed','input_pipeline_removed','input_actor_final_removed','input_ranged_removed','input_picking_removed']:
  e=events(n);checks[n]=len(e)==1 and e[0]['restored'] and e[0].get('failures',0)==0 and e[0].get('callbacks',e[0].get('remaining_callbacks',0))==0
 motion=bool(begin) and begin[0].get('motion_sequence',False)
 actors=events('input_actor_final_sample');checks['no_movement_or_gameplay_actions']=bool(actors) and all(x['desired_local']==[0,0,0] and x['world_move']==[0,0,0] and x['client_state_mask']==0 for x in actors)
 phases=events('controller_menu_phase');ui=events('input_ui_native_sample')
 if motion:
  del checks['no_movement_or_gameplay_actions']
  checks['motion_no_analog_movement']=bool(actors) and all(x['desired_local']==[0,0,0] and x['world_move']==[0,0,0] for x in actors)
 menu=bool(begin) and not begin[0]['neutral_only'] and not motion
 tabs=bool(begin) and begin[0].get('navigation_sequence',False)
 altar=bool(begin) and begin[0].get('altar_sequence',False)
 menuScreen=12 if altar else 4
 pressed=[x['buttons'] for x in phases if x['buttons']]
 if menu:
  polled=events('controller_fixture_packet_polled')
  if polled:
   checks['emitted_button_edges_consumed_by_native_xinput']=[x['buttons'] for x in polled if x['buttons']]==pressed and polled[-1]['buttons']==0
  checks['menu_sequence_finished']=bool(phases) and phases[-1]['phase']==8 and all(x['phase']!=9 for x in phases)
  checks['only_declared_navigation_and_release']=bool(phases) and all(x['buttons'] in ((0,32768,8192) if altar else (0,16,8,512,256) if tabs else (0,16)) for x in phases) and pressed==([32768,8192] if altar else [16,8,512,256,16] if tabs else [16,16])
  checks['native_controller_ingame_menu_observed']=any(x['hud_screen']==menuScreen and x.get('frontend_input_mode')==1 and x.get('ui_gamepad_mode')==1 for x in ui)
  checks['native_controller_hud_restored']=any(x['hud_screen']==1 and x.get('local_input_mode')==1 and x['us']>max((p['us'] for p in phases if p['phase']==5),default=2**64) for x in ui)
  if altar:
   cameras=events('controller_fixture_camera')
   checks['actual_native_altar_camera_and_return']=bool(cameras) and cameras[0]['camera_id'] in (4,19) and cameras[-1]['camera_id'] in (4,19) and any(x['camera_id']==31 for x in cameras)
   checks['only_altar_and_hud_no_upgrade_pages']=all(x['screen'] in (1,12) for x in phases)
 elif not motion:checks['neutral_mode_consistent']=len(begin)==len(end)==1 and begin[0]['neutral_only'] and end[0]['neutral_only'] and not phases
 if motion:
  steps=events('controller_motion_phase');packets=events('controller_fixture_packet_polled');twice=begin[0].get('double_jump',False);glide=begin[0].get('glide_sequence',False)
  checks['motion_completed_without_abort']=bool(steps) and steps[-1]['phase']==9 and all(x['phase']!=10 for x in steps)
  checks['only_expected_A_pulses_consumed']=[x['buttons'] for x in packets if x['buttons']]==[4096]*(2 if twice or glide else 1) and packets[-1]['buttons']==0
  # Explicit glide deploys on verified air immediately after a released A.
  # The airborne predicate/Root displacement below remains mandatory.
  air=[x for x in steps if x['phase']==4 or (glide and x['phase']==5)];land=[x for x in steps if x['phase']==8]
  checks['native_airborne_root_displacement_observed']=bool(air) and not air[0].get('actor_state',1)&1 and air[0].get('actor_state',0)&((1<<1)|(1<<32)) and air[0]['root'][1]>steps[0]['root'][1]+.03
  checks['native_landing_back_at_start_observed']=bool(land) and land[0].get('actor_state',0)&1 and max(abs(a-b) for a,b in zip(land[0]['root'],steps[0]['root']))<.03
  if glide:
   del checks['native_landing_back_at_start_observed']
   checks['native_glide_landed_at_floor_height']=bool(land) and land[0].get('actor_state',0)&1 and abs(land[0]['root'][1]-steps[0]['root'][1])<.03
  checks['only_normal_hud_during_motion']=bool(steps) and all(x['screen']==1 for x in steps)
 if world and not motion:
  armed=events('controller_fixture_armed');checks['armed_after_actual_stereo_submission']=len(armed)==1 and armed[0]['submitted_pairs']==30 and begin[0]['us']>armed[0]['us']
  xrPath=probe.parent/'native-xr';xr=[json.loads(x) for x in (xrPath/'openxr.jsonl').read_text(encoding='utf-8').splitlines()]
  captures=[x for x in xr if x['event']=='native_fixture_capture_complete'];requests=events('controller_menu_capture_requested')
  head=events('input_head_walking_sample') or events('input_head_idle_sample')
  opened=[x for x in phases if x['phase']==4];returned=[x for x in phases if x['phase']==7]
  if head:
   # Bind the interval to native screen evidence, not the fixture's later
   # ObserveClosed phase. Head routing may already see restored HUD in WaitClose.
   during=[]
   for sample in head:
    observed=[x for x in ui if x['us']<=sample['us']]
    if observed and max(observed,key=lambda x:x['us'])['hud_screen']==menuScreen:during.append(sample)
   checks['head_aim_preserves_native_menu_input']=bool(during) and all(not x['applied'] and x['native_preserved'] and not x['normal_ui_bound'] for x in during)
   checks['head_aim_resumes_after_native_hud_return']=bool(returned) and any(x['us']>returned[0]['us'] and x['applied'] and x['normal_ui_bound'] for x in head)
  checks['both_menu_and_return_images_captured']=[x['tag'] for x in requests]==([1,3,4,5,2] if tabs else [1,2]) and [x['tag'] for x in captures]==([1,3,4,5,2] if tabs else [1,2]) and all(x['ui_submitted'] for x in captures)
  if tabs:checks['exact_non_destructive_navigation_sequence']=[x['buttons'] for x in phases if x['buttons']]==[16,8,512,256,16]
  bounds=[(1,4,10 if tabs else 5,menuScreen),(2,7,8,1)]
  if tabs:bounds.extend([(3,11,12,4),(4,13,14,4),(5,15,5,4)])
  for tag,startPhase,endPhase,screen in bounds:
   starts=[x for x in phases if x['phase']==startPhase];ends=[x for x in phases if x['phase']==endPhase];images=[x for x in captures if x['tag']==tag];req=[x for x in requests if x['tag']==tag]
   ok=len(starts)==len(ends)==len(images)==len(req)==1 and req[0]['screen']==screen and starts[0]['us']<=req[0]['us']<images[0]['us']<ends[0]['us']
   if images:
    frame=images[0]['frame'];meta=xrPath/f'ui-frame-{frame}.json'
    ok=ok and meta.exists() and json.loads(meta.read_text()).get('fixture_capture_tag')==tag and all((xrPath/f'frame-{frame}-eye-{eye}.ppm').exists() for eye in (0,1)) and all((xrPath/f'ui-frame-{frame}-{kind}.rgba8').exists() for kind in ('native','xr'))
   checks[f'capture_{tag}_matches_observed_native_screen_and_submission']=ok
 exitPath=probe.parent/'process-exit.json';exitInfo=json.loads(exitPath.read_text(encoding='utf-8-sig')) if exitPath.exists() else {}
 checks['normal_game_shutdown']=exitInfo.get('exit_code')==0
 report=dict(scope=__doc__,probe=str(probe),checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},synthetic_menu=menu,integrated_world=world,tab_navigation=tabs,altar_sequence=altar,motion_sequence=motion,limitation='Host setting assertion requires separate host baseline audit; GPU/image/menu coverage beyond this sequence is not implied.')
 (probe/'controller-fixture-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(sum(bool(x) for x in checks.values()),'PASS /',sum(not x for x in checks.values()),'FAIL')
 return all(checks.values())
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();raise SystemExit(not check(a.probe))
