"""Bounded native calibration evidence, NOT playable OpenXR integration validation.

RenderDoc 1.46 ResourceUsage values are pinned to the downloaded official headers.
Backward traversal is a conservative resource-use graph (RW/blending can retain earlier contents),
not pixel provenance. Distinct geometry events, matrices and output pixels are checked separately.
"""
import argparse,collections,copy,json,math,pathlib,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
from PIL import Image,ImageChops,ImageStat
WRITES=set(range(22,31))|{32,33,35,37,38,40,41,43,45}
READS=set(range(13,31))|{31,32,33,39,42}

def evidence(textures,actions,usage):
    named=[[t for t in textures if t.get('name')==f'EVR calibration eye {eye}'] for eye in (0,1)]
    if any(len(t)!=1 for t in named):return {'valid':False,'reason':'missing or ambiguous named eye'}
    indices=[t[0]['index'] for t in named]
    writes=collections.defaultdict(set);reads=collections.defaultdict(set)
    for u in usage:
        if u['usage'] in WRITES:writes[u['texture']].add(u['event'])
        if u['usage'] in READS:reads[u['event']].add(u['texture'])
    by_event={a['event']:a for a in actions};end=max(by_event)+1
    branches=[];output_writers=[]
    for index in indices:
        seen=set();geometry=set();stack=[(index,end)]
        output_writers.append(max(writes[index],default=0))
        while stack:
            texture,before=stack.pop();event=max((e for e in writes[texture] if e<before),default=0)
            if not event or (texture,event) in seen:continue
            seen.add((texture,event));action=by_event.get(event,{})
            if action.get('flags',0)&2 and len([i for i in action.get('outputs',[]) if i>=0])>=3:geometry.add(event)
            stack.extend((source,event) for source in reads[event])
        branches.append(geometry)
    return dict(valid=indices[0]!=indices[1] and all(branches) and branches[0].isdisjoint(branches[1]) and len(branches[0])==len(branches[1]) and len(set(output_writers))==2,
                indices=indices,output_writers=output_writers,geometry_events=[sorted(s) for s in branches])

def main():
    p=argparse.ArgumentParser();p.add_argument('probe',type=pathlib.Path);p.add_argument('--replay',default='replay-usage');args=p.parse_args()
    replay=args.probe/args.replay
    def rows(path):return [json.loads(s) for s in path.read_text().splitlines()]
    textures=rows(replay/'textures.jsonl');actions=rows(replay/'actions.jsonl');usage=rows(replay/'usage.jsonl')
    graph=evidence(textures,actions,usage);checks=[]
    def test(name,passed,detail=None):checks.append(dict(name=name,passed=bool(passed),detail=detail))
    test('distinct_balanced_geometry_branches',graph['valid'],graph)
    if not graph['valid']:
        (args.probe/'native-calibration-checks.json').write_text(json.dumps(checks,indent=2));return 1
    eyes=[Image.open(replay/f'texture_{index}.png').convert('RGB') for index in graph['indices']]
    test('same_extents',eyes[0].size==eyes[1].size)
    for index,image in enumerate(eyes):
        stat=ImageStat.Stat(image);test(f'eye{index}_nonblack_nonuniform',sum(stat.mean)>9 and sum(stat.var)>12)
    difference=ImageStat.Stat(ImageChops.difference(*eyes)).mean
    test('different_eye_pixels',sum(difference)>0,difference)
    log=rows(args.probe/'view-resource-probe.jsonl');transforms=[e for e in log if e['event']=='calibration_eye_transform']
    test('two_eye_transforms',len(transforms)==2 and {e['eye'] for e in transforms}=={0,1})
    if len(transforms)==2:
        test('same_native_context_serial',transforms[0]['serial']==transforms[1]['serial'])
        separation=math.sqrt(sum(((x-y)/4294967296.0)**2 for x,y in zip(transforms[0]['camera_q32'],transforms[1]['camera_q32'])))
        test('synthetic_64mm_camera_separation',abs(separation-.064)<1e-6,separation)
        delta=[x-y for x,y in zip(transforms[0]['view_matrix'][12:15],transforms[1]['view_matrix'][12:15])]
        test('matching_64mm_view_translation',abs(math.sqrt(sum(x*x for x in delta))-.064)<1e-5,delta)
        test('same_calibration_projection',transforms[0]['projection_matrix']==transforms[1]['projection_matrix'])
    test('game_survived_postcheck',json.loads((args.probe/'probe-postcheck.json').read_text())['status']=='PASS')
    test('hook_restored',any(e['event']=='view_resource_hook_complete' and e.get('original_bytes_restored') for e in log))
    test('right_writer_missing_rejected',not evidence(textures,actions,[u for u in usage if u['texture']!=graph['indices'][1]])['valid'])
    broken=copy.deepcopy(textures)
    for t in broken:
        if t.get('name')=='EVR calibration eye 1':t['index']=graph['indices'][0]
    test('aliased_eyes_rejected',not evidence(broken,actions,usage)['valid'])
    (args.probe/'native-calibration-checks.json').write_text(json.dumps(checks,indent=2))
    for c in checks:print('PASS' if c['passed'] else 'FAIL',c['name'])
    print('Calibration only: synthetic pose, VFX excluded, resources retained, OpenXR not connected.')
    return 0 if all(c['passed'] for c in checks) else 1

if __name__=='__main__':raise SystemExit(main())
