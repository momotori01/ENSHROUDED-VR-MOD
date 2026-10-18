"""Process-isolated failures, tracking loss, scale, and session-loss probes."""
import json,os,pathlib,subprocess,time
root=pathlib.Path(__file__).resolve().parents[1];binary=root/'build/bin/Release/evr_xr_probe.exe'
out=root/'output'/('negative-'+time.strftime('%Y%m%d-%H%M%S'));out.mkdir(parents=True)
base=os.environ.copy();base['XR_RUNTIME_JSON']=str(binary.with_name('evr_mock_runtime.json'));base['EVR_MOCK_REALTIME']='0';base.pop('EVR_POSE_SEQUENCE',None)
results=[]
def run(name,args,expected=0,sequence=None,missing_runtime=False):
    dest=out/name;env=base.copy();env['EVR_MOCK_LOG']=str(out/(name+'-runtime.jsonl'))
    if sequence:env['EVR_POSE_SEQUENCE']=str(root/'config/poses'/sequence)
    if missing_runtime:env['XR_RUNTIME_JSON']=str(out/'not_a_runtime.json')
    p=subprocess.run([str(binary),'--output',str(dest),'--capture-every','0',*args],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,timeout=45)
    (out/(name+'.txt')).write_text(p.stdout)
    ok=p.returncode==expected
    log=dest/'openxr.jsonl';events=[json.loads(x) for x in log.read_text().splitlines()] if log.exists() else []
    if name=='tracking_lost':ok=ok and any(x['event']=='empty_frame' for x in events) and any(x['event']=='eye_render' and x['frame']>135 for x in events)
    if name=='disconnect':ok=ok and any(x['event']=='runtime_loss_handled' for x in events)
    if name=='resolution':ok=ok and any(x['event']=='swapchain' and x['width']==800 and x['height']==900 for x in events)
    if name=='scale':ok=ok and any(x['event']=='swapchain' and x['width']==800 and x['height']==900 for x in events)
    results.append({'test':name,'status':'PASS' if ok else 'FAIL','returncode':p.returncode})
    print(results[-1]['status'],name)
run('missing_runtime',['--frames','1'],1,missing_runtime=True)
run('bad_scale',['--frames','1','--scale','nan'],1)
run('oversize',['--frames','1','--scale','10'],1)
run('scale',['--frames','4','--scale','1.25'])
run('resolution',['--frames','4'],sequence='17_resolution.csv')
run('tracking_lost',['--frames','180'],sequence='14_tracking_lost.csv')
run('disconnect',['--frames','180'],sequence='15_disconnect.csv')
run('session_recreation',['--frames','4','--cycles','3'])
(out/'results.json').write_text(json.dumps(results,indent=2));print('Results:',out)
raise SystemExit(0 if all(x['status']=='PASS' for x in results) else 1)
