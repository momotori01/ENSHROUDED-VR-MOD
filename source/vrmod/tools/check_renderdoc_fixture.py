"""Cross-check RenderDoc replay pixels against independent GPU readback for fixture frame 5."""
import argparse,json,pathlib,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
from PIL import Image,ImageStat

def check(directory):
    checks=[]
    def test(name,passed):checks.append(dict(name=name,passed=bool(passed)))
    replay=directory/'replay';result=json.loads((replay/'result.json').read_text())
    test('exactly_two_fixture_draws',result['draws']==2)
    test('two_replay_outputs',result['textures']==2 and result['saved']==2 and result['save_failures']==0)
    actions=[json.loads(s) for s in (replay/'actions.jsonl').read_text().splitlines()]
    draws=[e for e in actions if e['flags']&2]
    test('distinct_draw_targets',[e['outputs'][0] for e in draws]==[0,1])
    pixels=[]
    for index,eye in enumerate(['left','right']):
        image=Image.open(replay/f'texture_{index}.png').convert('RGB');direct=Image.open(directory/f'c0_f5_{eye}.ppm').convert('RGB')
        test(eye+'_exact_replay_readback_match',image.size==direct.size and image.tobytes()==direct.tobytes())
        stat=ImageStat.Stat(image);test(eye+'_nonblack_nonuniform',sum(stat.mean)>9 and sum(stat.var)>12)
        pixels.append(image.tobytes())
    test('stereo_images_differ',pixels[0]!=pixels[1])
    telemetry=[json.loads(s) for s in (directory/'openxr.jsonl').read_text().splitlines()]
    eyes=[e for e in telemetry if e['event']=='eye_render' and e['frame']==5]
    test('same_predicted_frame',len(eyes)==2 and {e['eye'] for e in eyes}=={0,1} and eyes[0]['time']==eyes[1]['time'])
    test('persistent_registry_unchanged',json.loads((directory/'isolation.json').read_text())['registry_unchanged'])
    (directory/'renderdoc-checks.json').write_text(json.dumps(checks,indent=2))
    for c in checks:print('PASS' if c['passed'] else 'FAIL',c['name'])
    return all(c['passed'] for c in checks)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('directory',type=pathlib.Path);args=parser.parse_args()
    raise SystemExit(0 if check(args.directory) else 1)
