"""Recenter only the EVR software HMD, then restore its synthetic identity origin."""
import json,math,os,pathlib,subprocess,sys,time
from virtual_pose import publish

def main():
    mod=pathlib.Path(__file__).resolve().parents[1];test=mod/'test_env/steamvr';bin=mod/'build/bin/Release'
    state=json.loads((test/'active-session.json').read_text(encoding='utf-8-sig'))
    if not state['virtual']:raise RuntimeError('Virtual HMD session required')
    paths=json.loads((test/'openvrpaths.vrpath').read_text(encoding='utf-8-sig'));env=os.environ.copy()
    env.update(VR_PATHREG_OVERRIDE=str(test/'openvrpaths.vrpath'),VR_CONFIG_PATH=str(test/'config'),VR_LOG_PATH=str(test/'logs'),XR_RUNTIME_JSON=str(pathlib.Path(paths['runtime'][0])/'steamxr_win64.json'))
    out=mod/'output'/time.strftime('steamvr-recenter-%Y%m%d-%H%M%S');out.mkdir(parents=True)
    def run(exe,args):
        r=subprocess.run([str(bin/exe),*args],env=env,capture_output=True,text=True,timeout=25)
        if r.returncode:raise RuntimeError(r.stdout+r.stderr)
        return r.stdout
    try:
        run('evr_vr_control.exe',['calibrate-test'])
        publish(test/'pose.txt','pose 0.2 1.8 -0.1 45 0 0 0.064 1 1');time.sleep(.2)
        run('evr_vr_control.exe',['recenter-test']);time.sleep(.2)
        print(run('evr_xr_probe.exe',['--frames','8','--capture-every','0','--output',str(out)]),end='')
        rows=[json.loads(s) for s in (out/'openxr.jsonl').read_text().splitlines()]
        h=[r for r in rows if r['event']=='hmd'][-1]
        ok=math.dist(h['position'],[0,0,0])<.003 and abs(h['orientation'][3])>.9999
        (out/'recenter.json').write_text(json.dumps({'pass':ok,'head':h},indent=2))
        print(('PASS ' if ok else 'FAIL ')+'SteamVR LOCAL pose after recenter')
        return not ok
    finally:
        publish(test/'pose.txt','pose 0 1.65 0 0 0 0 0.064 1 1')
        print(run('evr_vr_control.exe',['calibrate-test']),end='')

if __name__=='__main__':sys.exit(main())
