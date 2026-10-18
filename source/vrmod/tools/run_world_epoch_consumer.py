"""Independent DLL consumer of the managed game's persistent World value broker."""
import argparse,hashlib,json,pathlib,time
from inject_camera_observer import inject

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('publisher',type=pathlib.Path);p.add_argument('--preclose-admission',action='store_true');p.add_argument('--seconds',type=int,default=120);a=p.parse_args()
 if not 30<=a.seconds<=150:p.error('consumer duration must be 30..150 seconds')
 mod=pathlib.Path(__file__).resolve().parents[1];game=mod.parent/'enshrouded.exe';launch_dir=a.publisher.parent
 profile=json.loads((mod/'config/profiles/build23966345.json').read_text())
 if hashlib.sha256(game.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported executable')
 launch=json.loads((launch_dir/'launch.json').read_text(encoding='utf-8-sig'))
 if (launch_dir/'process-exit.json').exists():raise ValueError('managed game already exited')
 telemetry=launch_dir/'vulkan.jsonl'
 if not telemetry.exists() or time.time()-telemetry.stat().st_mtime>10:raise ValueError('game telemetry stale')
 if list(launch_dir.glob('world-consumer-*')):raise ValueError('one finite consumer per managed process')
 rows=[json.loads(s) for s in (a.publisher/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 created=[r for r in rows if r['event']=='world_broker_created'];enabled=[r for r in rows if r['event']=='world_broker_enabled']
 if len(created)!=3 or {r['kind'] for r in created}!={0,1,2} or len(enabled)!=1 or any(r['event'] in ('world_broker_closing','world_broker_faulted','world_lifecycle_observation_failure','world_lifecycle_removed') for r in rows):raise ValueError('require complete initial live broker generation')
 provider=enabled[0]['provider']
 if not provider or any(r['provider']!=provider or r['phase']!=1 or r['flags']!=1 or not r['serial'] for r in created):raise ValueError('invalid broker publication')
 policy=f'EVR_WORLD_EPOCH_CONSUMER_1 {a.seconds} {provider}\n'
 for r in created:policy+=' '.join(str(int(r[k],16)) for k in ('world','registry','owner_root'))+f" {r['kind']} {r['slot']} {r['serial']}\n"
 out=launch_dir/time.strftime('world-consumer-%Y%m%d-%H%M%S');dll=pathlib.Path(json.loads((mod/'build/bin/Release/evr_world_epoch_consumer.json').read_text())['library_path'])
 inputs={'world-epoch-consumer.txt':policy.encode()}
 if a.preclose_admission:
  if (a.publisher/'world-preclose-admission.txt').read_text().strip()!='EVR_WORLD_PRECLOSE_ADMISSION_1':raise ValueError('publisher early admission policy required')
  inputs['world-preclose-admission.txt']=b'EVR_WORLD_PRECLOSE_ADMISSION_1\n'
 result=inject(launch['pid'],game,out,dll,b'EVR_ObserveWorldEpochConsumer',launch['creation_filetime'],timeout_ms=(a.seconds+10)*1000,input_files=inputs)
 events=[json.loads(s) for s in (out/'world-epoch-consumer.jsonl').read_text(encoding='utf-8').splitlines()]
 completed=any(r['event']=='world_consumer_complete' and r['result']==0 for r in events)
 report=dict(status='PASS' if result==0 and completed else 'FAIL',remote_thread_result=result,export_completed=completed,publisher=str(a.publisher),exe_sha256=profile['exe_sha256'])
 (out/'world-consumer-run.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print('Result:',result,'Output:',out,flush=True);return result or int(not completed)
if __name__=='__main__':raise SystemExit(main())
