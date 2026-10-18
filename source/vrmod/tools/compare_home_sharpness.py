"""Home fixed-camera sharpness comparison. Needs Pillow/NumPy; does not grade image quality."""
import argparse,hashlib,json
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw

def read(path):return json.loads(path.read_text(encoding='utf-8-sig'))
def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output',type=Path);p.add_argument('launches',type=Path,nargs='+');a=p.parse_args()
    assert 2<=len(a.launches)<=4
    a.output.mkdir(parents=True,exist_ok=True)
    regions={'cliff':(950,510,1390,900),'ridge_tree':(980,350,1300,600),'wood':(35,700,360,1120),'grass':(1200,1030,1630,1380)}
    results=[];images={};labels=[];fixture_reference=None
    for launch in a.launches:
        config=read(launch/'launch.json');settings=read(launch/'vr-render-settings.json')
        assert config['runtime']=='mock' and config['vr_upscaler']=='Dlss'
        assert settings['applied']['dlss']=='Quality'
        assert read(launch/'active-renderer-check.json')['status']=='PASS'
        assert read(launch/'play-session-check.json')['status']=='PASS'
        assert read(launch/'captured-camera-check.json')['status']=='PASS'
        assert read(launch/'scenery-replay-check.json')['status']=='PASS'
        assert read(launch/'save-reference-check.json')['status']=='PASS'
        assert read(launch/'ui-close-request.json')['exit_code']==0
        label='sharp-'+str(config['vr_sharpness']);assert label not in labels;labels.append(label)
        events=[json.loads(x) for x in (launch/'native-xr/openxr.jsonl').read_text().splitlines()]
        frames=[e['frame'] for e in events if e.get('event')=='native_burst_capture_complete'];assert len(frames)==32
        probe=sorted(launch.glob('view-resource-probe-*/world-session.txt'))[-1].parent
        fixture={name:hashlib.sha256(path.read_bytes()).hexdigest() for name,path in {'pose':launch/'mock-pose.csv','camera':probe/'world-scenery-replay.txt'}.items()}
        fixture['save_reference']=read(launch/'save-reference-check.json')['reference']
        if fixture_reference is None:fixture_reference=fixture
        assert fixture==fixture_reference, 'different fixed-camera or save fixture'
        result={'launch':str(launch),'label':label,'frames':frames,'regions':{}}
        for eye in (0,1):
            for name,box in regions.items():
                crops=[Image.open(launch/f'native-xr/frame-{frame}-eye-{eye}.ppm').crop(box).convert('RGB') for frame in frames]
                pixels=np.stack([np.asarray(c,dtype=np.float32) for c in crops]);delta=np.abs(np.diff(pixels,axis=0))
                result['regions'][f'{name}-eye{eye}']={'mean_adjacent_rgb_difference_255':float(delta.mean()),'p95_adjacent_rgb_difference_255':float(np.percentile(delta,95)),'fraction_difference_over_10':float((delta.max(axis=3)>10).mean()),'mean_rgb_255':float(pixels.mean())}
                images[label,eye,name]=crops[16];crops[16].save(a.output/f'{label}-eye{eye}-{name}.png')
        results.append(result)
    for eye in (0,1):
        for name,box in regions.items():
            w,h=box[2]-box[0],box[3]-box[1];canvas=Image.new('RGB',(w*len(labels),h+32),(24,24,24));draw=ImageDraw.Draw(canvas)
            for i,label in enumerate(labels):
                draw.text((i*w+8,8),label,fill='white');canvas.paste(images[label,eye,name],(i*w,32))
            canvas.save(a.output/f'comparison-eye{eye}-{name}.png')
    report={'fixture':fixture_reference,'regions':regions,'runs':results,'limitation':'Unmodified 1:1 crops. Fixed-camera and save audits required. World time, lighting, wind and exposure may differ; temporal metrics are not image-quality scores. Capture waits for GPU completion; no real-HMD quality or performance verdict.'}
    (a.output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(json.dumps({'output':str(a.output),'runs':len(results),'pairs_per_run':32}))
if __name__=='__main__':main()
