"""Bounded offline HUD-isolation checks; not live VR/UI or exact image equivalence.

RGBA8 source-over reconstruction allows three byte levels at 99.9% of pixels.
This tolerates quantization accumulated across native UI draws, but explicitly
reports every larger difference and baseline replay variability. It does not
hide outliers by excluding unstable pixels or certify menus not in this capture.
"""
import argparse,collections,hashlib,json,pathlib,sys

def check(folder,image_overrides=None):
    entries=list(map(json.loads,(folder/'ui-pass-images.jsonl').read_text().splitlines()))
    cases={e['case']:e for e in entries};expected={'background','before','control','isolated','restored'}
    if set(cases)!=expected or len(entries)!=5:raise ValueError('five unique diagnostic cases required')
    dimensions={(e['width'],e['height'],e['format']) for e in entries}
    if len(dimensions)!=1:raise ValueError('inconsistent image extents')
    w,h,fmt=dimensions.pop()
    if not(0<w<=4096 and 0<h<=4096 and fmt=='R8G8B8A8_UNORM'):raise ValueError('unsupported image format')
    data={}
    for k,e in cases.items():
        name=pathlib.Path(e['file'])
        if name.name!=str(name):raise ValueError('snapshot filename must be local')
        data[k]=(folder/name).read_bytes()
        if image_overrides and k in image_overrides:data[k]=image_overrides[k]
        if len(data[k])!=w*h*4:raise ValueError('raw snapshot length mismatch')
    background,before,control,ui,restored=(data[k] for k in ('background','before','control','isolated','restored'))
    counts=collections.Counter();maximum=total=0.;worst=[]
    for offset in range(0,len(ui),4):
        a=ui[offset+3];counts['transparent' if a==0 else 'opaque' if a==255 else 'partial']+=1
        if a==0 and any(ui[offset+j] for j in range(3)):counts['zero_alpha_nonzero_rgb']+=1
        if a>0:counts['ui_pixels']+=1
        diff=max(abs(ui[offset+j]+background[offset+j]*(255-a)/255-before[offset+j]) for j in range(3))
        maximum=max(maximum,diff);total+=diff
        if diff>3:
            counts['reconstruction_gt3']+=1
            if a>0:counts['ui_reconstruction_gt3']+=1
        if diff>8:counts['reconstruction_gt8']+=1
        if diff>3 and len(worst)<16:worst.append(dict(x=offset//4%w,y=offset//4//w,error=diff,alpha=a))
        if before[offset:offset+4]!=control[offset:offset+4]:counts['control_changed_pixels']+=1
        if before[offset:offset+4]!=restored[offset:offset+4]:counts['restored_changed_pixels']+=1
    n=w*h;checks=[]
    def add(name,ok,detail):checks.append(dict(name=name,status='PASS' if ok else 'FAIL',detail=detail))
    add('native_rgba8_extents',True,dict(width=w,height=h))
    add('background_before_ui',cases['background']['event']<cases['before']['event'],'captured event order')
    add('same_ui_end_event',len({cases[k]['event'] for k in expected-{'background'}})==1,'baseline/control/isolation/restoration')
    add('substantial_transparency',counts['transparent']>n//2,counts['transparent'])
    add('opaque_and_translucent_ui',counts['opaque']>1000 and counts['partial']>1000,dict(opaque=counts['opaque'],partial=counts['partial']))
    add('recompose_all_pixels_99_9_percent',counts['reconstruction_gt3']/n<=.001,dict(outliers=counts['reconstruction_gt3'],pixels=n,tolerance_byte_levels=3))
    add('recompose_ui_pixels_99_9_percent',counts['ui_pixels']>0 and counts['ui_reconstruction_gt3']/counts['ui_pixels']<=.001,dict(outliers=counts['ui_reconstruction_gt3'],pixels=counts['ui_pixels']))
    return dict(scope=__doc__,folder=str(folder),checks=checks,counts=dict(counts),
        metrics=dict(maximum_reconstruction_error=maximum,mean_max_channel_error=total/n,
                     exact_baseline_repeat=before==control,exact_restoration=before==restored),
        first_outliers=worst,sha256={k:hashlib.sha256(v).hexdigest() for k,v in data.items()},
        passed=sum(e['status']=='PASS' for e in checks),failed=sum(e['status']=='FAIL' for e in checks))

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('folder',type=pathlib.Path);a=p.parse_args()
    report=check(a.folder);(a.folder/'ui-pass-checks.json').write_text(json.dumps(report,indent=2)+'\n')
    print(f"{report['passed']} PASS / {report['failed']} FAIL; exact baseline repeat: {report['metrics']['exact_baseline_repeat']}; exact restoration: {report['metrics']['exact_restoration']}")
    print(json.dumps(report['counts']));return int(report['failed']!=0)
if __name__=='__main__':sys.exit(main())
