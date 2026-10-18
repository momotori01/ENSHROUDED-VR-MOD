"""Validate private native water readback ownership and matching-frame aggregation.

This is diagnostic evidence, not certification of water appearance or gameplay.
"""
import collections
import hashlib
import struct


def check_water(records, live, skipped=frozenset()):
    # skipped (2026-10-06): desktop-skipped serials. Eye 0 is the native water producer (copies into the native pool);
    # eye 1's private packet is drained through the partial path and not merged (known limitation: right-eye water).
    def events(name):return [e for e in records if e['event']==name]
    created=events('water_feedback_pool_created');released=events('water_feedback_pool_released')
    hooks=events('water_feedback_hook_removed');routes=events('water_feedback_routed');merged=events('water_feedback_merged')
    pools={e['eye']:e['pool'] for e in created}
    checks={
        'water_private_pool_ownership':len(created)==2 and set(pools)=={0,1} and len(set(pools.values()))==2 and all(e['pool']!=e['source_pool'] and 1<=e['capacity']<=8 and e['packet_bytes']==0x8000c for e in created),
        'water_private_pools_released':len(released)==2 and collections.Counter(e['pool'] for e in released)==collections.Counter(e['pool'] for e in created) and all(any(c['pool']==r['pool'] and c['us']<r['us'] for c in created) for r in released),
        'water_no_bridge_failure':not events('water_feedback_failure'),
        'water_consumer_hook_restored':len(hooks)==1 and hooks[0]['status']==0,
    }
    if not live:
        checks['water_unused_no_producer_or_merge']=not routes and not merged
        return checks
    copies=[(e,c) for e in events('download_feedback_pool') for c in e['calls'] if c['caller']=='0x1cda3f']
    checks['water_all_native_copy_results_successful']=bool(copies) and all(c['status']==0 for e,c in copies)
    checks['water_original_copy_pool_unchanged']=bool(copies) and all(c['pool']==c.get('requested_pool') for e,c in copies if e['eye']==-1)
    checks['water_private_copies_routed']=bool(routes) and all((c['pool']==c.get('requested_pool')) if (e['eye']==0 and e['serial'] in skipped) else
        (e['eye'] in pools and c['pool']==pools[e['eye']] and c['pool']!=c.get('requested_pool')) for e,c in copies if e['eye']>=0)
    produced=collections.Counter((e['serial'],e['eye']) for e in routes)
    serials=sorted({e['serial'] for e in routes})
    # Unready warmup Views can record water copies on only one eye. No XR
    # projection layer was submitted for these frames; native water stays intact.
    warmup={e['serial'] for e in events('world_warmup_frame_ended') if e['layers']==0}
    partial=events('water_feedback_partial_warmup')
    partial_serials={e['serial'] for e in partial}
    asym={s for s in serials if any(produced[(s,eye)]!=1 for eye in (0,1))}
    skip_ok={s for s in asym if s in skipped and produced[(s,0)]==0 and produced[(s,1)]==1}
    checks['water_balanced_producers']=bool(serials) and all(n==1 for n in produced.values()) and asym.issubset(warmup|skip_ok)
    if partial:
        checks['water_partial_warmup_native_preserved']=len(partial_serials)==len(partial) and partial_serials==asym and all(
            e['produced_eye_mask']==sum(1<<eye for eye in (0,1) if produced[(e['serial'],eye)]) and
            e['original_packet_unchanged'] and e['produced_packets_drained'] and e['serial']<e['current_serial'] for e in partial)
    consumed=[e['serial'] for e in merged]
    checks['water_same_frame_merged_once']=bool(consumed) and len(set(consumed))==len(consumed) and all(e['same_serial'] and e['supplied_to_native'] and e['serial']<e['current_serial'] for e in merged)
    # The final completed packet may be drained at Scene disposal after the
    # bounded rendering hooks have been restored; earlier packets must merge.
    checks['water_all_prior_produced_frames_consumed']=len(serials)>1 and set(serials[:-1]).issubset(set(consumed)|partial_serials) and set(consumed).issubset(set(serials)-asym)
    checks['water_merge_capacity_preserved']=bool(merged) and all(len(e['merged_counts'])==3 and all(0<=n<=0x8000 for n in e['merged_counts']) for e in merged)
    return checks


def check_packet_evidence(probe, records):
    evidence=[e for e in records if e['event']=='water_feedback_merged' and e.get('packet_evidence')]
    checks={'water_packet_evidence_present':bool(evidence)};hashes={}
    def decode(path):
        data=path.read_bytes();hashes[path.name]=hashlib.sha256(data).hexdigest()
        if len(data)!=0x8000c:raise ValueError('invalid packet byte size')
        counts=struct.unpack_from('<III',data)
        splits=[struct.unpack_from('<II',data,12+8*n) for n in range(min(counts[0],0x8000))]
        coarse={struct.unpack_from('<I',data,0x4000c+4*n)[0] for n in range(min(counts[1],0x8000))}
        retire={struct.unpack_from('<I',data,0x6000c+4*n)[0] for n in range(min(counts[2],0x8000))}
        return counts,splits,coarse,retire
    for e in evidence:
        serial=e['serial'];ok=False
        try:
            prefix=probe/'water-packets'/f'serial-{serial}'
            inputs=[decode(prefix.with_name(prefix.name+f'-view-{n}.bin')) for n in range(3)]
            out=decode(prefix.with_name(prefix.name+'-merged.bin'))
            priorities={}
            for packet in inputs:
                for priority,node in packet[1]:priorities[node]=max(priority,priorities.get(node,0))
            expected=sorted(((priority,node) for node,priority in priorities.items()),key=lambda s:(-s[0],s[1]))[:0x8000]
            coarse=set.intersection(*(packet[2] for packet in inputs))-priorities.keys()
            retire={v for v in set.intersection(*(packet[3] for packet in inputs)) if (v&0xfffffff) not in priorities}
            ok=(out[1]==expected and out[2]==coarse and out[3]==retire and
                list(out[0])==e['merged_counts'] and [list(p[0]) for p in inputs]==e['source_counts'] and
                tuple(map(len,(out[1],out[2],out[3])))==out[0])
        except (OSError,ValueError,struct.error):pass
        checks[f'water_packet_{serial}_merge_matches_independent_reference']=ok
    return checks,hashes
