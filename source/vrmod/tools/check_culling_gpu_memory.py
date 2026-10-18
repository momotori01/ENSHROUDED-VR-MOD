"""Check native-to-Vulkan buffer identity, memory binding and nonoverlap.

Uses actual Vulkan handles from verified native object metadata. Checks usable
buffer byte ranges, not all allocation padding or arbitrary GPU synchronization.
"""
import argparse
import json
from pathlib import Path


def check_gpu_memory(native, vulkan, rendered):
    made=[e for e in native if e['event']=='culling_history_created']
    checks={'culling_gpu_handle_metadata':len(made)==2 and all(e.get('vk_buffer') and e.get('source_vk_buffer') for e in made)}
    if not checks['culling_gpu_handle_metadata']:
        return checks
    ranges={}
    for e in made:
        for label,handle in ((f"eye{e['eye']}",e['vk_buffer']),('source',e['source_vk_buffer'])):
            creates=[v for v in vulkan if v['event']=='buffer_created' and v['buffer']==handle and v['us']<=e['us']]
            create=creates[-1] if creates else None
            binds=[v for v in vulkan if v['event']=='buffer_memory_bound' and v['buffer']==handle and create and create['us']<v['us']<=e['us']]
            bind=binds[-1] if binds else None
            valid=bool(create and bind and int(bind['memory'],16) and create['size']==e['bytes'])
            if label!='source':
                valid=valid and create['usage']==35 and bind['offset']==e['memory_offset']
            checks[f'culling_gpu_{label}_bound_before_use']=bool(valid)
            if valid:ranges[label]=(handle,bind['memory'],bind['offset'],create['size'])
        if rendered:
            releases=[n['us'] for n in native if n['event']=='culling_history_released' and n['eye']==e['eye']]
            end=min(releases) if releases else float('inf')
            fills=[v for v in vulkan if v['event']=='buffer_fill_audit' and v['buffer']==e['vk_buffer'] and e['us']<v['us']<end]
            checks[f"culling_gpu_eye{e['eye']}_fills_valid"]=bool(fills) and all(v.get('memory_bound') and v['valid_usage_and_range'] for v in fills)
    checks['culling_gpu_three_distinct_buffers']=len(ranges)==3 and len({r[0] for r in ranges.values()})==3
    overlap=False
    values=list(ranges.values())
    for i,(_,memory,offset,size) in enumerate(values):
        for _,other,start,length in values[i+1:]:
            overlap|=memory==other and max(offset,start)<min(offset+size,start+length)
    checks['culling_gpu_original_left_right_nonoverlap']=len(ranges)==3 and not overlap
    return checks


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('probe',type=Path)
    p.add_argument('--rendered',action='store_true')
    a=p.parse_args()
    def rows(path):return [json.loads(s) for s in path.read_text(encoding='utf-8-sig').splitlines()]
    checks=check_gpu_memory(rows(a.probe/'view-resource-probe.jsonl'),rows(a.probe.parent/'vulkan.jsonl'),a.rendered)
    report={k:'PASS' if v else 'FAIL' for k,v in checks.items()}
    (a.probe/'culling-gpu-memory-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    for k,v in report.items():print(v,k)
    print('PASS',sum(checks.values()),'FAIL',len(checks)-sum(checks.values()))
    return int(not all(checks.values()))


if __name__=='__main__':raise SystemExit(main())
