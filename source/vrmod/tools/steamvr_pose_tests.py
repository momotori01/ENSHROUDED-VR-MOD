"""Real SteamVR OpenXR pose tests using only the isolated software HMD.
The rendered images are the test scene, never claimed as Enshrouded VR output.
"""
import json,math,os,pathlib,subprocess,sys,time
from virtual_pose import publish

def quaternion(yaw,pitch,roll):
    y,p,r=[v*math.pi/360 for v in (yaw,pitch,roll)]
    cy,sy,cp,sp,cr,sr=math.cos(y),math.sin(y),math.cos(p),math.sin(p),math.cos(r),math.sin(r)
    return [cy*sp*cr+sy*cp*sr,sy*cp*cr-cy*sp*sr,cy*cp*sr-sy*sp*cr,cy*cp*cr+sy*sp*sr]

def main():
    mod=pathlib.Path(__file__).resolve().parents[1]
    test=mod/'test_env/steamvr'
    session=json.loads((test/'active-session.json').read_text(encoding='utf-8-sig'))
    if not session['virtual']:raise RuntimeError('Pose tests require the isolated custom driver')
    paths=json.loads((test/'openvrpaths.vrpath').read_text(encoding='utf-8-sig'))
    env=os.environ.copy()
    env.update(VR_PATHREG_OVERRIDE=str(test/'openvrpaths.vrpath'),VR_CONFIG_PATH=str(test/'config'),
               VR_LOG_PATH=str(test/'logs'),XR_RUNTIME_JSON=str(pathlib.Path(paths['runtime'][0])/'steamxr_win64.json'))
    out=mod/'output'/time.strftime('steamvr-pose-tests-%Y%m%d-%H%M%S');out.mkdir(parents=True)
    cases=[('neutral',[0,1.65,0],[0,0,0],.064),('yaw_left',[0,1.65,0],[90,0,0],.064),
           ('yaw_right',[0,1.65,0],[-90,0,0],.064),('yaw_back',[0,1.65,0],[180,0,0],.064),
           ('up',[0,1.65,0],[35,80,0],.064),('down',[0,1.65,0],[35,-90,0],.064),
           ('roll',[0,1.65,0],[30,20,45],.064),('translation',[.17,1.81,-.23],[-23,41,-19],.064),
           ('ipd',[0,1.65,0],[0,0,0],.072)]
    results=[]
    def check(ok,name):
        results.append({'name':name,'pass':bool(ok)})
        print(('PASS ' if ok else 'FAIL ')+name,flush=True)
    try:
        for name,pos,rot,ipd in cases:
            publish(test/'pose.txt','pose '+' '.join(map(str,pos+rot+[ipd,1,1])))
            time.sleep(.1)
            case=out/name;case.mkdir()
            run=subprocess.run([str(mod/'build/bin/Release/evr_xr_probe.exe'),'--frames','8','--capture-every','8','--output',str(case)],env=env,capture_output=True,text=True,timeout=45)
            (case/'probe.txt').write_text(run.stdout+run.stderr)
            check(run.returncode==0,name+' OpenXR frame lifecycle')
            if run.returncode:continue
            log=[json.loads(l) for l in (case/'openxr.jsonl').read_text().splitlines()]
            h=[x for x in log if x['event']=='hmd'][-1]
            check(math.dist(h['position'],pos)<.002,name+' HMD translation in LOCAL space')
            check(abs(sum(a*b for a,b in zip(h['orientation'],quaternion(*rot))))>.9999,name+' HMD quaternion')
            eyes=[x for x in log if x['event']=='eye_render'][-2:]
            check(len(eyes)==2 and eyes[0]['eye']==0 and eyes[1]['eye']==1 and eyes[0]['time']==eyes[1]['time'],name+' two eyes at one predicted time')
            check(abs(math.dist(eyes[0]['position'],eyes[1]['position'])-ipd)<.002,name+' runtime IPD')
            for eye in eyes:
                midpoint=[(a+b)/2 for a,b in zip(eyes[0]['position'],eyes[1]['position'])]
                check(math.dist(midpoint,pos)<.002 and eye['fov'][0]<0<eye['fov'][1],name+f" eye {eye['eye']} center/FOV")
            cycle=[x for x in log if x['event']=='cycle_complete'][-1]
            check(cycle['left']==cycle['right']==8,name+' all frames rendered for both eyes')
    finally:
        publish(test/'pose.txt','pose 0 1.65 0 0 0 0 0.064 1 1')
        (out/'report.json').write_text(json.dumps(results,indent=2))
    failures=sum(not r['pass'] for r in results)
    print(f'RESULT {len(results)-failures} PASS {failures} FAIL; {out}',flush=True)
    return bool(failures)

if __name__=='__main__':sys.exit(main())
