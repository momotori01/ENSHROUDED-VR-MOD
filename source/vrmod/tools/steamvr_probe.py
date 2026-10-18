"""Bounded OpenXR probe against the isolated, already-started SteamVR session.
No persistent environment, registry, real-user SteamVR settings, or device changes.
Run as the same Windows user who launched the isolated session.
"""
import argparse,hashlib,json,os,pathlib,subprocess,sys,time
from check_vulkan_binding import select_creation, names
import host_device_replay

def main():
    p=argparse.ArgumentParser()
    p.add_argument('--frames',type=int,default=180)
    p.add_argument('--output',type=pathlib.Path)
    p.add_argument('--calibrate',action='store_true')
    p.add_argument('--borrowed',action='store_true',help='Run the external Vulkan device lifecycle fixture')
    p.add_argument('--retain-xr-instance',action='store_true',help='Borrowed fixture: retain owner XR instance across sessions')
    p.add_argument('--requirements-only',action='store_true',help='Query Vulkan requirements without a GPU device or XR session')
    p.add_argument('--game-trace',type=pathlib.Path,help='Borrowed fixture: preserve recorded game API and extension lists; does not reproduce game features/queues')
    p.add_argument('--instance-index',type=int,help='Explicit instance selection for legacy logs without handles')
    p.add_argument('--recorded-device',action='store_true',help='Additionally reproduce recorded feature values and queue parameters in the fixture')
    a=p.parse_args()
    if a.retain_xr_instance and (not a.borrowed or a.requirements_only):p.error('--retain-xr-instance requires --borrowed without --requirements-only')
    if a.game_trace and (not a.borrowed or a.requirements_only):p.error('--game-trace requires --borrowed, without --requirements-only')
    if a.recorded_device and not a.game_trace:p.error('--recorded-device requires --game-trace')
    mod=pathlib.Path(__file__).resolve().parents[1]
    test=mod/'test_env/steamvr'
    if not (test/'active-session.json').is_file():p.error('Start switch_vr.ps1 -Mode Virtual first')
    registration=json.loads((test/'openvrpaths.vrpath').read_text(encoding='utf-8-sig'))
    env=os.environ.copy()
    env.update(VR_PATHREG_OVERRIDE=str(test/'openvrpaths.vrpath'),VR_CONFIG_PATH=str(test/'config'),
               VR_LOG_PATH=str(test/'logs'),XR_RUNTIME_JSON=str(pathlib.Path(registration['runtime'][0])/'steamxr_win64.json'))
    out=a.output or mod/'output'/time.strftime('steamvr-%Y%m%d-%H%M%S')
    out.mkdir(parents=True,exist_ok=True)
    binary=mod/'build/bin/Release'
    host_args=[]
    if a.game_trace:
        events=[json.loads(line) for line in a.game_trace.read_text(encoding='utf-8-sig').splitlines() if line.strip()]
        if sum(e.get('event')=='create_device' for e in events)!=1:raise ValueError('game trace must contain exactly one device creation')
        instance,device,index,association=select_creation(events,instance_index=a.instance_index)
        api=instance['api'] or (1<<22)
        if type(api) is not int or not 0<api<(1<<29):raise ValueError('invalid game Vulkan API')
        host_args=['--host-api',str(api)]
        for kind,entry in [('instance',instance),('device',device)]:
            for extension in names(entry['extensions']):host_args.extend(['--host-'+kind+'-extension',extension])
        if a.recorded_device:host_args=host_device_replay.arguments(events)
        (out/'creation-input.json').write_text(json.dumps(dict(trace=str(a.game_trace.resolve()),
            sha256=hashlib.sha256(a.game_trace.read_bytes()).hexdigest(),instance_index=index,association=association,
            scope=('fixture reproduces recorded API, extensions, feature values and queue parameters; excludes game resources and creation flags/queue pNext' if a.recorded_device else 'fixture preserves API and extension names only; game feature chain/queues are not reproduced'),arguments=host_args),indent=2),encoding='utf8')
    def run(name,args,timeout):
        result=subprocess.run([str(binary/name),*args],env=env,capture_output=True,text=True,timeout=timeout)
        (out/(name+'.txt')).write_text(result.stdout+'\n'+result.stderr,encoding='utf8')
        print(result.stdout,result.stderr,end='',flush=True)
        if result.returncode:raise RuntimeError(f'{name} returned {result.returncode}')
    run('evr_vr_control.exe',['calibrate-test'] if a.calibrate else [],20)
    if a.requirements_only:
        run('evr_borrowed_vulkan_tests.exe',[str(out),'--requirements-only'],30)
    elif a.borrowed:
        if a.retain_xr_instance:host_args.append('--retain-xr-instance')
        run('evr_borrowed_vulkan_tests.exe',[str(out),*host_args],90)
        if a.recorded_device:host_device_replay.verify(events,out/'host-vulkan.jsonl')
    else:
        run('evr_xr_probe.exe',['--frames',str(a.frames),'--capture-every','30','--output',str(out)],max(60,a.frames/90*4))
    print('Results:',out)

if __name__=='__main__':
    try:main()
    except (subprocess.TimeoutExpired,RuntimeError) as e:print('FAIL',e,file=sys.stderr);sys.exit(1)
