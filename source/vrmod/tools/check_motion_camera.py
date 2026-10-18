"""Actual jump motion plus stereo camera stability. Does not certify every animation."""
import argparse,json,pathlib,math
from check_root_camera import check_root
from check_native_calibration import Image,ImageChops,ImageStat

def check_motion_images(images,extents):
 checks={};stats={}
 for tag,pair in images.items():
  checks[f'image_{tag}_two_runtime_sized_eyes']=len(pair)==2 and len(extents)==2 and all(im.size==extent for im,extent in zip(pair,extents))
  if not checks[f'image_{tag}_two_runtime_sized_eyes']:continue
  mean=[ImageStat.Stat(im).mean for im in pair];variance=[ImageStat.Stat(im).var for im in pair]
  central=[]
  for im in pair:
   w,h=im.size;hist=im.crop((w//4,h//4,3*w//4,3*h//4)).convert('L').histogram()
   central.append(sum(hist[9:])/sum(hist))
  stats[tag]=dict(mean=mean,variance=variance,central_visible_fraction=central,stereo_difference=ImageStat.Stat(ImageChops.difference(*pair)).mean)
  checks[f'image_{tag}_nonblack_nonuniform']=all(sum(m)>9 and sum(v)>12 for m,v in zip(mean,variance))
  checks[f'image_{tag}_eyes_differ']=sum(stats[tag]['stereo_difference'])>0
  # Controlled lit-room diagnostic only. This catches a black scene with a
  # surviving glove, which global mean/variance alone wrongly accept.
  checks[f'image_{tag}_lit_test_scene_not_missing']=all(v>.05 for v in central)
 checks['air_and_landing_image_pair_complete']=set(images)=={1,2}
 if set(images)=={1,2} and all(len(v)==2 for v in images.values()):
  checks['motion_images_updated_both_eyes']=all(images[1][eye].size==images[2][eye].size and sum(ImageStat.Stat(ImageChops.difference(images[1][eye],images[2][eye])).mean)>0 for eye in (0,1))
 return checks,stats


def check(probe):
 rows=[json.loads(s) for s in (probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 events=lambda n:[x for x in rows if x['event']==n]
 roots=events('root_camera_sample');done=events('live_native_pair_complete');motion=events('controller_motion_sample');phases=events('controller_motion_phase')
 anchors={x['recorded_xr_frame']:x for x in events('recorded_xr_anchor')};y=[x['stable_q32'][1]*2**-32 for x in roots]
 first=phases[0]['root'] if phases else [0,0,0]
 packets=events('controller_fixture_packet_polled');jump=[x for x in packets if x['buttons']==4096];config=events('controller_fixture_connected')
 glide=bool(config) and config[0].get('glide_sequence',False)
 yaw=[x['orientation'] for x in anchors.values()]
 checks=check_root(rows,2)
 checks.update({
  '600_completed_native_stereo_pairs':len(done)==600,
  'actual_first_person_all_frames':bool(roots) and all(not x['synthetic_presentation'] and x['camera_id']==x['presentation_id'] and x['camera_id'] in ((4,19,25,43,44) if glide else (4,19,43,44)) for x in roots),
  'native_motion_observed_and_ended':bool(phases) and phases[-1]['phase']==9 and all(x['phase']!=10 for x in phases),
  'real_root_rise_and_landing':bool(y) and max(y)-min(y)>.3 and abs(y[-1]-first[1])<.03,
  'no_horizontal_input_or_drift':bool(motion) and max(math.hypot(x['root'][0]-first[0],x['root'][2]-first[2]) for x in motion)<.03,
  'air_and_ground_actor_states_observed':any(not x['actor_state']&1 and x['actor_state']&((1<<1)|(1<<32)) for x in motion) and any(x['actor_state']&1 for x in motion),
  'all_motion_snapshots_valid':bool(motion) and all(x['valid'] and x['entity']==phases[0]['entity'] for x in motion),
  'no_animation_yaw_pitch_roll_added_to_anchor':bool(yaw) and max(max(abs(a-b) for a,b in zip(q,yaw[0])) for q in yaw)<1e-6,
  'no_native_or_fixture_failure':not any('failure' in x['event'] for x in rows),
 })
 if glide:
  del checks['no_horizontal_input_or_drift']
  checks['native_glide_actor_and_camera_observed']=any(x['actor_state']&(1<<37) for x in motion) and any(x['camera_id']==25 for x in roots)
  checks['no_analog_controls_in_native_sender']=all(x['movement_stick']==[0,0] for x in events('input_sender_sample')) and bool(events('input_sender_sample'))
  checks['real_glide_world_movement_tracked']=.1<max(math.hypot(x['root'][0]-first[0],x['root'][2]-first[2]) for x in motion)<20
 evidence=dict(root_height_min=min(y) if y else None,root_height_max=max(y) if y else None,root_height_range=max(y)-min(y) if y else None,native_motion_samples=len(motion))
 if config and config[0].get('double_jump'):
  checks['two_A_edges_actually_consumed']=len(jump)==2
  if len(jump)==2:
   t=jump[1]['us'];before=[x for x in motion if t-350000<=x['us']<t];after=[x for x in motion if t<=x['us']<=t+400000]
   def velocity(a,b):return (b['root'][1]-a['root'][1])*1e6/(b['us']-a['us'])
   pre=velocity(*before[-2:]) if len(before)>=2 else None
   post=[velocity(a,b) for a,b in zip(after,after[1:]) if b['us']>a['us']]
   gain=max(post)-pre if post and pre is not None else None
   checks['second_jump_has_observed_upward_velocity_gain']=gain is not None and gain>2 and max(post)>2
   evidence.update(second_press_us=t,pre_second_velocity=pre,post_second_velocities=post,observed_velocity_gain=gain)
 layer=[json.loads(x) for x in (probe.parent/'native-xr/openxr.jsonl').read_text(encoding='utf-8').splitlines()]
 captures={x['tag']:x for x in layer if x['event']=='native_fixture_capture_complete'}
 rmap={x['frame']:x for x in roots}
 checks['air_and_landing_images_submitted']=set(captures)=={1,2} and all(x['ui_submitted'] for x in captures.values())
 if checks['air_and_landing_images_submitted']:
  heights=[rmap[captures[t]['frame']]['stable_q32'][1]*2**-32 for t in (1,2)]
  if glide:checks['capture_actual_glider_camera']=rmap[captures[1]['frame']]['camera_id']==25
  checks['capture_actual_airborne_and_landed_root']=heights[0]>first[1]+.1 and abs(heights[1]-first[1])<.03;evidence['capture_root_heights']=heights
 images={}
 extents=[(x['width'],x['height']) for x in layer if x['event']=='swapchain']
 for tag,capture in captures.items():
  paths=[probe.parent/'native-xr'/f'frame-{capture["frame"]}-eye-{eye}.ppm' for eye in (0,1)]
  images[tag]=[Image.open(path).convert('RGB') for path in paths if path.exists()]
 image_checks,image_stats=check_motion_images(images,extents);checks.update(image_checks);evidence['image_stats']=image_stats
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},evidence=evidence,limitation=__doc__)
 (probe/'motion-camera-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 print(sum(bool(x) for x in checks.values()),'PASS /',sum(not x for x in checks.values()),'FAIL')
 return all(checks.values())
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args();raise SystemExit(not check(a.probe))
