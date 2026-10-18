"""Join Vulkan pipeline traces to saved shader/reflection layout candidates."""
import argparse,collections,json,pathlib
def main():
    p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path);a=p.parse_args()
    inv=json.loads(pathlib.Path('docs/analysis/shader_inventory.json').read_text())
    shapes={s['file']:{c for b in s['blocks'] for c in b['offset_candidates']} for s in inv}
    modules={};pipelines={};records=[];ranges=collections.Counter();events=collections.Counter()
    for line in (a.directory/'vulkan.jsonl').read_text().splitlines():
        try:e=json.loads(line)
        except json.JSONDecodeError:continue
        event=e['event'];events[event]+=1
        if event=='shader' and 'module' in e:modules[e['module']]=e['hash']
        elif event in ('graphics_pipeline','compute_pipeline'):
            ms=[x['module'] for x in e['stages']] if event=='graphics_pipeline' else [e['module']]
            hashes=[modules[m] for m in ms if m in modules]
            candidates=set().union(*(shapes.get('shader_'+h+'.spv',set()) for h in hashes))
            pipelines[e['pipeline']]={'hashes':hashes,'candidates':sorted(candidates)}
        elif event=='trace_pipeline':
            info=pipelines.get(e['pipeline'],{})
            records.append({'pipeline':e['pipeline'],'bind_point':e['bind_point'],'stack':e['game_rva_stack'],**info})
        elif event=='trace_buffer_binding':ranges[(e['type'],e['range'])]+=1
    unique={}
    for r in records:
        key=(r['pipeline'],tuple(r['stack']));unique.setdefault(key,{**r,'uses':0})['uses']+=1
    report={'events':dict(events),'pipeline_observations':list(unique.values()),'buffer_ranges':[{'descriptor_type':t,'bytes':b,'updates':n} for (t,b),n in ranges.most_common()]}
    (a.directory/'render_analysis.json').write_text(json.dumps(report,indent=2))
    print('Trace events',sum(n for e,n in events.items() if e.startswith('trace_')),'unique pipelines/stacks',len(unique))
    for r in unique.values():
        names=[c for c in r.get('candidates',[]) if any(n in c for n in ('RenderGBuffer','DeferredLighting','RenderTransparent','TemporalAnti','GpuCulling'))]
        if names:print(r['uses'],','.join(names),r['stack'][:7])
    print('Uniform buffer ranges',[(b,n) for (t,b),n in ranges.most_common(15) if t in (6,8)])
if __name__=='__main__':main()
