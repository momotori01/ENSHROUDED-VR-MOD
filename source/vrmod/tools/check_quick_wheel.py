"""Finite XR radial-stick routing and native UI overlay evidence; not unrestricted play.
EVR_QUICK_WHEEL_1: one open/close with four cardinal directions.
EVR_QUICK_WHEEL_REBIND_1: three selections (up-right/up/up-right). Starting from another
item the native sequence is O,W,O,W; starting from the weapon it is W,O,W. The first
verified melee resource gets one private clone, the other selection suspends it and the
return rebinds the same clone. Diagnostic identity only: no grip calibration or combat claim.
"""
import argparse,copy,hashlib,json,math
from pathlib import Path

DIAGONAL=math.sqrt(.5)
SCHEDULES={
 'EVR_QUICK_WHEEL_1':dict(fixture='world-quick-wheel-hands.csv',end=800,cycles=[(500,720)],held=[(720,740,(0,1))],directions=[(0,1),(1,0),(0,-1),(-1,0)],grace=0),
 'EVR_QUICK_WHEEL_CONSUMABLE_1':dict(fixture='world-quick-wheel-consumable-hands.csv',end=730,cycles=[(480,530),(610,660)],held=[(530,540,(0,-1)),(660,670,(DIAGONAL,DIAGONAL))],directions=[(0,-1),(DIAGONAL,DIAGONAL)],grace=30),
 'EVR_QUICK_WHEEL_CONSUMABLE_2':dict(fixture='world-quick-wheel-consumable2-hands.csv',end=730,cycles=[(480,530),(610,660)],held=[(530,540,(1,0)),(660,670,(DIAGONAL,DIAGONAL))],directions=[(1,0),(DIAGONAL,DIAGONAL)],grace=30),
 'EVR_GRIP_ROUTER_EQUIP_1':dict(fixture='world-grip-router-equip-hands.csv',end=760,cycles=[(470,490),(560,580),(660,680)],held=[],directions=[],grace=15),
 'EVR_GRIP_ROUTER_EQUIP_2':dict(fixture='world-grip-router-equip-overlap-hands.csv',end=760,cycles=[(470,490),(560,580),(660,680)],held=[],directions=[],grace=15),
 'EVR_GRIP_ROUTER_WHEEL_1':dict(fixture='world-grip-router-wheel-hands.csv',end=880,cycles=[(490,550),(770,820)],held=[(550,560,(DIAGONAL,DIAGONAL)),(820,830,(DIAGONAL,DIAGONAL))],directions=[(DIAGONAL,DIAGONAL)],grace=15),
 'EVR_QUICK_WHEEL_SURVEY_1':dict(fixture='world-quick-wheel-survey-hands.csv',end=880,cycles=[(465,510),(545,590),(625,670),(705,750),(785,830)],held=[(510, 520, (-0.90630779, 0.42261826)), (590, 600, (-0.22495105, 0.97437006)), (670, 680, (0.22495105, 0.97437006)), (750, 760, (0.90630779, 0.42261826)), (830, 840, (0.62932039, 0.77714596))],directions=[(-0.90630779, 0.42261826), (-0.22495105, 0.97437006), (0.22495105, 0.97437006), (0.90630779, 0.42261826), (0.62932039, 0.77714596)],grace=15),
 'EVR_QUICK_WHEEL_REBIND_1':dict(fixture='world-quick-wheel-rebind-hands.csv',end=860,cycles=[(480,530),(610,660),(740,790)],held=[(530,540,(DIAGONAL,DIAGONAL)),(660,670,(0,1)),(790,800,(DIAGONAL,DIAGONAL))],directions=[(DIAGONAL,DIAGONAL),(0,1)],grace=30),
}
def near(a,b,tol=1e-6):return len(a)==len(b) and all(abs(x-y)<=tol for x,y in zip(a,b))
def covered(wanted,seen,tol):return all(any(near(w,s,tol) for s in seen) for w in wanted)

