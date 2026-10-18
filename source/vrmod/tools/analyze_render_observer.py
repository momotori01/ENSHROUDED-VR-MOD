"""Decode bounded render-entry observations offline; no process access or writes.

Reports evidence and unknowns separately. A valid camera observation is not a
native-stereo rendering PASS and the renderer's view counter isn't a frame ID.
"""
import argparse
import copy
import json
import math
import pathlib
import struct
from analyze_camera_dump import transform


def decode_sample(event):
    if not event.get('query_read'):
        raise ValueError('entry stack was unreadable')
    components={item['index']:item for item in event['components']}
    def data(index, minimum):
        item=components[index]
        raw=bytes.fromhex(item['bytes']) if item['readable'] else b''
        if len(raw)<minimum:
            raise ValueError(f'entry region {index} incomplete')
        return raw
    params=data(3,0x80)
    camera=transform(params[0x38:0x70])
    if not camera or not .99<camera['quaternion_norm']<1.01 or not all(.001<x<1000 for x in camera['scale']):
        raise ValueError('render camera transform invalid')
    near,far,fov=struct.unpack_from('<3f',params,0x70)
    kind=params[0x7c]  # e4a71 uses MOVZX byte; adjacent bytes are independent state
    if not all(math.isfinite(x) for x in (near,far,fov)) or not 0<near<far:
        raise ValueError('render camera clip planes invalid')
    if kind not in (0,1) or not (0<fov<math.pi if kind==0 else fov>0):
        raise ValueError('unrecognized game camera projection')
    camera.update(near=near,far=far,projection_type=kind,fov_y_radians=fov if kind==0 else None,ortho_extent=fov if kind==1 else None)
    view=data(2,16)
    scene_address=struct.unpack_from('<Q',view,8)[0]
    target=data(4,24)
    row=dict(sample=event['sample'],tick_ms=event['tick_ms'],thread=event['thread'],
             return_rva_decimal=event.get('return_rva'),view_address=components[2]['address'],
             parameters_address=components[3]['address'],scene_address=hex(scene_address),camera=camera,
             target_descriptor=target[:24].hex(),target_qwords=[hex(x) for x in struct.unpack('<3Q',target[:24])])
    if len(view)>=0xb98 and view[0xb90]:
        previous=transform(view[0xa40:0xa78])
        row['history']=dict(valid=bool(view[0xb90]),previous_camera=previous,
            matches_current_camera=bool(previous and previous['position_q32']==camera['position_q32'] and previous['orientation']==camera['orientation']),
            previous_render_context_serial=struct.unpack_from('<Q',view,0x660)[0],
            target_ping_pong_index=struct.unpack_from('<I',view,0x5f0)[0],
            matrices={name:list(struct.unpack_from('<16f',view,offset)) for name,offset in
                [('world_view',0xa80),('camera_centered_view',0xac0),('projection',0xb00),('jittered_projection',0xb40)]})
        # CPU affine fourth-column padding contains unrelated bytes; sanitize
        # before JSON encoding and never use it for matrix comparisons.
        for name in ('world_view','camera_centered_view'):
            for i in (3,7,11):row['history']['matrices'][name][i]=0
            row['history']['matrices'][name][15]=1
        if not all(math.isfinite(x) for values in row['history']['matrices'].values() for x in values):
            raise ValueError('nonfinite history matrix')
    elif len(view)>=0xb98:
        row['history']=dict(valid=False)
    # Cross-check known ownership instead of assuming arbitrary nearby objects
    # belong to this view. scene+0x28 is the back-pointer assigned in d0060.
    scene=components.get(5)
    if scene and scene['readable']:
        raw=bytes.fromhex(scene['bytes'])
        if len(raw)>=0x30:
            row['scene_snapshot_matches_pointer']=int(scene['address'],16)==scene_address
            row['scene_view_back_pointer']=hex(struct.unpack_from('<Q',raw,0x28)[0])
            row['scene_view_back_pointer_matches']=row['scene_view_back_pointer']==row['view_address']
        if len(raw)>=0x8f8 and row.get('scene_snapshot_matches_pointer'):
            epoch,current,previous=struct.unpack_from('<3q',raw,0x8e0)
            row['scene_time']=dict(epoch_raw=epoch,current_relative_raw=current,previous_relative_raw=previous,
                                   delta_raw=current-previous,units='not yet runtime-calibrated')
    counter=components.get(6)
    if counter and counter['readable']:
        raw=bytes.fromhex(counter['bytes'])
        if len(raw)>=8:
            limit,count=struct.unpack_from('<2I',raw)
            row['render_view_admission']=dict(limit=limit,count_before_entry=count,simulation_frame_id=False)
    if 7 in components and components[7]['readable']:
        texture=data(7,12)
        width,height=struct.unpack_from('<2I',texture,4)
        row['target_extent']=dict(width=width,height=height,source='f923d0 texture record')
    if 8 in components and components[8]['readable']:
        row['render_context_serial']=struct.unpack_from('<Q',data(8,8))[0]
    if 9 in components and components[9]['readable']:
        wrapper=data(9,0xd0)
        owner=struct.unpack_from('<Q',view,0)[0]
        row['wrapper']=dict(address=components[9]['address'],
                            matches_view_owner=int(components[9]['address'],16)==owner,
                            scene_matches=struct.unpack_from('<Q',wrapper,0x70)[0]==scene_address,
                            view_matches=struct.unpack_from('<Q',wrapper,0x78)[0]==int(row['view_address'],16),
                            output_extent=dict(width=struct.unpack_from('<I',wrapper,0x18)[0],height=struct.unpack_from('<I',wrapper,0x1c)[0]),
                            render_scale=struct.unpack_from('<f',wrapper,0x20)[0])
        if not math.isfinite(row['wrapper']['render_scale']):
            raise ValueError('nonfinite game wrapper render scale')
    return row


