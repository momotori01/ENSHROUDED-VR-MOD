"""One unused native model create/move/retire test; no VR or model drawing."""
import argparse,hashlib,json,pathlib,time
from inject_camera_observer import inject
from run_render_resource_probe import postcheck,retained_scene_probe

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--launch',type=pathlib.Path,required=True);p.add_argument('--source-probe',type=pathlib.Path,required=True);p.add_argument("--hold-frames",type=int,default=0,help="TEST: detach owned SDF, keep hidden model for1..600world render boundaries");a=p.parse_args()
 if not 0<=a.hold_frames<=600:p.error("hold frames must be0..600")
 mod=pathlib.Path(__file__).resolve().parents[1];game=mod.parent/'enshrouded.exe'
 if a.source_probe.resolve().parent!=a.launch.resolve():raise ValueError('source probe must belong to this launch')
 if retained_scene_probe(a.launch):raise ValueError('requires launch without retained drawing probe')
 if list(a.launch.glob('model-instance-*')):raise ValueError('one model lifecycle attempt per managed process; use fresh launch')
 profile=json.loads((mod/'config/profiles/build23966345.json').read_text())
 if hashlib.sha256(game.read_bytes()).hexdigest()!=profile['exe_sha256']:raise ValueError('unsupported executable hash')
 launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
 if time.time()-(a.launch/'vulkan.jsonl').stat().st_mtime>10:raise ValueError('stale managed game telemetry')
 rows=[json.loads(x) for x in (a.source_probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
 ended=[x for x in rows if x['event']=='attachment_observer_removed']
 if len(ended)!=1 or not ended[0]['restored'] or ended[0]['failures'] or ended[0]['callbacks']:raise ValueError('source observer did not cleanly retire')
 models=[x for x in rows if x['event']=='attachment_model' and x['item_slot']==1 and x['read_only'] and 'resource_guid_words' in x]
 if not models:raise ValueError('no observed local melee resource')
 candidate=models[-1]
 if not any(x['event']=='attachment_slot' and x['entity']==candidate['entity'] and x['parent']==candidate['parent'] and x['slot']==9 for x in rows):raise ValueError('source has no local right-hand ownership evidence')
 out=a.launch/time.strftime('model-instance-%Y%m%d-%H%M%S')
 words=candidate['resource_guid_words']
 if len(words)!=4 or not any(words) or not all(type(x)==int and 0<=x<2**32 for x in words):raise ValueError('invalid GUID words')
 magic="EVR_UNUSED_MODEL_HOLD_1" if a.hold_frames else "EVR_UNUSED_MODEL_1"
 policy=f"{magic} {candidate['scene']} {candidate['handle']} "+' '.join(map(str,words))+(f" {a.hold_frames}" if a.hold_frames else "")+'\n'
 dll=pathlib.Path(json.loads((mod/'build/bin/Release/evr_view_resource_probe.json').read_text())['library_path'])
 crashes={x.name for x in (game.parent/'crash_dumps').iterdir()}
 result=inject(launch['pid'],game,out,dll,b'EVR_ProbeModelInstance',launch['creation_filetime'],timeout_ms=40000 if a.hold_frames else 25000,input_files={'model-instance.txt':policy.encode()})
 (out/'source-candidate.json').write_text(json.dumps(dict(source_probe=str(a.source_probe.resolve()),candidate=candidate),indent=2)+'\n',encoding='utf-8')
 events=[json.loads(x) for x in (out/'view-resource-probe.jsonl').read_text().splitlines()]
 survived=postcheck(launch,out,events[-1]['us'],crashes,game)
 print('Result:',result,'Output:',out);return result or int(not survived)
if __name__=='__main__':raise SystemExit(main())
