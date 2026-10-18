"""Native UI -> standard OpenXR Quad evidence. Mock does not perform compositor rendering.

Pixel tests use acquired Runtime images read back AFTER GPU completion. Optional previews
are offline reference composites from real separate eye images, not Runtime captures.
"""
import argparse
import json
import hashlib
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw


def rows(path):
    return [json.loads(line) for line in path.read_text(encoding='utf-8-sig').splitlines() if line.strip()]


def decode(x):
    return np.where(x <= .04045, x/12.92, ((x+.055)/1.055)**2.4)


def encode(x):
    return np.where(x <= .0031308, x*12.92, 1.055*np.maximum(x,0)**(1/2.4)-.055)


def rotation(q):
    x,y,z,w=q
    return np.array([[1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)],
                     [2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)],
                     [2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)]])


def preview(directory,frame,ui,head,eye_events,quad):
    outputs=[]
    rh=rotation(head['orientation'])
    plane_width=quad['width_m'];plane_height=plane_width*ui.shape[0]/ui.shape[1]
    for e in sorted(eye_events,key=lambda e:e['eye']):
        image=Image.open(directory/f'frame-{frame}-eye-{e["eye"]}.ppm').convert('RGB')
        width,height=image.size
        left,right,up,down=np.tan(e['fov'])
        y,x=np.mgrid[:height,:width]
        rays=np.stack((left+(right-left)*(x+.5)/width,up-(up-down)*(y+.5)/height,-np.ones_like(x)),axis=-1)
        rays=rays@(rh.T@rotation(e['orientation'])).T
        origin=rh.T@(np.array(e['position'])-head['position'])
        distance=(-quad['distance_m']-origin[2])/rays[:,:,2]
        hit=origin+rays*distance[:,:,None]
        uv=np.stack((hit[:,:,0]/plane_width+.5,.5-hit[:,:,1]/plane_height),axis=-1)
        mask=(distance>0)&(uv[:,:,0]>=0)&(uv[:,:,0]<1)&(uv[:,:,1]>=0)&(uv[:,:,1]<1)
        px=np.clip((uv[:,:,0]*ui.shape[1]).astype(int),0,ui.shape[1]-1)
        py=np.clip((uv[:,:,1]*ui.shape[0]).astype(int),0,ui.shape[0]-1)
        sampled=ui[py,px].astype(float)/255
        world=decode(np.array(image).astype(float)/255)
        rgb=decode(sampled[:,:,:3])+world*(1-sampled[:,:,3:4])
        world[mask]=rgb[mask]
        outputs.append(Image.fromarray(np.uint8(np.round(np.clip(encode(world),0,1)*255))))
    if len(outputs)==2:
        canvas=Image.new('RGB',(outputs[0].width+outputs[1].width,max(i.height for i in outputs)+24))
        canvas.paste(outputs[0],(0,24));canvas.paste(outputs[1],(outputs[0].width,24))
        ImageDraw.Draw(canvas).text((8,5),'Offline reference composition: real eye images + real XR UI | NOT a Runtime compositor capture',fill='white')
        canvas.save(directory/f'ui-reference-frame-{frame}.png')


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('launch',type=Path);p.add_argument('--require-cleanup',action='store_true');p.add_argument('--previews',action='store_true')
    p.add_argument('--active-contact-owner-probe',type=Path,help='Independent successful same-raw active exit proof; audit available captured UI images, not nonexistent future midpoint/end captures')
    args=p.parse_args();directory=args.launch/'native-xr';events=rows(directory/'openxr.jsonl')
    by=lambda name:[e for e in events if e.get('event')==name]
    pairs=by('native_game_pair_submitted');proof=by('native_ui_frame_evidence');gpu=by('native_ui_gpu_complete');quads=by('ui_quad_submitted')
    ids=[e['frame'] for e in pairs];gpu_map={e['frame']:e for e in gpu};quad_map={e['frame']:e for e in quads}
    policy=by('native_xr_completion_policy');async_policy=len(policy)==1 and policy[0]['async'] is True and policy[0]['created_queue_count']==1
    checks={}
    checks['native_stereo_pairs_present']=len(ids)>0 and len(set(ids))==len(ids)
    checks['each_pair_has_one_gpu_ui_and_quad']=len(gpu)==len(quads)==len(ids) and [e['frame'] for e in gpu]==ids and [e['frame'] for e in quads]==ids
    checks['ui_not_disabled_or_omitted']=not by('native_ui_disabled') and not by('native_ui_omitted')
    checks['ui_and_world_same_frame_proven']=len(proof)==len(ids)
    for e in proof:
        matches=[t for t in e['candidates'] if t['present']==e['begin_present'] and t['presented_frame']==t['present']
                 and t['version']==t['submitted']==t['presented_version'] and t['valid'] and not t['failed'] and t['queue_match'] and t['ui_thread']==e['xr_thread']]
        complete=gpu_map.get(e['frame'])
        ok=(len(matches)==1 and e['current_present']==e['begin_present']+1 and complete is not None)
        if ok:
            t=matches[0]
            ok=(complete['begin_present']==complete['source_present']==t['present'] and complete['source_version']==t['version'] and complete['source_image']==t['image'] and (complete['gpu_fence_complete'] or (complete.get('async_completion') is True and async_policy)))
        checks['ui_and_world_same_frame_proven'] &= ok
    pair_map={e['frame']:e for e in pairs}
    checks['ui_fence_before_end_frame']=bool(quads) and all(e['frame'] in gpu_map and e['frame'] in pair_map and gpu_map[e['frame']]['us']<e['us']<pair_map[e['frame']]['us'] for e in quads)
    checks['quad_both_eyes_view_space_premultiplied']=bool(quads) and all(e['layers']==2 and e['premultiplied'] and e['space']=='VIEW' for e in quads)
    checks['all_runtime_slots_cycle']=len({e['image_index'] for e in gpu})==3 # pinned Mock returns 3 slots
    stats=[]
    for path in sorted(directory.glob('ui-frame-*.json')):
        m=json.loads(path.read_text(encoding='utf-8-sig'));f=m['frame'];size=m['width']*m['height']*4
        native=np.fromfile(path.with_name(path.stem+'-native.rgba8'),dtype=np.uint8)
        actual=np.fromfile(path.with_name(path.stem+'-xr.rgba8'),dtype=np.uint8)
        checks[f'{f}_raw_size']=native.size==actual.size==size
        if not checks[f'{f}_raw_size']:continue
        native=native.reshape(m['height'],m['width'],4);actual=actual.reshape(native.shape)
        n=native.astype(float)/255;a=n[:,:,3:4]
        straight=np.divide(n[:,:,:3],a,out=n[:,:,:3].copy(),where=a>0)
        linear=decode(straight)*np.where(a>0,a,1)
        expected=np.round(encode(np.clip(linear,0,1))*255).astype(np.int16)
        error=np.abs(expected-actual[:,:,:3].astype(np.int16))
        checks[f'{f}_alpha_exact']=np.array_equal(native[:,:,3],actual[:,:,3])
        checks[f'{f}_color_conversion']=int(error.max())<=2
        checks[f'{f}_ui_visible']=np.count_nonzero(actual[:,:,:3])>size//200
        # Native menus may be fullscreen (backpack) or an overlay (altar).
        # The source determines coverage; every image still requires visibility,
        # exact alpha and independent RGB conversion. HUD is always an overlay.
        menu_image=m.get('fixture_capture_tag') in (1,3,4,5)
        if menu_image and np.all(native[:,:,3]==255):
            checks[f'{f}_native_fullscreen_menu_coverage']=bool(np.all(actual[:,:,3]==255))
        else:
            checks[f'{f}_transparent_world_area']=np.count_nonzero(actual[:,:,3]==0)>size//40
            checks[f'{f}_partial_coverage']=np.count_nonzero((actual[:,:,3]>0)&(actual[:,:,3]<255))>100
        checks[f'{f}_capture_matches_submission']=all(m[k]==gpu_map.get(f,{}).get(k) for k in ('source_image','source_present','source_version','xr_image','image_index','width','height'))
        stats.append({'frame':f,'width':m['width'],'height':m['height'],'max_rgb_error_bytes':int(error.max()),'transparent_pixels':int(np.count_nonzero(actual[:,:,3]==0)),'opaque_pixels':int(np.count_nonzero(actual[:,:,3]==255))})
        # Store a preview with correct straight sRGB for standard PNG alpha viewers.
        linear_actual=decode(actual[:,:,:3].astype(float)/255)
        straight_actual=np.divide(linear_actual,a,out=np.zeros_like(linear_actual),where=a>0)
        png=actual.copy();png[:,:,:3]=np.uint8(np.round(np.clip(encode(straight_actual),0,1)*255))
        Image.fromarray(png).save(path.with_name(path.stem+'-ui.png'))
        if args.previews and f in quad_map:
            head=next(e for e in by('hmd') if e['frame']==f)
            eye=[e for e in by('runtime_eye_pose') if e['frame']==f]
            preview(directory,f,actual,head,eye,quad_map[f])
    if args.active_contact_owner_probe:
        probe=args.active_contact_owner_probe;report=json.loads((probe/'active-contact-owner-checks.json').read_text())
        exact=probe.resolve().parent==args.launch.resolve() and report['status']=='PASS' and bool(report['checks']) and all(bool(v) for v in report['checks'].values())
        for source in report['sources']:
            path=Path(source['path']);exact &= path.resolve().is_relative_to(args.launch.resolve()) and hashlib.sha256(path.read_bytes()).hexdigest()==source['sha256']
        exact &= (probe/'world-active-contact-owner.txt').read_text().strip()=='EVR_ACTIVE_CONTACT_OWNER_1'
        checks['independent_active_exit_same_raw']=exact and args.require_cleanup
        checks['first_available_UI_image_captured_before_interrupted_end']=bool(ids) and bool(stats) and ids[0] in {s['frame'] for s in stats} and all(s['frame'] in ids for s in stats)
    else:
        checks['first_middle_last_images_captured']=bool(ids) and {ids[0],ids[len(ids)//2-1],ids[-1]}<={s['frame'] for s in stats}
    validation=args.launch/'validation.log'
    checks['vulkan_validation_no_messages']=validation.exists() and validation.stat().st_size==0
    if args.require_cleanup:
        exit_record=json.loads((args.launch/'process-exit.json').read_text(encoding='utf-8-sig'))
        checks['normal_game_exit']=exit_record.get('exit_code')==0
        checks['ui_transfer_destroyed']=len(by('native_ui_transfer_destroyed'))==1
        checks['ui_swapchain_destroyed']=len(by('ui_swapchain_destroyed'))==1 and by('ui_swapchain_destroyed')[0]['code']==0
        checks['no_client_cleanup_error']=not by('cleanup_error') and not by('ui_cleanup_deferred')
        mock=rows(args.launch/'mock.jsonl')
        checks['mock_instance_session_destroyed']=sum(e['event']=='instance_destroyed' for e in mock)==sum(e['event']=='session_destroyed' for e in mock)==1
        checks['swapchains_released_before_destroy']=len([e for e in mock if e['event']=='swapchain_destroyed'])==3 and all(e['pending_phase']==0 for e in mock if e['event']=='swapchain_destroyed')
        gpu_events=rows(args.launch/'vulkan.jsonl')
        cleanup=[e for e in gpu_events if e['event']=='ui_capture_cleanup']
        checks['ui_native_targets_cleanup']=len(cleanup)==1 and cleanup[0]['targets']==3 and cleanup[0]['idle_result']==0
    report={'checks':{k:'PASS' if bool(v) else 'FAIL' for k,v in checks.items()},'images':stats,'pairs':len(ids),'limit':__doc__}
    (directory/'ui-xr-checks.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    for k,v in report['checks'].items():print(v,k)
    print('PASS',sum(bool(v) for v in checks.values()),'FAIL',sum(not bool(v) for v in checks.values()))
    return int(not all(checks.values()))

if __name__=='__main__':raise SystemExit(main())