def analyze(events):
    completed=[e for e in events if e['event']=='render_hook_complete']
    if not completed or not completed[-1].get('original_bytes_restored'):
        raise ValueError('original instruction restoration not confirmed')
    rows=[];errors=[]
    for event in events:
        if event['event']!='render_query_sample':continue
        try:rows.append(decode_sample(event))
        except (KeyError,ValueError,struct.error) as error:
            errors.append(dict(sample=event.get('sample'),error=str(error)))
    if not rows:raise ValueError('no valid render camera samples: '+str(errors))
    views=[]
    for address in sorted({r['view_address'] for r in rows}):
        group=[r for r in rows if r['view_address']==address]
        views.append(dict(address=address,samples=len(group),scene_addresses=sorted({r['scene_address'] for r in group}),
                          distinct_positions=len({tuple(r['camera']['position_q32']) for r in group}),
                          distinct_orientations=len({tuple(r['camera']['orientation']) for r in group}),
                          distinct_targets=len({r['target_descriptor'] for r in group}),
                          view_classification='unknown; requires gameplay/UI correlation'))
    return dict(valid_samples=len(rows),invalid_samples=len(errors),original_bytes_restored=True,
                stereo_rendering_verified=False,views=views,samples=rows,errors=errors)


def self_test():
    # Synthetic structures exercise field boundaries and ownership mismatch.
    params=bytearray(0x400)
    struct.pack_into('<3q4f3f',params,0x38,1<<40,2<<40,3<<40,0,0,0,1,1,1,1)
    struct.pack_into('<3fI',params,0x70,.01,10000,math.radians(65),0)
    params[0x7d]=2  # real menu uses 00 02 00 00; never read the tag as a uint32
    view=bytearray(0xb98);struct.pack_into('<Q',view,8,0x3000)
    view[0xa40:0xa78]=params[0x38:0x70]
    view[0xb90]=1
    for offset in (0xa80,0xac0,0xb00,0xb40):
        struct.pack_into('<16f',view,offset,*[1.0 if i%5==0 else 0.0 for i in range(16)])
    struct.pack_into('<f',view,0xa80+12,float('nan')) # padding is not matrix data
    scene=bytearray(0x9a8);struct.pack_into('<Q',scene,0x28,0x2000)
    blobs=[b'',b'\0'*64,view,params,b'\0'*24,scene,struct.pack('<2I',4,1)]
    sample=dict(event='render_query_sample',sample=0,tick_ms=1,thread=2,query_read=True,
                components=[dict(index=i,address=hex([0,0x1000,0x2000,0x4000,0x5000,0x3000,0x6000][i]),readable=bool(blob),bytes=blob.hex()) for i,blob in enumerate(blobs)])
    complete=dict(event='render_hook_complete',original_bytes_restored=True)
    report=analyze([sample,complete])
    assert report['samples'][0]['scene_view_back_pointer_matches'] and not report['stereo_rendering_verified']
    print('PASS synthetic perspective fields, ownership and evidence limits')
    for label,mutate in [
        ('missing restoration',lambda s,c:c.update(original_bytes_restored=False)),
        ('truncated camera',lambda s,c:s['components'][3].update(bytes='00')),
        ('unreadable camera',lambda s,c:s['components'][3].update(readable=False)),
        ('unknown projection tag',lambda s,c:s['components'][3].update(bytes=(params[:0x7c]+struct.pack('<I',7)+params[0x80:]).hex())),
        ('nonfinite FOV',lambda s,c:s['components'][3].update(bytes=(params[:0x78]+struct.pack('<f',float('nan'))+params[0x7c:]).hex()))]:
        s,c=copy.deepcopy(sample),copy.deepcopy(complete);mutate(s,c)
        try:analyze([s,c])
        except ValueError:print('PASS rejected',label)
        else:raise AssertionError(label)
    mismatch=copy.deepcopy(sample);mismatch['components'][5]['address']='0x3001'
    assert not decode_sample(mismatch)['scene_snapshot_matches_pointer']
    print('PASS mismatched scene identity reported')
    uninitialized=copy.deepcopy(sample)
    invalid_view=bytearray(view);invalid_view[0xb90]=0
    struct.pack_into('<f',invalid_view,0xb00,float('nan'))
    uninitialized['components'][2]['bytes']=invalid_view.hex()
    assert decode_sample(uninitialized)['history']==dict(valid=False)
    print('PASS inactive history and affine padding are excluded from matrix validity')
    extended=copy.deepcopy(sample)
    extended_view=bytearray(view);struct.pack_into('<Q',extended_view,0,0x7000)
    extended['components'][2]['bytes']=extended_view.hex()
    wrapper=bytearray(0xd0)
    struct.pack_into('<QQ',wrapper,0x70,0x3000,0x2000)
    struct.pack_into('<IIf',wrapper,0x18,1900,2100,1.0)
    extended['components'].append(dict(index=9,address='0x7000',readable=True,bytes=wrapper.hex()))
    owner=decode_sample(extended)['wrapper']
    assert owner['matches_view_owner'] and owner['view_matches'] and owner['scene_matches']
    extended['components'][-1]['address']='0x7001'
    assert not decode_sample(extended)['wrapper']['matches_view_owner']
    print('PASS wrapper ownership links and mismatch reported')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--launch',type=pathlib.Path)
    parser.add_argument('--observation',type=pathlib.Path)
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:self_test();return
    if not args.launch or not args.observation:parser.error('--launch and --observation required')
    launch=json.loads((args.launch/'launch.json').read_text(encoding='utf-8-sig'))
    audit=json.loads((args.observation/'loader-audit.json').read_text())
    if audit['pid']!=launch['pid']:raise ValueError('observation launch PID mismatch')
    events=[json.loads(line) for line in (args.observation/'render-observer.jsonl').read_text().splitlines()]
    report=analyze(events)
    (args.observation/'analysis.json').write_text(json.dumps(report,indent=2,allow_nan=False),encoding='utf8')
    print(json.dumps({key:value for key,value in report.items() if key!='samples'},indent=2))


if __name__=='__main__':main()
