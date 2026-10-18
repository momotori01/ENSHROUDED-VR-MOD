"""Replay verified native World identities/order in the value-only epoch book.
Callback pins are deliberately synthetic; this is not native epoch integration.
"""
import argparse,hashlib,json,pathlib,subprocess
from check_world_lifetimes import audit

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('proof',type=pathlib.Path);p.add_argument('--mode',choices=['create','exit'],required=True);a=p.parse_args()
 raw=a.proof/'view-resource-probe.jsonl';rows=[json.loads(x) for x in raw.read_text(encoding='utf-8').splitlines()]
 checks,metrics=audit(rows,a.mode)
 if not all(checks.values()):raise ValueError('native source evidence failed validation')
 lines=[];root_by_call={r['call_id']:r for r in rows if r['event']=='world_lifecycle_begin' and r['operation']=='root_destroy'}
 if a.mode=='exit':
  # Offline inventory uses the exact first observed metadata of each World.
  # This does not claim those worlds were all observable at a live attach point.
  inventory={}
  for r in rows:
   if r['event']=='world_lifecycle_begin' and r['operation']=='entity_cleanup':
    root=root_by_call[r['parent_call_id']]['root'];inventory[r['world']]=(r['registry'],root,r['kind'])
  for world,(registry,root,kind) in inventory.items():lines.append(f'adopt {world} {registry} {root} {kind}')
  lines.append('seal')
  for r in rows:
   if r['event']=='world_lifecycle_begin' and r['operation']=='root_destroy':lines.append(f"root_begin {r['call_id']} {r['root']}")
   elif r['event']=='world_lifecycle_end' and r['operation']=='root_destroy':lines.append(f"root_end {r['call_id']}")
   elif r['event']=='world_lifecycle_begin' and r['operation'] in ('entity_cleanup','storage_destroy'):lines.append(f"world_closing {r['world']} {r['registry']} {r['kind']}")
  lines.append('expect_size 0')
 else:
  created={r['call_id']:r for r in rows if r['event']=='world_lifecycle_end' and r['operation']=='create'}
  for r in rows:
   if r['event']=='world_lifecycle_end' and r['operation']=='create':lines.append(f"create {r['world']} {r['registry']} {r['root']} {r['kind']}")
   elif r['event']=='world_lifecycle_prediction':lines.append(f"create {r['world']} {r['registry']} {created[r['call_id']]['root']} {r['kind']}")
  lines+=['seal','expect_size 3']
 csv=a.proof/f'world-epoch-{a.mode}-replay.txt';csv.write_text('\n'.join(lines)+'\n',encoding='ascii')
 binary=pathlib.Path(__file__).resolve().parents[1]/'build/bin/Release/evr_world_lifetime_replay.exe'
 result=subprocess.run([str(binary),str(csv)],capture_output=True,text=True)
 report=dict(status='PASS' if result.returncode==0 else 'FAIL',result=result.returncode,stdout=result.stdout,stderr=result.stderr,source_checks=checks,raw_sha256=hashlib.sha256(raw.read_bytes()).hexdigest(),replay_sha256=hashlib.sha256(csv.read_bytes()).hexdigest(),binary_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),scope=__doc__)
 (a.proof/f'world-epoch-{a.mode}-replay-checks.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8');print(result.stdout,result.stderr);return result.returncode
if __name__=='__main__':raise SystemExit(main())
