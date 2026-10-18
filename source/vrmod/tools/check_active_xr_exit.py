"""Active same-frame stereo / private hands and weapon / Scene teardown, without combat or reentry."""
import argparse,copy,hashlib,json,pathlib
from collections import Counter

def audit(rows,xr,mock,gpu,publisher):
 ev=lambda rr,n:[r for r in rr if r.get('event')==n]
 c={};begin=ev(rows,'world_active_xr_exit_begin');end=ev(rows,'world_active_xr_exit_complete');pairs=ev(rows,'live_native_pair_complete');stereo=ev(xr,'stereo_frame');resources=ev(rows,'calibration_scene_disposal_complete');hook=ev(rows,'view_resource_hook_complete');closed=ev(xr,'native_xr_closed')
 c['active_unfinished_series_at_native_scene_exit']=len(begin)==len(end)==1 and 30<=begin[0]['pairs']<begin[0]['limit']==3000 and end[0]['pairs']==begin[0]['pairs'] and end[0]['native_owner_returned'] and not end[0]['reads_destroyed_memory']
 c['same_frame_both_eyes_submitted']=len(stereo)==len(pairs)>=30 and {r['frame'] for r in stereo}=={r['frame'] for r in pairs} and all(r['left_draws']==r['right_draws']==n for n,r in enumerate(stereo,1))
 submitted=ev(xr,'native_game_pair_submitted');c['native_pair_submission_matches_render_evidence']=len(submitted)==len(stereo) and {r['frame'] for r in submitted}=={r['frame'] for r in stereo}
 c['no_completed_series_before_scene_exit']=not ev(rows,'calibration_retained_until_scene_disposal') and bool(begin) and all(r['us']<begin[0]['us'] for r in pairs)
 c['normal_stop_closes_openxr_before_views']=len(closed)==1 and len(resources)==1 and bool(begin) and begin[0]['us']<closed[0]['us']<resources[0]['us']
 c['view_ownership_released']=len(resources)==1 and resources[0]['views_destroyed']==2 and resources[0]['owned_groups_cleared'] and resources[0]['original_view_selected']
 c['private_hands_and_weapon_released']=all(len(ev(rows,n))==1 and ev(rows,n)[0]['generation_invalidated'] and ev(rows,n)[0]['reference_released'] for n in ('motion_hand_model_disposed','motion_hand_pose_disposed','motion_visual_disposed'))
 created=ev(rows,'water_feedback_pool_created');released=ev(rows,'water_feedback_pool_released')
 c['all_private_readback_pools_released']=len(created)==len(released)==2 and {(r['eye'],r['pool']) for r in created}=={(r['eye'],r['pool']) for r in released}
 c['no_hidden_render_or_retirement_failure']=not any('failure' in r['event'] or 'retained_on_failure' in r['event'] or r['event']=='view_resource_dependencies_retained' for r in rows)
 c['resource_result_propagated']=len(end)==len(hook)==1 and end[0]['result']==hook[0]['result']==0 and end[0]['us']<hook[0]['us'] and hook[0]['original_bytes_restored']
 c['mock_swapchains_destroyed_empty']=len(ev(mock,'swapchain_created'))==len(ev(mock,'swapchain_destroyed'))==3 and all(r['pending_phase']==0 for r in ev(mock,'swapchain_destroyed'))
 c['mock_acquire_wait_release_balanced']=len(ev(mock,'acquire'))==len(ev(mock,'wait_image'))==len(ev(mock,'release'))>0
 c['mock_frames_all_ended']=len(ev(mock,'wait_frame'))==len(ev(mock,'end_frame'))==len(ev(xr,'native_frame_begin'))>30
 c['mock_session_and_instance_closed']=len(ev(mock,'session_destroyed'))==len(ev(mock,'instance_destroyed'))==1
 roots=[r for r in ev(publisher,'world_lifecycle_end') if r['operation']=='root_destroy' and r['kind']==2]
 c['client_root_ended_before_scene_retirement']=len(roots)==1 and bool(begin) and roots[0]['us']<begin[0]['us'] and roots[0]['thread']==begin[0]['thread']
 # Stronger than the C++ return code: every resource must have a successful terminal event.
 registrations=ev(gpu,'present_retirement_registered');deferred=ev(rows,'water_deferred_retired');polls=ev(gpu,'present_retirement_poll');snap=ev(rows,'water_deferred_snapshot');detach=[r for r in snap if r['phase']=='scene_detached']
 if registrations or deferred or snap:
  c['detached_owner_registered_once']=len(registrations)==len(ev(rows,'water_deferred_ownership'))==1 and registrations[0]['capacity']==1 and registrations[0]['callback_module_pinned'] and registrations[0]['thread']==begin[0]['thread']
  c['water_retired_after_original_scene_return']=len(deferred)==1 and end[0]['us']<deferred[0]['us']<hook[0]['us'] and deferred[0]['pools_remaining']==0 and not deferred[0]['scene_accessed'] and not deferred[0]['native_serials_modified']
  c['deferred_pools_match_private_creations']=bool(detach) and all((r['eye'],r['pool']) in {(x['eye'],x['pool']) for x in created} for r in snap) and len({r['pool'] for r in detach})==len(detach)
  c['unread_private_feedback_owned_at_detach']=bool(detach) and all(r['pending']>0 for r in detach)
  c['private_readback_serial_ledger_consistent']=bool(snap) and all(r['pending']==sum(bool(t[0] or t[1]) for t in r['slots']) for r in snap)
  cancels=ev(rows,'water_private_feedback_discard');c['native_discard_after_scene_and_completion']=len(cancels)==2 and all(r['scene_returned'] and r['native_take_calls']==0 and not r['native_serials_modified'] and r['completed']>0 and end[0]['us']<r['us'] for r in cancels)
  c['native_completion_advances_before_release']=bool(detach) and all(any(s['phase']=='later_present' and s['pool']==r['pool'] and s['completed']>=max(t[0] for t in r['slots']) and s['thread']==r['thread'] and r['us']<s['us']<x['us'] for s in snap for x in released if x['pool']==r['pool']) for r in detach)
  c['callback_completes_on_original_thread']=bool(polls) and polls[-1]['result']==1 and all(r['result'] in (0,1) and not r['device_ending'] and r['thread']==begin[0]['thread'] for r in polls) and all(r['result']==0 for r in polls[:-1])
 return c,dict(recorded_pairs=begin[0]['pairs'] if begin else 0,submitted_pairs=len(stereo),pending_pair_at_exit=begin[0]['pair_pending'] if begin else None,readback_pools_released=len(released),deferred_polls=deferred[0]['polls'] if deferred else None,scope=__doc__)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('live',type=pathlib.Path);p.add_argument('publisher',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();launch=a.live.parent
 paths=[a.live/'view-resource-probe.jsonl',launch/'native-xr/openxr.jsonl',launch/'mock.jsonl',launch/'vulkan.jsonl',a.publisher/'view-resource-probe.jsonl'];raw=[p.read_bytes() for p in paths];data=[[json.loads(l) for l in b.splitlines()] for b in raw];c,m=audit(*data)
 c['isolated_explicit_scene_exit_policy']=(a.live/'world-active-xr-exit.txt').read_text().strip()=='EVR_WORLD_ACTIVE_XR_EXIT_1' and not (a.live/'controller-ui-fixture.txt').exists()
 c['validation_clean']=(launch/'validation.log').stat().st_size==0
 status=json.loads((a.live/'probe-status.json').read_text());ex=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'));pub=json.loads((a.publisher/'world-lifetimes-run.json').read_text())
 c['export_publisher_and_normal_exit']=status['native_result']==0 and status['status']==pub['status']=='PASS' and ex['exit_code']==0
 if a.negative_tests:
  specs=[('missing_water',0,'water_feedback_pool_released',None,None,'all_private_readback_pools_released'),('eye_mismatch',1,'stereo_frame','right_draws',0,'same_frame_both_eyes_submitted'),('pending_swapchain',2,'swapchain_destroyed','pending_phase',1,'mock_swapchains_destroyed_empty'),('private_weapon_leak',0,'motion_visual_disposed','reference_released',False,'private_hands_and_weapon_released'),('early_close',1,'native_xr_closed','us',0,'normal_stop_closes_openxr_before_views')]
  if m['deferred_polls']:
   specs.extend([('early_deferred_free',0,'water_deferred_retired','us',0,'water_retired_after_original_scene_return'),('forged_serials',0,'water_deferred_retired','native_serials_modified',True,'water_retired_after_original_scene_return'),('wrong_thread',3,'present_retirement_poll','thread',0,'callback_completes_on_original_thread'),('lost_slot_serial',0,'water_deferred_snapshot','slots',[[0,0],[0,0]],'private_readback_serial_ledger_consistent')])
  for name,index,event,key,value,target in specs:
   bad=copy.deepcopy(data);row=next((r for r in bad[index] if r['event']==event),None)
   if row is None:c['reject_'+name]=False;continue
   if key:row[key]=value
   else:bad[index].remove(row)
   c['reject_'+name]=bool(c.get(target)) and audit(*bad)[0].get(target) is False
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,sources=[dict(path=str(p),sha256=hashlib.sha256(b).hexdigest()) for p,b in zip(paths,raw)]);(a.live/'active-xr-exit-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(json.dumps(m));return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
