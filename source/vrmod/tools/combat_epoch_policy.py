"""Build a strict read-only combat-epoch policy from the initial native publisher."""
import hashlib,json,pathlib,struct,time
IDENTITY=('provider','serial','slot','world','registry','owner_root','kind')

def published(rows, live=True):
 created=[r for r in rows if r['event']=='world_broker_created']
 enabled=[r for r in rows if r['event']=='world_broker_enabled']
 starts=[r for r in rows if r['event']=='world_lifecycle_observation_begin']
 if len(created)!=3 or {r['kind'] for r in created}!={0,1,2} or len(enabled)!=1:raise ValueError('three distinct initial native World roles required')
 if len(starts)!=1 or not starts[0].get('xr_trial') or not starts[0].get('exit_trial') or not 300<=starts[0]['seconds']<=420:raise ValueError('explicit bounded XR startup publisher required')
 if live and any(r['event'] in ('world_broker_closing','world_broker_faulted','world_broker_stopped','world_lifecycle_observation_failure','world_lifecycle_removed') for r in rows):raise ValueError('publisher is no longer live')
 if any(r['provider']!=enabled[0]['provider'] or not r['provider'] or not r['serial'] or r['phase']!=1 or r['flags']!=1 or r['pins']!=0 for r in created):raise ValueError('invalid broker generation')
 if len({r['serial'] for r in created})!=3 or len({r['slot'] for r in created})!=3:raise ValueError('aliased initial epochs')
 for r in created:
  event='world_lifecycle_prediction' if r['kind']==1 else 'world_lifecycle_end'
  native=[n for n in rows if n['event']==event and n['call_id']==r['call_id']]
  if len(native)!=1:raise ValueError('missing exact native creation')
  n=native[0];raw=bytes.fromhex(n['header_hex'])
  if len(raw)!=16 or struct.unpack_from('<Q',raw)[0]!=int(r['registry'],16) or raw[8]!=r['kind'] or n['world']!=r['world'] or r['us']<n['us']:raise ValueError('native World header disagrees with broker')
  if r['kind'] in (0,2) and (n['operation']!='create' or n['native_result']!=r['owner_root']):raise ValueError('incorrect native root owner')
 roles={r['kind']:r for r in created};c,s=roles[2],roles[0]
 if any(c[k]==s[k] for k in ('world','registry','owner_root')):raise ValueError('combat client/server roles must be distinct')
 if roles[1]['registry']!=c['registry'] or roles[1]['owner_root']!=c['owner_root']:raise ValueError('prediction must share the client owner')
 return c,s

def prepare(publisher,launch_dir,strict=False):
 publisher=pathlib.Path(publisher).resolve();launch_dir=pathlib.Path(launch_dir).resolve()
 if publisher.parent!=launch_dir or (launch_dir/'process-exit.json').exists():raise ValueError('publisher must belong to this live managed process')
 launch=json.loads((launch_dir/'launch.json').read_text(encoding='utf-8-sig'))
 telemetry=launch_dir/'vulkan.jsonl'
 if time.time()-telemetry.stat().st_mtime>10:raise ValueError('managed telemetry stale')
 raw=(publisher/'view-resource-probe.jsonl').read_bytes()
 rows=[json.loads(s) for s in raw.decode('utf-8').splitlines() if s.strip().endswith('}')]
 c,s=published(rows)
 magic="EVR_COMBAT_EPOCH_STRICT_1" if strict else "EVR_COMBAT_EPOCH_SHADOW_1"
 policy=f"{magic} {c['provider']}\n"
 for r in (c,s):policy+=' '.join(str(int(r[k],16)) for k in ('world','registry','owner_root'))+f" {r['kind']} {r['slot']} {r['serial']}\n"
 proof=dict(publisher=str(publisher),pid=launch['pid'],creation_filetime=launch['creation_filetime'],source_prefix_sha256=hashlib.sha256(raw).hexdigest(),source_prefix_bytes=len(raw),identities=[{k:r[k] for k in IDENTITY} for r in (c,s)],shadow_only=not strict)
 return {('world-combat-epoch-strict.txt' if strict else 'world-combat-epoch-shadow.txt'):policy.encode(),'combat-epoch-provenance.json':(json.dumps(proof,indent=2)+'\n').encode()}
