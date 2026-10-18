"""Private native weapon and synchronized OpenXR grip evidence (single asset diagnostic).
No hit/contact, model pivot calibration, hand mesh, or hardware latency claim.
"""
import argparse,json,math,csv,struct
from pathlib import Path
from check_recorded_xr_native import rotate,multiply,normalize

def rows(path):return [json.loads(x) for x in path.read_text(encoding='utf-8-sig').splitlines()]
def check(native,gpu,xr,sequence,pose_expectation="moving",profile=None,active_exit=False):
    ev=lambda data,name:[x for x in data if x['event']==name]
    created=ev(native,'motion_visual_created');poses=ev(native,'motion_visual_pose');disposed=ev(native,'motion_visual_disposed')
    pair=ev(native,'live_native_pair_complete');payload=ev(native,'head_visibility_payload')
    item=created[0] if len(created)==1 else {};handle=item.get('handle',0);source=item.get('source_handle',0)
    serials={x['serial'] for x in pair};poseSerial={x['serial']:x for x in poses};anchors={x['recorded_xr_frame']:x for x in ev(native,'recorded_xr_anchor')}
    eyes=ev(xr,'runtime_eye_pose');starts={x['frame']:x for x in ev(xr,'native_frame_begin')}
    private=[x for x in payload if x['serial'] in serials]
    originals=[x for x in ev(native,'world_shared_render_stage') if x['eye']==-1]
    c={
      'owned_clone_created_once':len(created)==1 and handle and source and handle!=source and item['native_mask']==0 and item['sdf_handle']==0 and item['references_after']==item['references_before']+1,
      'explicit_diagnostic_calibration':bool(item) and item.get('weapon_calibration')==('measured_local_grip' if pose_expectation in ('calibrated','calibrated-attack','calibrated-release','controller-speed','head-combat','right-speed','guard-posture','melee-repeat','calibrated-prefix') else 'diagnostic_identity'),
      'complete_native_stereo_pairs':(len(pair)==600 if not active_exit else (len(ev(native,'world_active_xr_exit_begin'))==1 and 30<=len(pair)<3000 and ev(native,'world_active_xr_exit_begin')[0]['limit']==3000 and ev(native,'world_active_xr_exit_begin')[0]['pairs']==len(pair)+int(ev(native,'world_active_xr_exit_begin')[0]['pair_pending']))) and len(serials)==len(pair),
      'native_and_runtime_no_failures':not any('failure' in x['event'] or 'failed' in x['event'] for x in native+xr),
      'every_submitted_pair_has_one_hand_pose':bool(serials) and serials<=set(poseSerial) and len(poses)==len(poseSerial),
      'pose_uses_same_XR_frame_and_time':bool(poses) and all(x['frame'] in starts and x['time']==starts[x['frame']]['time'] and x['same_frame_hand'] for x in poses),
      'original_model_preserved_native_clone_hidden':bool(poses) and all(x['handle']==handle and x['source_handle']==source and x['native_mask']==0 and x['sdf_handle']==0 and x['source_unchanged'] for x in poses),
      'updates_before_native_shared_stages':bool(poses) and all(len([s for s in originals if s['serial']==x['serial'] and s['us']>x['us']])==2 for x in poses),
      'both_eyes_use_owned_clone':bool(private) and all(x.get('motion_visual') and x['hand_instance']==(handle&0xffffff) and x['hand_mask_before']==0 for x in private),
      'original_weapon_hidden_only_private_payload':bool(private) and all((source&0xffffff) in x['indices'] and (handle&0xffffff) not in x['indices'] and x['only_private_stack_padding_changed'] for x in private),
      'owned_model_scene_disposal':len(disposed)==1 and disposed[0]['handle']==handle and disposed[0]['generation_invalidated'] and disposed[0]['reference_released'] and disposed[0]['scene_disposal'] and disposed[0]['updates']==len(poses),
      'XR_actions_open_and_close_once':len(ev(xr,'native_motion_opened'))==len(ev(xr,'native_motion_closed'))==1 and ev(xr,'native_motion_closed')[0]['success'],
      'world_CPU_wait_keeps_pending_SDF':bool(ev(native,'eye_cpu_tasks_completion')) and all(x['complete'] and x['sdf_results_preserved'] for x in ev(native,'eye_cpu_tasks_completion')),
      'expected_clone_motion':(len({tuple(x['q32']) for x in poses})>100 and len({tuple(x['orientation']) for x in poses})>100) if pose_expectation in ('moving','calibrated') else (len({tuple(x['grip_position']) for x in poses})==7 and len({tuple(x['orientation']) for x in poses})==1),
    }
    if pose_expectation in ('two-swing','two-swing-cooldown'):
        f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
        expected={tuple(f32(v) for v in sequence[min(x['frame']-1,len(sequence)-1)][14:17]) for x in poses}
        c['expected_clone_motion']=len(expected)>30 and len({tuple(x['grip_position']) for x in poses})==len(expected) and len({tuple(x['orientation']) for x in poses})==1
    if pose_expectation=='vertical-stick':
        c['expected_clone_motion']=bool(poses) and len({tuple(x['grip_position']) for x in poses})==1 and len({tuple(x['orientation']) for x in poses})==1 and len({tuple(x['q32']) for x in poses})>10
    if pose_expectation=='head-gesture':
        c['expected_clone_motion']=len({tuple(x['grip_position']) for x in poses})==2 and len({tuple(x['orientation']) for x in poses})==1
    if pose_expectation in ('calibrated-attack','calibrated-release','controller-speed','head-combat','right-speed','guard-posture','melee-repeat','calibrated-prefix'):
        f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
        expected={tuple(f32(v) for v in sequence[min(x['frame']-1,len(sequence)-1)][14:17]) for x in poses}
        c['expected_clone_motion']=len(expected)>20 and len({tuple(x['grip_position']) for x in poses})==len(expected)
        c['explicit_diagnostic_calibration']=bool(item) and item.get('weapon_calibration')=='measured_local_grip'
    if pose_expectation=='guard-posture':
        c['guard_weapon_rotation_fixed']=bool(poses) and max(min(max(abs(a-b) for a,b in zip(x['orientation'],poses[0]['orientation'])),max(abs(a+b) for a,b in zip(x['orientation'],poses[0]['orientation']))) for x in poses)<2e-6
    # Independent expected position/orientation from the archived CSV, static
    # runtime eyes and original stable anchor, not the bridge's expected pose.
    c['static_HMD_fixture']=bool(eyes) and all(max(abs(t) for t in x['orientation'][:3])<1e-7 and abs(x['orientation'][3]-1)<1e-7 for x in eyes)
    center=[sum(x['position'][i] for x in eyes[:2])/2 for i in range(3)] if len(eyes)>=2 else [0,0,0]
    c['static_HMD_fixture'] &= bool(eyes) and all(abs(x['position'][1]-center[1])<1e-6 and abs(x['position'][2]-center[2])<1e-6 for x in eyes)
    errors={'csv_position_m':0.,'world_position_m':0.,'quaternion':0.}
    complete=bool(poses) and bool(sequence)
    for x in poses:
        if x['frame'] not in anchors or not sequence:complete=False;continue
        row=sequence[min(x['frame']-1,len(sequence)-1)];hand=row[14:27];a=anchors[x['frame']]
        errors['csv_position_m']=max(errors['csv_position_m'],max(abs(u-v) for u,v in zip(hand[:3],x['grip_position'])))
        hp=hand[:3];hq=None
        if pose_expectation in ('calibrated','calibrated-attack','calibrated-release','controller-speed','head-combat','right-speed','guard-posture','melee-repeat','calibrated-prefix'):
            from grip_preview_math import calibrated
            hp,hq,both=calibrated(row,profile)
        local=[hp[0]-center[0],hp[1]-center[1],-(hp[2]-center[2])]
        delta=rotate(a['orientation'],[u+v for u,v in zip(local,a['camera_offset'])])
        errors['world_position_m']=max(errors['world_position_m'],max(abs((q-r)/4294967296-d) for q,r,d in zip(x['q32'],a['camera_q32'],delta)))
        yaw,pitch,roll=[math.radians(v)/2 for v in hand[3:6]]
        q=multiply(multiply([0,math.sin(yaw),0,math.cos(yaw)],[math.sin(pitch),0,0,math.cos(pitch)]),[0,0,math.sin(roll),math.cos(roll)])
        if hq is not None:q=hq
        q=normalize(multiply(a['orientation'],[-q[0],-q[1],q[2],q[3]]))
        errors['quaternion']=max(errors['quaternion'],min(max(abs(u-v) for u,v in zip(q,x['orientation'])),max(abs(u+v) for u,v in zip(q,x['orientation']))))
    c['independent_CSV_pose_conversion']=complete and errors['csv_position_m']<1e-6 and errors['world_position_m']<1e-5 and errors['quaternion']<1e-5
    return c,errors

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--active-exit',action='store_true');p.add_argument('--pose-expectation',choices=['moving','single-swing','two-swing','two-swing-cooldown','head-gesture','vertical-stick','calibrated','calibrated-attack','calibrated-release','controller-speed','head-combat','right-speed','guard-posture','melee-repeat','calibrated-prefix'],default='moving');a=p.parse_args();launch=a.probe.parent
    native=rows(a.probe/'view-resource-probe.jsonl');gpu=rows(launch/'vulkan.jsonl');xr=rows(launch/'native-xr/openxr.jsonl')
    sequence=[[float(v) for v in line.split(',')] for line in (launch/'mock-hands.csv').read_text().splitlines() if line and not line.startswith('#')]
    profile=json.loads((a.probe/'grip-preview.json').read_text()) if a.pose_expectation in ('calibrated','calibrated-attack','calibrated-release','controller-speed','head-combat','right-speed','guard-posture','melee-repeat','calibrated-prefix') else None
    checks,errors=check(native,gpu,xr,sequence,a.pose_expectation,profile,a.active_exit)
    if a.active_exit:
        report=json.loads((a.probe/'active-xr-exit-checks.json').read_text());checks['explicit_active_exit_audited']=report['status']=='PASS' and (a.probe/'world-active-xr-exit.txt').read_text().strip()=='EVR_WORLD_ACTIVE_XR_EXIT_1' and not (a.probe/'controller-ui-fixture.txt').exists() and a.pose_expectation=='calibrated-prefix'
    elif a.pose_expectation=='calibrated-prefix':raise ValueError('prefix needs explicit active exit')
    if a.pose_expectation=='single-swing':checks['exact_single_swing_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/single-swing-hands.csv').read_bytes()
    if a.pose_expectation in ('two-swing','two-swing-cooldown'):checks['exact_two_swing_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env'/('two-swing-cooldown-hands.csv' if a.pose_expectation=='two-swing-cooldown' else 'two-swing-hands.csv')).read_bytes()
    if a.pose_expectation=='vertical-stick':
        checks['exact_vertical_stick_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/world-vertical-stick-hands.csv').read_bytes() and (a.probe/'controller-ui-fixture.txt').read_text().strip()=='EVR_VERTICAL_STICK_1'
    if a.pose_expectation=='head-gesture':checks['exact_head_gesture_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/world-head-gesture-hands.csv').read_bytes()
    if a.pose_expectation=='calibrated-attack':checks['exact_calibrated_attack_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/two-hand-attack-hands.csv').read_bytes()
    if a.pose_expectation=='melee-repeat':
        if (a.probe/'world-melee-session.txt').read_text().strip()!='EVR_MELEE_SESSION_REPEAT_2':raise ValueError('explicit measured repeat required')
        from generate_melee_repeat_fixture import verify
        verify(launch/'mock-hands.csv',profile)
    if a.pose_expectation=='right-speed':checks['exact_right_speed_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/right-hand-speed-hands.csv').read_bytes()
    if a.pose_expectation=='head-combat':checks['exact_head_combat_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/head-combat-hands.csv').read_bytes()
    if a.pose_expectation=='controller-speed':checks['exact_controller_speed_fixture']=(launch/'mock-hands.csv').read_bytes()==(Path(__file__).resolve().parents[1]/'test_env/controller-speed-late-hands.csv').read_bytes()
    if a.pose_expectation=='calibrated-release':checks['exact_calibrated_release_fixture']=(launch/'mock-hands.csv').read_bytes() in [(Path(__file__).resolve().parents[1]/'test_env'/f'two-hand-release-{start}.csv').read_bytes() for start in (441,445)]
    if a.pose_expectation=='guard-posture':
        if (a.probe/'controller-ui-fixture.txt').read_text().strip()!='EVR_MOTION_GUARD_1':raise ValueError('explicit posture fixture required')
        from generate_guard_posture_fixture import verify
        verify(launch/'mock-hands.csv',profile,delay=350)
    checks['Vulkan_Core_Sync_clean']=(launch/'validation.log').stat().st_size==0
    checks['runner_and_survival_pass']=json.loads((a.probe/'probe-status.json').read_text())['status']=='PASS'
    checks['normal_exit']=json.loads((launch/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0
    if a.negative_tests:
        base_valid=all(check(native,gpu,xr,sequence,a.pose_expectation,profile,a.active_exit)[0].values())
        import copy
        for name,event,field,value in [('wrong_calibration','motion_visual_created','weapon_calibration','unknown'),('wrong_eye_clone','head_visibility_payload','hand_instance',0),('missing_update','motion_visual_pose','serial',0),('original_changed','motion_visual_pose','source_unchanged',False),('world_pose_wrong','motion_visual_pose','q32',[0,0,0]),('visible_native','motion_visual_created','native_mask',1),('no_cleanup','motion_visual_disposed','generation_invalidated',False)]:
            altered=copy.deepcopy(native)
            for e in altered:
                if e['event']==event:e[field]=value
            checks['reject_'+name]=base_valid and not all(check(altered,gpu,xr,sequence,a.pose_expectation,profile,a.active_exit)[0].values())
    (a.probe/'motion-visual-checks.json').write_text(json.dumps(dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},errors=errors,limitation=__doc__),indent=2)+'\n')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print(errors);print('PASS',sum(bool(v) for v in checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
