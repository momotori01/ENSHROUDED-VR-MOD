"""Image sanity tests; comparisons never constitute Enshrouded integration proof."""
import argparse,hashlib,json,pathlib,statistics,sys
def ppm(path):
    with path.open('rb') as f:
        if f.readline()!=b'P6\n':raise ValueError('unsupported image')
        w,h=map(int,f.readline().split());assert f.readline()==b'255\n'
        pixels=f.read()
    if len(pixels)!=w*h*3:raise ValueError('truncated image')
    return w,h,pixels
def analyze(directory,expect_motion=False):
    checks=[]
    def check(name,ok,details=None):checks.append({'test':name,'status':'PASS' if ok else 'FAIL','details':details})
    lefts=sorted(directory.glob('*_left.ppm'))
    check('capture_pairs_exist',bool(lefts))
    hashes=[]
    for left in lefts:
        right=left.with_name(left.name.replace('_left.ppm','_right.ppm'))
        check(left.stem+':right_exists',right.exists())
        if not right.exists():continue
        lw,lh,lp=ppm(left);rw,rh,rp=ppm(right)
        for label,p in [('left',lp),('right',rp)]:
            sampled=p[::51];avg=statistics.mean(p[::17]);var=statistics.pvariance(sampled)
            check(left.stem+':'+label+':nonblack',avg>3,{'mean':avg})
            check(left.stem+':'+label+':nonuniform',var>4,{'variance':var})
        check(left.stem+':matching_dimensions',(lw,lh)==(rw,rh))
        # Images can legitimately match for a featureless scene. This test scene has depth.
        check(left.stem+':stereo_different',hashlib.sha256(lp).digest()!=hashlib.sha256(rp).digest())
        hashes.append(hashlib.sha256(lp).hexdigest())
    if expect_motion:check('pose_changes_image',len(set(hashes))>1,{'distinct_images':len(set(hashes))})
    logfile=directory/'openxr.jsonl'
    if logfile.exists():
        events=[json.loads(s) for s in logfile.read_text().splitlines() if s]
        pairs={};cycle=0
        for e in events:
            if e['event']=='runtime':cycle+=1
            if e['event']=='eye_render':pairs.setdefault((cycle,e['frame']),[]).append(e)
        check('eye_telemetry_present',bool(pairs))
        check('each_frame_has_two_eyes',all(len(p)==2 and {e['eye'] for e in p}=={0,1} for p in pairs.values()))
        check('eyes_share_predicted_time',all(len({e['time'] for e in p})==1 for p in pairs.values()))
        completed=[e for e in events if e['event']=='cycle_complete']
        check('balanced_completed_draw_counts',bool(completed) and all(e['left']==e['right'] and e['left']>0 for e in completed))
        check('no_fallback',not any(e['event']=='fallback' for e in events))
    (directory/'capture_results.json').write_text(json.dumps(checks,indent=2))
    for c in checks:print(c['status'],c['test'])
    failed=sum(c['status']=='FAIL' for c in checks)
    print('RESULT',len(checks)-failed,'PASS',failed,'FAIL')
    return failed==0
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=pathlib.Path);p.add_argument('--expect-motion',action='store_true');a=p.parse_args();sys.exit(0 if analyze(a.directory,a.expect_motion) else 1)
