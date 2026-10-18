"""Finite read-only World creation/teardown evidence; native UI drives transitions."""
import argparse, hashlib, json, pathlib, time
from inject_camera_observer import inject
from run_render_resource_probe import postcheck

def active_layer_devices(rows):
 devices={}
 for r in rows:
  if r.get('event')=='create_device' and r.get('device'):devices[r['device']]=r
  elif r.get('event')=='destroy_device':devices.pop(r.get('device'),None)
  elif r.get('event')=='destroy_instance':devices={k:v for k,v in devices.items() if v.get('instance')!=r.get('instance')}
 return devices

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--launch',type=pathlib.Path,required=True)
 p.add_argument('--seconds',type=int,default=120)
 p.add_argument('--epoch-shadow',action='store_true')
 p.add_argument('--epoch-broker',action='store_true')
 p.add_argument('--reentry',action='store_true',help='Read-only two-generation World observation, normal UI exit/reenter/final close; no VR claim')
 p.add_argument('--xr-trial',action='store_true',help='Explicit finite 300..420s startup publisher for paired XR combat role shadow')
 p.add_argument('--preclose-admission',action='store_true',help='Stop MOD new-work admission before native entity deletion')
 p.add_argument('--input-backend-exit',action='store_true',help='Observe exact native input backend destruction; no synthetic controller')
 p.add_argument('--exit-progress',action='store_true',help='Read-only render/Scene/queue progress across native teardown')
 p.add_argument('--preclose',action='store_true',help='Read-only pre-root entity deletion observation')
 p.add_argument('--normal-exit',action='store_true',help='Finite initial-root generation/teardown trial; requires normal UI close before deadline')
 p.add_argument('--startup',action='store_true',help='Allow at most 10 seconds for first telemetry in a newly managed process')
 a=p.parse_args()
 if not ((300<=a.seconds<=420) if (a.xr_trial or a.reentry) else (60<=a.seconds<=180)):p.error('ordinary duration 60..180 seconds; explicit XR trial 300..420 seconds')
 if a.reentry and (a.xr_trial or a.input_backend_exit or not (a.startup and a.epoch_shadow and a.epoch_broker and a.normal_exit)):p.error('reentry requires isolated startup/shadow/broker/normal exit and no input-owner finalizer')
 if a.xr_trial and not (a.startup and a.epoch_shadow and a.epoch_broker and a.normal_exit):p.error('XR trial requires startup/shadow/broker/normal exit')
 if (a.epoch_shadow and not a.startup) or (a.normal_exit and not a.epoch_shadow):p.error('epoch shadow requires initial startup; exit mode also requires epoch shadow')
 if a.epoch_broker and not (a.startup and a.epoch_shadow and a.normal_exit):p.error('broker trial requires startup/shadow/normal exit')
 if a.input_backend_exit and (not a.epoch_broker or not a.normal_exit or a.xr_trial):p.error('backend exit requires isolated broker/normal-exit')
 if a.exit_progress and (not a.preclose or a.xr_trial):p.error('exit progress requires non-XR preclose; render hook cannot be shared')
 if a.preclose_admission and not a.preclose:p.error('early admission needs explicit preclose observation')
 if a.preclose and not (a.startup and a.epoch_broker and a.normal_exit):p.error('preclose requires initial broker/normal exit')
 mod=pathlib.Path(__file__).resolve().parents[1];game=mod.parent/'enshrouded.exe'
 profile=json.loads((mod/'config/profiles/build23966345.json').read_text())
 if hashlib.sha256(game.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported executable hash')
 launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
 telemetry=a.launch/'vulkan.jsonl'
 readiness=None
 if a.startup:
  began=time.monotonic();deadline=began+10
  while time.monotonic()<deadline:
   if telemetry.exists():
    if not a.epoch_broker:break
    raw=telemetry.read_bytes();complete=raw[:raw.rfind(b'\n')+1]
    devices=active_layer_devices([json.loads(l) for l in complete.splitlines() if l.strip()])
    if devices:
     readiness=dict(devices=[dict(device=r['device'],instance=r.get('instance'),us=r['us']) for r in devices.values()],wait_seconds=time.monotonic()-began,source_prefix_bytes=len(complete),source_prefix_sha256=hashlib.sha256(complete).hexdigest(),scope='Vulkan device creation observed, not World creation/adoption proof')
     break
   time.sleep(.05)
  if a.epoch_broker and readiness is None:raise ValueError('no live layer device within startup deadline; refusing transient probe-instance attach')
 if not telemetry.exists() or time.time()-telemetry.stat().st_mtime>10:raise ValueError('managed game telemetry is stale')
 if list(a.launch.glob('world-lifetimes-*')):raise ValueError('one lifetime observer per managed process')
 dll=pathlib.Path(json.loads((mod/'build/bin/Release/evr_view_resource_probe.json').read_text())['library_path'])
 out=a.launch/time.strftime('world-lifetimes-%Y%m%d-%H%M%S')
 crashes={s.name for s in (game.parent/'crash_dumps').iterdir()}
 magic='EVR_WORLD_REENTRY_1' if a.reentry else 'EVR_WORLD_LIFETIMES_XR_1' if a.xr_trial else 'EVR_WORLD_LIFETIMES_1'
 policy={'world-lifetimes-duration.txt':f'{magic} {a.seconds}\n'.encode()}
 if readiness:policy['startup-layer-ready.json']=(json.dumps(readiness,indent=2)+'\n').encode()
 if a.epoch_shadow:policy['world-epoch-shadow.txt']=b'EVR_WORLD_EPOCH_SHADOW_1\n'
 if a.epoch_broker:policy['world-epoch-broker.txt']=b'EVR_WORLD_EPOCH_BROKER_1\n'
 if a.preclose_admission:policy['world-preclose-admission.txt']=b'EVR_WORLD_PRECLOSE_ADMISSION_1\n'
 if a.preclose:policy['world-preclose-observe.txt']=b'EVR_WORLD_PRECLOSE_OBSERVE_1\n'
 if a.input_backend_exit:policy['input-backend-exit.txt']=b'EVR_INPUT_BACKEND_EXIT_1\n'
 if a.exit_progress:policy['world-exit-progress.txt']=b'EVR_WORLD_EXIT_PROGRESS_1\n'
 if a.normal_exit:policy['world-lifetimes-exit.txt']=b'EVR_WORLD_EXIT_OBSERVE_1\n'
 result=inject(launch['pid'],game,out,dll,b'EVR_ObserveWorldLifetimes',launch['creation_filetime'],
  timeout_ms=(a.seconds+15)*1000,input_files=policy)
 events=[json.loads(s) for s in (out/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 if a.normal_exit:
  deadline=time.monotonic()+15;exit_file=a.launch/'process-exit.json'
  while not exit_file.exists() and time.monotonic()<deadline:time.sleep(.1)
  exit_data=json.loads(exit_file.read_text(encoding='utf-8-sig')) if exit_file.exists() else {}
  new_crashes={p.name for p in (game.parent/'crash_dumps').iterdir()}-crashes
  survived=exit_data.get('status')=='exited' and exit_data.get('exit_code')==0 and not new_crashes
  (out/'probe-postcheck.json').write_text(json.dumps(dict(status='PASS' if survived else 'FAIL',expected_normal_exit=True,process_exit=exit_data,new_crash_directories=sorted(new_crashes),scope='normal exit trial; no post-detach rendering claim'),indent=2)+'\n',encoding='utf-8')
 else:survived=postcheck(launch,out,events[-1]['us'],crashes,game)
 completed=any(e.get('event')=='world_lifecycle_observation_complete' and e.get('result')==0 for e in events)
 report=dict(remote_thread_result=result,observer_completed=completed,postcheck=survived,exe_sha256=profile['exe_sha256'],status='PASS' if result==0 and completed and survived else 'FAIL')
 (out/'world-lifetimes-run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print('Result:',result,'Output:',out,flush=True);return result or int(not survived or not completed)
if __name__=='__main__':raise SystemExit(main())