def audit(rows,token='EVR_QUICK_WHEEL_1'):
 plan=SCHEDULES[token]
 ev=lambda name:[r for r in rows if r.get('event')==name]
 front=ev('quick_wheel_frontend');packets=ev('quick_wheel_packet_polled');enabled=ev('quick_wheel_fixture_enabled');restored=ev('controller_fixture_restored')
 overlay=[r for r in front if r['auxiliary']==1]
 rebind=token!='EVR_QUICK_WHEEL_1'
 scope=len(enabled)==1 and enabled[0]['buttons_only']==[512] and enabled[0]['end_frame']==plan['end'] and enabled[0]['native_state_writes'] is False
 if rebind:scope=scope and enabled[0].get('schedule')==token and enabled[0].get('open_cycles')==[list(c) for c in plan['cycles']] and enabled[0].get('close_grace')==plan['grace']
 closed_held=[]
 for begin,end,held in plan['held']:
  after=[r for r in front if begin<=r['frame']<end and r['auxiliary']==0]
  closed_held.append(bool(after) and all(r['right']==[0,0] and near(r['raw_right'],held) and r['neutral_required'] for r in after))
 checks=dict(
  finite_scope=scope,
  current_owner_and_fresh_XR=bool(front) and len({r['owner'] for r in front})==1 and all(r['owner']>0 and 0<=r['age_ms']<=200 and r['screen']==1 for r in front),
  increasing_frame_time=bool(front) and all(b['frame']>a['frame'] and b['time']>a['time'] for a,b in zip(front,front[1:])),
  wheel_opened=bool(overlay) and all(any(r['frame']>=o and r['frame']<c for r in front) and all(r['auxiliary']==1 for r in front if o<=r['frame']<c) for o,c in plan['cycles']),
  radial_axes_preserved=bool(overlay) and all(r['valid'] and r['mode']==6 and r['right']==r['raw_right'] for r in overlay),
  four_directions_delivered=covered(plan['directions'],[tuple(r['right']) for r in overlay],1e-6),
  no_vertical_gameplay_buttons=bool(front) and all(r['mapped_buttons']==0 for r in front),
  closed_held_stick_neutral=all(closed_held),
  completed_rearmed=bool(front) and front[-1]['frame']>=plan['end'] and front[-1]['valid'] and front[-1]['auxiliary']==0 and front[-1]['right']==[0,0] and not front[-1]['neutral_required'] and not front[-1]['grip'],
  real_native_poll_cardinal_axes=covered(plan['directions'],[(r['right_x']/32767,r['right_y']/32767) for r in packets if r['buttons']==512],1/32767),
  no_native_attack_dodge_sprint=bool(packets) and all(r['buttons'] in (0,512) for r in packets),
  real_neutral_poll=bool(packets) and packets[-1]['buttons']==packets[-1]['right_x']==packets[-1]['right_y']==0,
  actual_native_disconnect_restore=len(restored)==1 and restored[0].get('pointers_restored') and restored[0].get('native_connection_seen') and restored[0].get('native_disconnection_seen') and restored[0].get('native_connected')==0,
  no_fixture_or_render_failure=not any('failure' in r.get('event','') or 'failed' in r.get('event','') for r in rows))
 if rebind:
  checks['each_cycle_closed_in_order']=bool(front) and [max((r.get('closed_cycles',-1) for r in front if r['frame']<o),default=0) for o,_ in plan['cycles']]==list(range(len(plan['cycles']))) and front[-1].get('closed_cycles')==len(plan['cycles'])
 pairs=ev('live_native_pair_complete');selection={r['serial']:r for r in ev('motion_visual_selection')};payload=ev('head_visibility_payload');created=ev('motion_visual_created');disposed=ev('motion_visual_disposed')
 checks['same_frame_600_pairs']=len(pairs)==600 and len({r['serial'] for r in pairs})==600
 checks['each_pair_has_current_selection']=bool(pairs) and all(r['serial'] in selection for r in pairs)
 checks['unready_weapon_never_exposed']=bool(payload) and all(r['hand_instance']==4294967295 for r in payload if r['serial'] in selection and not selection[r['serial']]['weapon_ready'])
 checks['owned_weapon_cleanup']=len(created)==len(disposed) and all(r['generation_invalidated'] and r['reference_released'] and r['scene_disposal'] for r in disposed)
 if token=='EVR_QUICK_WHEEL_REBIND_1':checks.update(audit_rebind(rows,plan,pairs,selection,payload,created,disposed))
 elif token in EQUIP:
  checks.update(audit_equip(rows,plan,pairs,selection,payload,created,disposed))
  if token=='EVR_GRIP_ROUTER_EQUIP_2':checks.update(audit_overlap(rows))
 elif token=='EVR_GRIP_ROUTER_WHEEL_1':checks.update(audit_router(rows,plan,pairs,selection,payload,created,disposed))
 elif token=='EVR_QUICK_WHEEL_SURVEY_1':checks.update(audit_survey(rows,plan,pairs,selection,payload,created,disposed)[0])
 elif rebind:checks.update(audit_consumable(rows,plan,pairs,selection,payload,created,disposed)[0])
 return {k:bool(v) for k,v in checks.items()}

