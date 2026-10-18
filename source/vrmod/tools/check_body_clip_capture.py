"""Validate active body clipping from an immutable native GPU capture.

Manifest event/slot values are established from that capture's native visibility
buffers, never reused across launches. Separate clean live validation is required.
"""
import argparse,hashlib,json,math,struct
from pathlib import Path
from check_body_clip_live import SOURCES,rows
CASES=('before','control','after','restored','restored_again')
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def main():
    ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('manifest',type=Path);ap.add_argument('--output',type=Path);a=ap.parse_args()
    cfg=json.loads(a.manifest.read_text(encoding='utf-8-sig'));base=a.manifest.parent
    probe=base/cfg['probe'];native=rows(probe/'view-resource-probe.jsonl')
    selected=[m['handle']&0xffffff for e in native if e['event']=='local_equipment_observation' for m in e['models'] if m['part']==cfg['part'] and m['handle']]
    if not selected or len(set(selected))!=1:raise ValueError('stationary selected model identity required')
    wanted=selected[0];distance=f32(float((probe/'world-body-clip.txt').read_text().split()[1]))
    writes=[e for e in native if e['event']=='body_clip_payload'];owned={int(e['address'],16) for e in writes}
    if len(owned)!=1:raise ValueError('single immutable owned address required')
    checks={};details={};clips={};local={i for e in writes for i in e['indices']}
    def same_output(p,rr,event,stride,prefix):
        mesh=[r for r in rr if r.get('mesh_event')==event]
        checks[prefix+'_reviewed_output_layout']=len(mesh)==5 and all(r['vertex_stride']==stride and r['index_stride']==2 and r['base_vertex']==0 and r['component_count']==4 and r['component_bytes']==4 and r['component_type']==1 for r in mesh)
        if not checks[prefix+'_reviewed_output_layout']:raise ValueError('post-VS packing changed')
        raw={r['case']:(p/r['vertices_file']).read_bytes() for r in mesh};control=raw['control'];other=raw['after']
        checks[prefix+'_complete_vertex_extent']=len(control)>0 and len(control)%stride==0 and len({len(b) for b in raw.values()})==1
        checks[prefix+'_original_outputs_restored']=all(raw[c]==control for c in ('before','restored','restored_again'))
        checks[prefix+'_indices_preserved']=len({(p/f'{c}_mesh_{event}_indices.bin').read_bytes() for c in CASES})==1
        checks[prefix+'_native_position_preserved']=all(control[n:n+16]==other[n:n+16] for n in range(0,len(control),stride))
        if stride!=16:
            checks[prefix+'_native_flat_preserved']=all(control[n+20:n+24]==other[n+16:n+20] for n in range(0,len(control),stride))
            checks[prefix+'_native_varyings_preserved']=all(control[n+32:n+stride]==other[n+32:n+stride] for n in range(0,len(control),stride))
        else:checks[prefix+'_all_shadow_outputs_unchanged']=control==other
        return control
    def identity(p,rr,event,index,slot,prefix):
        br=[r for r in rr if r.get('buffer_event')==event and r['vertex_resource_index']==index]
        if len(br)!=5:raise ValueError('complete native model-list evidence required')
        raw={r['case']:(p/r['file']).read_bytes() for r in br};b=raw['control']
        checks[prefix+'_model_list_same_during_ablation']=raw['before']==b and raw['after']==b
        # GPU atomic append order can vary on a later unmodified replay.
        # Preserve the full record multiset and exact selected indirect identity.
        records=sorted(b[n:n+8] for n in range(0,len(b),8))
        checks[prefix+'_restored_model_records_preserved']=all(sorted(v[n:n+8] for n in range(0,len(v),8))==records for v in raw.values())
        checks[prefix+'_selected_indirect_slot_stable']=all(v[slot*8:(slot+1)*8]==b[slot*8:(slot+1)*8] for v in raw.values())
        if len(b)<(slot+1)*8:raise ValueError('indirect slot beyond buffer')
        return struct.unpack_from('<II',b,slot*8)[1]
    for kind,g in cfg['groups'].items():
        p=base/g['comparison'];rr=rows(p/'shader-comparison.jsonl');bindings=base/cfg['bindings']
        checks[kind+'_active_and_native_module_hashes']=hashlib.sha256((p/'original_1.spv').read_bytes()).hexdigest()==SOURCES[kind][1] and hashlib.sha256((p/'replacement_1.spv').read_bytes()).hexdigest()==SOURCES[kind][0]
        for phase,event in g['events'].items():
            prefix=kind+'_'+phase;raw=same_output(p,rr,event,g['stride'],prefix)
            clip=[struct.unpack_from('<f',raw,n+16)[0] for n in range(0,len(raw),g['stride'])]
            w=[struct.unpack_from('<f',raw,n+12)[0] for n in range(0,len(raw),g['stride'])]
            constants=(bindings/f'event_{event}_stage_0_constant_0.bin').read_bytes();offset=632 if kind=='color' else 472
            address=struct.unpack_from('<Q',constants,offset)[0]
            checks[prefix+'_gpu_address_scope']=address==(0 if phase=='original' else next(iter(owned)))
            if phase=='original':checks[prefix+'_clip_inactive']=all(v==1 for v in clip)
            else:
                checks[prefix+'_exact_local_model_identity']=identity(p,rr,event,g['buffer_index'],g['slots'][phase],prefix)==wanted
                checks[prefix+'_exact_clip_formula']=clip==[f32(v-distance) for v in w] and all(math.isfinite(v) for v in clip)
                checks[prefix+'_partial_mesh_retained']=0<sum(v>=0 for v in clip)<len(clip)
                details[prefix]={'vertices':len(clip),'retained':sum(v>=0 for v in clip),'clipped':sum(v<0 for v in clip)};clips[kind,phase]=clip
        same_output(p,rr,cfg['shadow_event'],16,kind+'_shadow')
        checks[kind+'_native_mirror_unchanged']=len({(p/f'{c}_texture_{cfg["mirror"]}.bin').read_bytes() for c in CASES})==1
    checks['depth_color_clip_values_match_both_eyes']=all(clips['depth',phase]==clips['color',phase] for phase in ('left','right'))
    g=cfg['other'];p=base/g['comparison'];rr=rows(p/'shader-comparison.jsonl')
    raw=same_output(p,rr,g['event'],192,'unselected_model')
    identity_other=identity(p,rr,g['event'],8,g['slot'],'unselected_model')
    checks['other_model_is_outside_all_local_targets']=identity_other not in local
    checks['other_model_clip_inactive']=all(struct.unpack_from('<f',raw,n+16)[0]==1 for n in range(0,len(raw),192))
    report={'checks':checks,'passed':sum(checks.values()),'failed':sum(not x for x in checks.values()),'model':wanted,'other_model':identity_other,'distance':distance,'vertices':details,'limits':'Captured current armour only; unselected scene model is not another player. Original and one shadow draw preserved. No proof of all shader variants, glider animation, physical VR comfort, or image exactness across temporal replay. Core+Sync acceptance comes from separate live run.'}
    (a.output or base/'body-active-gpu-check.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print(f"{report['passed']} PASS / {report['failed']} FAIL")
    return int(report['failed']>0)
if __name__=='__main__':raise SystemExit(main())
