"""Correlate raw native ImpactHierarchy, action resource and collider records.
This proves source ownership in the capture, not live geometry replacement or damage.
The earlier DespawnLink checker is retained unchanged as a rejected hypothesis.
"""
import argparse, copy, json, math, pathlib, struct

def u32(b, o=0): return struct.unpack_from('<I',b,o)[0]
def audit(rows, physics=False, lookup=False):
    events=lambda name:[r for r in rows if r.get('event')==name]
    hs=events('impact_hierarchy_sample');cs=events('sequence_collider_sample');assets=events('sequence_asset_snapshot');checks={}
    def check(k,v): checks[k]=bool(v)
    widths={'hierarchy_bytes':16,'sequence_event_bytes':24,'impact_bytes':20,'item_config_bytes':44}
    check('hierarchy_capture_abi',len(events('impact_hierarchy_enabled'))==1 and events('impact_hierarchy_enabled')[0].get('caller_rva')=='0x36b628' and events('impact_hierarchy_enabled')[0].get('query_bytes')==1216)
    check('collider_capture_abi',len(events('sequence_collider_enabled'))==1 and events('sequence_collider_enabled')[0].get('caller_rva')=='0x36be7e')
    check('read_only_stable_bounded',0<len(hs)<=1027 and 0<len(cs)<=1043 and all(r.get('stable_read') and r.get('read_only') for r in hs+cs+assets))
    check('raw_hierarchy_widths',bool(hs) and all(all(len(r.get(k,''))==2*n for k,n in widths.items()) for r in hs))
    check('no_observer_failure',not any(r['event'].endswith('_failure') for r in rows))
    for name,samples in [('impact_hierarchy',hs),('sequence_collider',cs)]:
        end=events(name+'_removed');check(name+'_restored',len(end)==1 and end[0].get('restored') and end[0].get('callbacks')==0 and end[0].get('failures')==0 and end[0].get('samples')==len(samples))
    # Select from raw event sequence, rather than the logged ownership booleans.
    owned=[r for r in hs if len(r['sequence_event_bytes'])==48 and u32(bytes.fromhex(r['sequence_event_bytes']))!=0]
    check('nonzero_sequence_impacts_observed',bool(owned))
    evidence=[];root_ok=item_ok=sequence_ok=asset_ok=collider_ok=fresh_ok=geometry_ok=physics_ok=True
    try:
        for h in owned:
            hierarchy=bytes.fromhex(h['hierarchy_bytes']);event=bytes.fromhex(h['sequence_event_bytes']);item=bytes.fromhex(h['item_config_bytes']);impact=bytes.fromhex(h['impact_bytes'])
            parent,root,pide,permission=struct.unpack('<4I',hierarchy);sequence=u32(event);sub=event[4];guid=event[8:24].hex();used=u32(item,8)
            root_ok &= h['hierarchy_present'] and root==h['root_entity']==h['latest_actor_entity']!=0 and parent==h['parent_entity']==root and h['entity']!=root
            item_ok &= h['item_config_present'] and used==h['latest_actor_item']!=0 and pide==u32(item,12)!=0 and permission==root
            sequence_ok &= sequence==h['sequence']==h['latest_actor_sequence']!=0
            fresh_ok &= h['latest_actor_active'] and 0<=h['milliseconds']-h['latest_actor_ms']<=200
            matches=[]
            for a in assets:
                if (a['sequence'],a['item'],a['entity'])!=(sequence,used,root):continue
                for s in a['subsequences']:
                    if s['index']!=sub:continue
                    for e in s['events']:
                        data=bytes.fromhex(e.get('spawn_impact',''))
                        if len(data)==488 and data[0x158:0x168].hex()==guid:matches.append((a,s,e,data))
            asset_ok &= len(matches)==1
            if len(matches)==1:
                a,s,e,d=matches[0];asset_ok &= a['stable_read'] and a['read_only'] and impact[:16]==d[0x148:0x158] and len(e['shapes'])==struct.unpack_from('<I',d,0x1a0)[0]>0
                for shape in e['shapes']:
                    b=bytes.fromhex(shape.get('box_data',''));asset_ok &= shape['type_hash']==0x88154c34 and len(b)==96 and b[:80].hex()==shape['base_data']
                    if len(b)==96:asset_ok &= all(math.isfinite(v) and v>0 for v in struct.unpack_from('<3f',b,80)) and b[92] in (0,1)
            pairs=[c for c in cs if c['entity']==h['entity'] and c['event_bytes']==event.hex() and c['thread']==h['thread'] and abs(c['milliseconds']-h['milliseconds'])<=20]
            collider_ok &= bool(pairs)
            if not pairs:continue
            c=min(pairs,key=lambda c:abs(c['us']-h['us']));raw=bytes.fromhex(c['collider_bytes']);transform=bytes.fromhex(c['transform_bytes'])
            collider_ok &= len(raw)==24 and struct.unpack_from('<Q',raw,8)[0]==c['collider_id']!=0 and c['sequence']==sequence and c['latest_actor_entity']==root and c['latest_actor_item']==used and c['latest_actor_sequence']==sequence
            xyz=[v/2**32 for v in struct.unpack_from('<3q',transform)];q=struct.unpack_from('<4f',transform,24);scale=struct.unpack_from('<3f',transform,40)
            geometry_ok &= all(math.isfinite(v) for v in xyz+list(q)+list(scale)) and abs(sum(v*v for v in q)-1)<1e-3 and min(scale)>0
            error=None
            if physics:
                b=bytes.fromhex(c.get('physics_current_bytes',''))
                if len(b)!=32:physics_ok=False
                else:
                    actual=struct.unpack('<8f',b)
                    # Native CVTSI2SS then MULSS by exact 2^-32, not double->float.
                    f32=lambda v:struct.unpack('<f',struct.pack('<f',v))[0]
                    expected=[f32(f32(v)*2**-32) for v in struct.unpack_from('<3q',transform)]+[scale[0]]+list(q)
                    error=max(abs(v-w) for v,w in zip(actual,expected));physics_ok &= all(math.isfinite(v) for v in actual) and actual==tuple(expected)
            evidence.append(dict(entity=h['entity'],root=root,item=used,weapon_pide=pide,sequence=sequence,subsequence=sub,event_guid=guid,collider=c['collider_id'],hierarchy_sample=h['sample'],collider_sample=c['sample'],gap_us=h['us']-c['us'],position=xyz,orientation=q,scale=scale,physics_error=error))
    except (KeyError,ValueError,struct.error,OverflowError):root_ok=item_ok=sequence_ok=asset_ok=collider_ok=fresh_ok=geometry_ok=physics_ok=False
    for name,value in [('raw_native_root_owner',root_ok),('item_and_weapon_pide_match',item_ok),('sequence_matches_native_action',sequence_ok),('matching_spawn_resource_and_box_shapes',asset_ok),('same_entity_event_native_collider',collider_ok),('actor_status_fresh_and_active',fresh_ok),('finite_native_geometry',geometry_ok)]:check(name,bool(owned) and value)
    if physics:check('physics_input_exact_native_conversion',bool(owned) and physics_ok)
    check('multiple_native_collider_updates',len({x['collider_sample'] for x in evidence})>=3)
    if lookup:
        bindings=events('impact_component_binding');ready=events('sequence_collider_ready_sample')
        check('dynamic_component_ids_from_query',bool(bindings) and all(r.get('query_pointer_match') and r.get('read_only') and 0<=r['hierarchy_component_id']<640 and 0<=r['item_component_id']<640 and r['hierarchy_component_id']!=r['item_component_id'] for r in bindings))
        direct_ok=pre_ok=True;first=[]
        for x in evidence:
            c=next(c for c in cs if c['sample']==x['collider_sample']);h=next(h for h in hs if h['sample']==x['hierarchy_sample']);o=c.get('owner_lookup',{})
            def matches(o,at):
                return o.get('ready') and o.get('stable') and o.get('hierarchy_bytes')==h['hierarchy_bytes'] and o.get('item_config_bytes')==h['item_config_bytes'] and any(b['world']==o.get('world') and b['hierarchy_component_id']==o.get('hierarchy_component_id') and b['item_component_id']==o.get('item_component_id') and b['us']<at for b in bindings)
            direct_ok &= matches(o,c['us'])
            candidates=[r for r in ready if r['entity']==x['entity'] and r['event_bytes']==c['event_bytes'] and r['thread']==c['thread'] and r['us']<c['us'] and ('ready_sample' not in c or c['ready_sample']==r['sample'])]
            candidates=[max(candidates,key=lambda r:r['us'])] if candidates else []
            pre_ok &= len(candidates)==1 and matches(candidates[0].get('owner_lookup',{}),candidates[0]['us'])
            if candidates:first.append(candidates[0])
        check('lookup_matches_later_direct_query_bytes',bool(evidence) and direct_ok)
        check('lookup_ready_before_each_physics_call',bool(evidence) and pre_ok)
        check('first_collider_creation_has_owner',bool(first) and min(first,key=lambda r:r['us'])['collider_id_before_native']==0)
        check('existing_collider_updates_have_owner',sum(bool(r['collider_id_before_native']) for r in first)>=3)
    return checks,dict(hierarchy_samples=len(hs),collider_samples=len(cs),matched_samples=len(evidence),local_entities=sorted({x['entity'] for x in evidence}),matches=evidence,limitation=__doc__)

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--physics-input',action='store_true');p.add_argument('--negative-tests',action='store_true');p.add_argument('--component-lookup',action='store_true');a=p.parse_args()
    rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()];checks,evidence=audit(rows,a.physics_input,a.component_lookup)
    if a.negative_tests:
        def owned(r):return r.get('event')=='impact_hierarchy_sample' and r.get('sequence',0)!=0
        for name,key in [('wrong_root','root_entity'),('wrong_item','latest_actor_item'),('wrong_sequence','sequence'),('stale_actor','latest_actor_ms')]:
            bad=copy.deepcopy(rows);r=next((r for r in bad if owned(r)),None)
            if r:r[key]+=1000
            checks['reject_'+name]=r is not None and not all(audit(bad,a.physics_input,a.component_lookup)[0].values())
        for name,field,offset in [('raw_parent','hierarchy_bytes',0),('raw_item','item_config_bytes',8),('event_guid','sequence_event_bytes',8)]:
            bad=copy.deepcopy(rows);r=next((r for r in bad if owned(r)),None)
            if r:b=bytearray.fromhex(r[field]);b[offset]^=1;r[field]=b.hex()
            checks['reject_'+name]=r is not None and not all(audit(bad,a.physics_input,a.component_lookup)[0].values())
        bad=[r for r in rows if r.get('event')!='sequence_asset_snapshot'];checks['reject_missing_asset']=not all(audit(bad,a.physics_input,a.component_lookup)[0].values())
        bad=[r for r in rows if r.get('event')!='impact_hierarchy_removed'];checks['reject_missing_restore']=not all(audit(bad,a.physics_input,a.component_lookup)[0].values())
        if a.physics_input:
            bad=copy.deepcopy(rows)
            for r in bad:
                if r.get('event')=='sequence_collider_sample':r['physics_current_bytes']='00'*32
            checks['reject_zero_physics_input']=not all(audit(bad,True,a.component_lookup)[0].values())
        if a.component_lookup:
            for name,field,value in [('missing_direct_owner','ready',False),('wrong_direct_root','hierarchy_bytes','00'*16)]:
                bad=copy.deepcopy(rows)
                for r in bad:
                    if r.get('event')=='sequence_collider_sample' and r.get('owner_lookup'):r['owner_lookup'][field]=value
                checks['reject_'+name]=not all(audit(bad,a.physics_input,True)[0].values())
            bad=[r for r in rows if r.get('event')!='sequence_collider_ready_sample'];checks['reject_missing_pre_creation_lookup']=not all(audit(bad,a.physics_input,True)[0].values())
    result=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},passed=sum(checks.values()),failed=sum(not v for v in checks.values()),evidence=evidence)
    (a.probe/('impact-component-checks.json' if a.component_lookup else 'impact-physics-checks.json' if a.physics_input else 'impact-ownership-checks.json')).write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print('PASS',result['passed'],'FAIL',result['failed']);return bool(result['failed'])
if __name__=='__main__':raise SystemExit(main())