def audit_rebind(rows,plan,pairs,selection,payload,created,disposed):
 ev=lambda name:[r for r in rows if r.get('event')==name]
 serials={r['serial'] for r in pairs};ordered=sorted(selection.values(),key=lambda r:r['frame'])
 items=[]
 for r in ordered:
  if r['item'] and (not items or items[-1]!=r['item']):items.append(r['item'])
 clone=created[0] if len(created)==1 else {};handle=clone.get('handle',0);weapon=clone.get('item',0)
 starts_with_weapon=bool(items) and items[0]==weapon
 poses=ev('motion_visual_pose');rebounds=ev('motion_visual_rebound');dormant=ev('motion_visual_dormant');resumed=ev('motion_visual_resumed')
 ready={s for s in serials if selection.get(s,{}).get('weapon_ready')}
 other={s for s in serials if selection.get(s,{}).get('item') not in (0,weapon)}
 pose_serial={r['serial']:r for r in poses}
 third_close=plan['cycles'][2][1];second_close=plan['cycles'][1][1];first_close=plan['cycles'][0][1]
 eyes=[r for r in payload if r['serial'] in serials]
 c={}
 expected=[weapon,'O',weapon] if starts_with_weapon else ['O',weapon,'O',weapon]
 c['native_selection_sequence_matches_wheel']=bool(weapon) and len(items)==len(expected) and all(i==e if e!='O' else i not in (0,weapon) for i,e in zip(items,expected)) and len({i for i in items if i!=weapon})==1
 c['clone_created_once_for_selected_weapon']=len(created)==1 and clone['item']==weapon and clone['weapon_calibration']=='diagnostic_identity' and clone['native_mask']==0 and clone['sdf_handle']==0 and clone['references_after']==clone['references_before']+1
 c['weapon_pose_matches_initial_selection']=bool(poses) and (min(r['frame'] for r in poses)<first_close if starts_with_weapon else min(r['frame'] for r in poses)>=first_close)
 c['other_selection_suspended']=len(other)>=10 and all(s not in pose_serial and not selection[s]['weapon_ready'] for s in other) and any(r['retained_handle']==handle and r['resource_retained'] and not r['combat_authorized'] and second_close<=r['frame'] for r in dormant)
 c['rebound_same_clone_once']=len(rebounds)==1 and rebounds[0]['handle']==handle and rebounds[0]['item']==weapon and rebounds[0]['clone_retained'] and not rebounds[0]['combat_authorized'] and rebounds[0]['previous_source']!=rebounds[0]['source'] and rebounds[0]['references']>=2 and rebounds[0]['frame']>=third_close
 c['pose_before_and_after_rebound']=len(rebounds)==1 and any(r['frame']<second_close and r['source_handle']==rebounds[0]['previous_source'] for r in poses) and any(r['frame']>=rebounds[0]['frame'] and r['source_handle']==rebounds[0]['source'] for r in poses) and any(r['frame']>=rebounds[0]['frame'] for r in resumed)
 c['poses_only_owned_clone_and_current_source']=bool(poses) and all(r['handle']==handle and r['item']==weapon and r['native_mask']==r['sdf_handle']==0 and r['source_unchanged'] and r['serial'] in selection and selection[r['serial']]['item']==weapon and selection[r['serial']]['source_handle']==r['source_handle'] for r in poses)
 c['ready_pairs_have_pose']=bool(ready) and ready<=set(pose_serial)
 c['both_eyes_clone_only_when_ready']=bool(eyes) and all(r.get('motion_visual') and r['hand_instance']==((handle&0xffffff) if r['serial'] in ready else 0xffffffff) and (r['serial'] not in ready or r['hand_mask_before']==0) for r in eyes)
 # Native Scene indices are reused by generation: the clone index may have belonged to
 # the earlier original before the clone existed, so exclude it only while it is owned.
 alive=(clone.get('us',0),disposed[0]['us'] if len(disposed)==1 else float('inf'))
 c['original_hidden_clone_not_in_native_list']=bool(eyes) and all((not selection[r['serial']]['source_handle'] or (selection[r['serial']]['source_handle']&0xffffff) in r['indices']) and (not alive[0]<r['us']<alive[1] or (handle&0xffffff) not in r['indices']) and r['only_private_stack_padding_changed'] for r in eyes if r['serial'] in selection)
 c['final_selection_is_weapon']=bool(ordered) and items[-1:]==[weapon] and ordered[-1]['item']==weapon
 c['single_disposal']=len(disposed)==1 and disposed[0]['handle']==handle and disposed[0]['updates']==len(poses)
 c['no_attack_authority']=not any(ev(n) for n in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','controller_attack_packet_polled','motion_collider_apply_enabled'))
 return c

def audit_consumable(rows,plan,pairs,selection,payload,created,disposed):
 """Weapon -> wheel-selected consumable -> weapon. game170323 showed that selecting a
 consumable in the gamepad quick wheel uses it immediately: the held weapon is stowed for
 the use animation and then re-attached with a new original model. Safety invariants: no
 private weapon while nothing is held, the same clone is re-bound on return, and the
 fixture never sends X/MainhandAction. Returns (checks, observations)."""
 ev=lambda name:[r for r in rows if r.get('event')==name]
 serials={r['serial'] for r in pairs};ordered=sorted(selection.values(),key=lambda r:r['frame'])
 clone=created[0] if len(created)==1 else {};handle=clone.get('handle',0);weapon=clone.get('item',0)
 poses=ev('motion_visual_pose');pose_serial={r['serial'] for r in poses};rebounds=ev('motion_visual_rebound')
 first_close=plan['cycles'][0][1]
 before=[r for r in ordered if r['frame']<first_close and r['item']]
 live=[r for r in ordered if r['serial'] in serials and r['frame']>=first_close]
 stow=[r for r in live if r['item']!=weapon];returned=[r for r in live if stow and r['item']==weapon and r['frame']>max(x['frame'] for x in stow)]
 stow_frames=sorted(r['frame'] for r in stow)
 contiguous=bool(stow_frames) and stow_frames[-1]-stow_frames[0]+1==len(stow_frames)
 eyes=[r for r in payload if r['serial'] in serials]
 c={}
 c['weapon_clone_created_once']=len(created)==1 and clone['weapon_calibration']=='diagnostic_identity' and clone['native_mask']==0 and clone['sdf_handle']==0
 c['weapon_selected_before_consumable']=bool(before) and bool(poses) and all(r['item']==weapon for r in before) and min(r['frame'] for r in poses)<first_close
 c['consumable_use_stows_weapon_without_clone']=len(stow)>=5 and contiguous and all(not r['weapon_ready'] and r['serial'] not in pose_serial for r in stow)
 c['weapon_returns_with_same_clone']=len(returned)>=10 and all(r['weapon_ready'] and r['serial'] in pose_serial for r in returned) and all(r['handle']==handle and r['item']==weapon for r in poses)
 c['rebound_once_same_clone_after_use']=len(rebounds)==1 and all(r['handle']==handle and r['item']==weapon and r['clone_retained'] and not r['combat_authorized'] and r['previous_source']!=r['source'] and stow_frames and r['frame']>stow_frames[-1] for r in rebounds)
 c['both_eyes_clone_only_when_ready']=bool(eyes) and all(r.get('motion_visual') and r['hand_instance']==((handle&0xffffff) if selection[r['serial']]['weapon_ready'] else 0xffffffff) for r in eyes if r['serial'] in selection)
 c['single_disposal']=len(disposed)==1 and disposed[0]['handle']==handle and disposed[0]['updates']==len(poses)
 c['no_attack_or_item_use_authority']=not any(ev(n) for n in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','controller_attack_packet_polled','motion_collider_apply_enabled'))
 obs=dict(weapon=weapon,stow_frames=[stow_frames[0],stow_frames[-1]] if stow_frames else [],stow_items=sorted({(r['item'],r['category'],r['item_slot'],r['slot']) for r in stow}),
  attachment_items=[{k:v for k,v in r.items() if k not in ('us','event')} for r in ev('attachment_visual_item')],
  attachment_categories=[{k:v for k,v in r.items() if k not in ('us','event','raw_item')} for r in ev('attachment_weapon_category')],
  dormant=[{k:r[k] for k in ('reason','frame','selected_item')} for r in ev('motion_visual_dormant')],rebounds=[{k:r[k] for k in ('frame','previous_source','source')} for r in rebounds])
 return c,obs

EQUIP=('EVR_GRIP_ROUTER_EQUIP_1','EVR_GRIP_ROUTER_EQUIP_2')
def audit_overlap(rows):
 """2026-10-05 18:50 user spec: shoulder presses made while the same hand rests at its temple.
 The temple gesture is active (left hand -> bit0, right -> bit1) yet the grip still equips,
 inside the v2 head-relative shoulder reach; the router never reports a gesture block."""
 ev=lambda name:[r for r in rows if r.get('event')==name]
 enabled=ev('grip_router_fixture_enabled');requests=ev('grip_router_equip_request');samples={r['frame']:r for r in ev('head_gesture_sample')};front=ev('quick_wheel_frontend')
 c={}
 c['v2_shoulder_reach_profile']=len(enabled)==1 and enabled[0].get('shoulder_reach')==[.25,.125,.5,0,.04] and enabled[0].get('gesture_blocks_grip') is False
 by={(r['request'],r['hand'],r['zone']):r for r in requests}
 staff,bow,main=by.get((3,1,3)),by.get((2,0,2)),by.get((1,1,1))
 c['staff_press_without_gesture']=bool(staff) and staff.get('gesture_overlap') is False and staff.get('gesture_hands')==0
 c['bow_press_during_left_temple_gesture']=bool(bow) and bow.get('gesture_overlap') is True and bow.get('gesture_hands')==1 and samples.get(bow['frame'],{}).get('active') is True
 c['main_press_during_right_temple_gesture']=bool(main) and main.get('gesture_overlap') is True and main.get('gesture_hands')==2 and samples.get(main['frame'],{}).get('active') is True
 entries=[(r['frame'],r.get('entered_hand')) for r in ev('head_gesture_sample') if r['entered']]
 c['one_entry_per_temple_before_press']=len(entries)==2 and entries[0][1]==0 and entries[1][1]==1 and bool(bow) and bool(main) and entries[0][0]<bow['frame'] and bow['frame']<entries[1][0]<main['frame']
 c['router_never_gesture_blocked']=bool(front) and all(r.get('router_blocked')!=4 for r in front) and any(r.get('router_gesture_overlap') for r in front)
 c['gesture_never_maps_buttons_here']=bool(samples) and all(r['mapped_buttons']==0 for r in samples.values())
 return c

def audit_equip(rows,plan,pairs,selection,payload,created,disposed):
 """Router zone presses executed only via WheelSelect toward planned weapon slots:
 right hip -> staff, left shoulder -> bow, right shoulder -> remembered melee (axe)."""
 ev=lambda name:[r for r in rows if r.get('event')==name]
 front=ev('quick_wheel_frontend');requests=ev('grip_router_equip_request');plans=ev('equip_plan');results=ev('equip_wheel_result');snaps=ev('actionbar_snapshot');slots=ev('input_slot_selection')
 c={}
 c['three_requests_executed']=[(r['request'],r['hand'],r['zone']) for r in requests]==[(3,1,3),(2,0,2),(1,1,1)] and all(r['executed'] is True and r['native_writes'] is False for r in requests)
 c['plans_target_weapon_slots']=len(plans)==3 and [p['result'] for p in plans]==[1,1,1] and all(p['fresh'] for p in plans)
 def slot_of(p):return p['slots'][p['slot']]
 c['planned_slots_are_unique_weapon_instances']=len(plans)==3 and all(slot_of(p)[1]==1 and slot_of(p)[2]!=0 and slot_of(p)[3]==2 for p in plans) and [slot_of(p)[4] for p in plans][:2]==[4,5] and slot_of(plans[2])[4]==1
 c['main_weapon_uses_remembered_melee']=len(plans)==3 and plans[2]['preferred_melee']==plans[2]['slot'] and plans[0]['selected']==plans[2]['slot']
 c['never_targets_consumable']=all(not (p['slots'][p['slot']][2]==0) for p in plans if p['slot']<8)
 c['wheel_results_done']=len(results)==3 and all(r['done'] and r['error']=='' and r['selected']==r['target'] for r in results) and [r['target'] for r in results]==[p['slot'] for p in plans]
 seq=[s['actionbar_slot'] for s in slots];stable=[]
 for s in seq:
  if not stable or stable[-1]!=s:stable.append(s)
 c['native_selection_followed_plans']=bool(plans) and stable[0]==plans[0]['selected'] and all(p['slot'] in stable for p in plans) and stable[-1]==plans[2]['slot']
 c['rb_only_from_wheel_select']=bool(front) and all((r['grip']==bool(r['select_rb'])) or (r.get('router_wheel') and not r['select_rb']) for r in front) and not any(r.get('router_wheel') for r in front)
 c['actionbar_snapshots_logged']=len(snaps)>=2 and all(s['read_only'] for s in snaps)
 c['single_clone_rebound_on_return']=len(created)==1 and len(disposed)==1 and bool(ev('motion_visual_rebound')) and all(r['handle']==created[0]['handle'] and not r['combat_authorized'] for r in ev('motion_visual_rebound'))
 c['no_attack_or_item_use_authority']=not any(ev(n) for n in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','controller_attack_packet_polled','motion_collider_apply_enabled'))
 return c

def audit_router(rows,plan,pairs,selection,payload,created,disposed):
 """RB only from GripRouter: long presses open the wheel ~300 ms after the press, a short press
 and a right-shoulder zone press never do; the zone press yields one logged MainWeapon request."""
 ev=lambda name:[r for r in rows if r.get('event')==name]
 front=ev('quick_wheel_frontend');enabled=ev('grip_router_fixture_enabled');requests=ev('grip_router_equip_request')
 wheel=[r for r in front if r.get('router_wheel')]
 def first_true(lo,hi):return min((r['frame'] for r in wheel if lo<=r['frame']<hi),default=None)
 c={}
 c['router_fixture_enabled']=len(enabled)==1 and enabled[0]['rb_source']=='grip_router_only' and enabled[0]['equip_executed'] is False and enabled[0]['long_press_ns']==300000000
 c['router_fields_every_frame']=bool(front) and all('router_wheel' in r and 'raw_right_grip' in r for r in front)
 c['rb_only_from_router']=bool(front) and all(r['grip']==(r['router_valid'] and r['router_wheel']) for r in front)
 f1,f2=first_true(440,560),first_true(720,830)
 c['long_press_latency_300ms']=f1 is not None and f2 is not None and 476<=f1<=482 and 756<=f2<=762
 c['wheel_only_during_long_presses']=bool(wheel) and all(476<=r['frame']<=551 or 756<=r['frame']<=821 for r in wheel)
 c['short_press_no_wheel']=any(r['raw_right_grip'] for r in front if 600<=r['frame']<=609) and not any(r['auxiliary']==1 or r.get('router_wheel') for r in front if 565<=r['frame']<650)
 zone=[r for r in front if 660<=r['frame']<=699]
 c['zone_press_consumed_no_wheel']=bool(zone) and all(r['raw_right_grip'] and r['router_right_consumed'] and not r['router_wheel'] and r['auxiliary']==0 for r in zone)
 c['one_logged_main_weapon_request']=len(requests)==1 and requests[0]['request']==1 and requests[0]['hand']==1 and requests[0]['zone']==1 and 660<=requests[0]['frame']<=662 and requests[0]['executed'] is False and requests[0]['native_writes'] is False
 c['equip_rearm_flagged']=any(r['router_rearm'] for r in front if 660<=r['frame']<=700)
 c['no_overlay_between_long_presses']=not any(r['auxiliary']==1 for r in front if 565<=r['frame']<750)
 c['weapon_kept_and_single_clone']=len(created)==1 and len(disposed)==1 and not ev('motion_visual_rebound') and all(r['item']==created[0]['item'] for r in selection.values() if r['item'])
 c['no_attack_or_item_use_authority']=not any(ev(n) for n in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','controller_attack_packet_polled','motion_collider_apply_enabled'))
 return c

def audit_survey(rows,plan,pairs,selection,payload,created,disposed):
 """Five weapon/tool wheel selections ending on the initial weapon. Only the first verified
 resource owns a private clone; every other item suspends it, the return re-binds it.
 Observations map each wheel cycle to the native SlotSelection actionbar index."""
 ev=lambda name:[r for r in rows if r.get('event')==name]
 serials={r['serial'] for r in pairs};ordered=sorted(selection.values(),key=lambda r:r['frame'])
 clone=created[0] if len(created)==1 else {};handle=clone.get('handle',0);weapon=clone.get('item',0)
 poses=ev('motion_visual_pose');pose_serial={r['serial'] for r in poses};rebounds=ev('motion_visual_rebound')
 closes=[c for _,c in plan['cycles']];slots=ev('input_slot_selection')
 other=[r for r in ordered if r['serial'] in serials and r['item'] not in (0,weapon)]
 eyes=[r for r in payload if r['serial'] in serials]
 c={}
 c['survey_clone_created_once_for_initial_weapon']=len(created)==1 and clone['weapon_calibration']=='diagnostic_identity' and bool(ordered) and next((r['item'] for r in ordered if r['item']),0)==weapon
 c['other_items_suspend_clone']=len(other)>=20 and all(not r['weapon_ready'] and r['serial'] not in pose_serial for r in other)
 c['final_weapon_rebound_same_clone']=bool(rebounds) and all(r['handle']==handle and r['item']==weapon and r['clone_retained'] and not r['combat_authorized'] for r in rebounds) and rebounds[-1]['frame']>=closes[-1] and any(r['frame']>=rebounds[-1]['frame'] for r in poses) and bool(ordered) and ordered[-1]['item']==weapon
 c['poses_only_owned_clone']=bool(poses) and all(r['handle']==handle and r['item']==weapon for r in poses)
 c['both_eyes_clone_only_when_ready']=bool(eyes) and all(r.get('motion_visual') and r['hand_instance']==((handle&0xffffff) if selection[r['serial']]['weapon_ready'] else 0xffffffff) for r in eyes if r['serial'] in selection)
 c['slot_selection_changes_logged']=len({r['actionbar_slot'] for r in slots})>=5 and bool(slots) and slots[0]['actionbar_slot']==slots[-1]['actionbar_slot'] and all(r['read_only'] for r in slots)
 c['single_disposal']=len(disposed)==1 and disposed[0]['handle']==handle and disposed[0]['updates']==len(poses)
 c['no_attack_or_item_use_authority']=not any(ev(n) for n in ('native_attack_ticket','motion_attack_fixture_enabled','controller_attack_fixture_enabled','controller_attack_packet_polled','motion_collider_apply_enabled'))
 mapping=[]
 for i,close in enumerate(closes):
  until=closes[i+1] if i+1<len(closes) else 10**9
  after=[r for r in slots if r['frame']>=close-2 and r['frame']<until]
  held=[r for r in ordered if close+30<=r['frame']<min(until,close+60) and r['item']]
  mapping.append(dict(cycle=i+1,direction=plan['directions'][i],release_frame=close,actionbar_slots=[r['actionbar_slot'] for r in after],
   held_items=sorted({(r['item'],r['category'],r['item_slot'],r['slot']) for r in held})))
 obs=dict(weapon=weapon,initial_slot=slots[0]['actionbar_slot'] if slots else None,mapping=mapping,actionbar_entities=sorted({tuple(r['actionbar_entities']) for r in slots}),
  slot_events=len(slots),rebounds=[{k:r[k] for k in ('frame','previous_source','source')} for r in rebounds],
  attachment_items=[{k:v for k,v in r.items() if k not in ('us','event')} for r in ev('attachment_visual_item')])
 return c,obs

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--report',type=Path,help='Write elsewhere (regression on preserved evidence); default probe/quick-wheel-checks.json');a=p.parse_args()
 token=(a.probe/'controller-ui-fixture.txt').read_text().strip()
 if token not in SCHEDULES:raise ValueError('explicit quick wheel fixture required')
 raw=(a.probe/'view-resource-probe.jsonl').read_bytes();rows=[json.loads(l) for l in raw.splitlines() if l.strip()];checks=audit(rows,token);base=all(checks.values())
 checks['exact_approved_fixture']=(a.probe.parent/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env'/SCHEDULES[token]['fixture']).read_bytes()
 if a.negative_tests:
  cases=[('wrong_axis','quick_wheel_frontend',{'right':[0,0]}),('stale','quick_wheel_frontend',{'age_ms':201}),('wrong_owner','quick_wheel_frontend',{'owner':0}),('gameplay_button','quick_wheel_packet_polled',{'buttons':8192}),('no_restore','controller_fixture_restored',{'pointers_restored':False})]
  if token in EQUIP:
   # The CSV stick never moves in equip mode; the wheel stick comes from WheelSelect.
   cases=[c for c in cases if c[0]!='wrong_axis']+[('equip_rb_mismatch','quick_wheel_frontend',{'grip':True,'select_rb':0})]
  if token.startswith('EVR_QUICK_WHEEL_CONSUMABLE_'):
   cases+=[('recreated_clone','motion_visual_created',None),('consumable_pose','motion_visual_selection',{'weapon_ready':True}),('leaked_clone','motion_visual_disposed',{'reference_released':False})]
   if any(r.get('event')=='motion_visual_rebound' for r in rows):cases.append(('rebound_combat','motion_visual_rebound',{'combat_authorized':True}))
  elif token in EQUIP:
   cases+=[('equip_extra_plan','equip_plan',None),('equip_failed','equip_wheel_result',{'done':False}),('equip_extra_request','grip_router_equip_request',None)]
   if token=='EVR_GRIP_ROUTER_EQUIP_2':cases+=[('overlap_dropped','grip_router_equip_request',{'gesture_overlap':False}),('gesture_blocked','quick_wheel_frontend',{'router_blocked':4}),('v1_profile','grip_router_fixture_enabled',{'shoulder_reach':None})]
  elif token=='EVR_GRIP_ROUTER_WHEEL_1':
   cases+=[('router_rb_mismatch','quick_wheel_frontend',{'grip':True,'router_wheel':False}),('router_extra_request','grip_router_equip_request',None),('router_executed','grip_router_equip_request',{'executed':True})]
  elif token=='EVR_QUICK_WHEEL_SURVEY_1':
   cases+=[('recreated_clone','motion_visual_created',None),('other_pose','motion_visual_selection',{'weapon_ready':True}),('leaked_clone','motion_visual_disposed',{'reference_released':False}),('slot_log_writable','input_slot_selection',{'read_only':False})]
  elif token!='EVR_QUICK_WHEEL_1':cases+=[('recreated_clone','motion_visual_created',None),('rebound_new_clone','motion_visual_rebound',{'handle':1}),('rebound_combat','motion_visual_rebound',{'combat_authorized':True}),('pose_while_other','motion_visual_selection',{'weapon_ready':True}),('leaked_clone','motion_visual_disposed',{'reference_released':False})]
  for name,event,change in cases:
   bad=copy.deepcopy(rows)
   if name=='wrong_axis':target=next((r for r in bad if r['event']==event and r['auxiliary']==1 and r['right']!=[0,0]),None)
   elif name=='consumable_pose':
    live={r['serial'] for r in bad if r['event']=='live_native_pair_complete'};first,second=SCHEDULES[token]['cycles'][0][1],SCHEDULES[token]['cycles'][1][1]
    weapon=next((r['item'] for r in bad if r['event']=='motion_visual_created'),0)
    target=next((r for r in bad if r['event']==event and r['serial'] in live and r['frame']>=first and r['item']!=weapon),None)
   elif name=='other_pose':
    weapon=next((r['item'] for r in bad if r['event']=='motion_visual_created'),0)
    live={r['serial'] for r in bad if r['event']=='live_native_pair_complete'}
    target=next((r for r in bad if r['event']==event and r['item'] not in (0,weapon) and r['serial'] in live),None)
   elif name=='pose_while_other':
    weapon=next((r['item'] for r in bad if r['event']=='motion_visual_created'),0)
    live={r['serial'] for r in bad if r['event']=='live_native_pair_complete'}
    target=next((r for r in bad if r['event']==event and r['item'] not in (0,weapon) and r['serial'] in live),None)
   elif name=='overlap_dropped':target=next((r for r in bad if r['event']==event and r.get('gesture_overlap')),None)
   else:target=next((r for r in bad if r['event']==event),None)
   if target is None:checks['reject_'+name]=False;continue
   if change is None:bad.append(copy.deepcopy(target))
   else:target.update(change)
   checks['reject_'+name]=base and not all(audit(bad,token).values())
 report=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,raw_sha256=hashlib.sha256(raw).hexdigest(),schedule=token,scope=__doc__)
 if token=='EVR_QUICK_WHEEL_SURVEY_1':
  plan=SCHEDULES[token];ev=lambda name:[r for r in rows if r.get('event')==name]
  report['observations']=audit_survey(rows,plan,ev('live_native_pair_complete'),{r['serial']:r for r in ev('motion_visual_selection')},ev('head_visibility_payload'),ev('motion_visual_created'),ev('motion_visual_disposed'))[1]
 if token.startswith('EVR_QUICK_WHEEL_CONSUMABLE_'):
  plan=SCHEDULES[token];ev=lambda name:[r for r in rows if r.get('event')==name]
  report['observations']=audit_consumable(rows,plan,ev('live_native_pair_complete'),{r['serial']:r for r in ev('motion_visual_selection')},ev('head_visibility_payload'),ev('motion_visual_created'),ev('motion_visual_disposed'))[1]
 (a.report or a.probe/'quick-wheel-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
