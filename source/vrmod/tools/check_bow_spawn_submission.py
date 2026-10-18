"""Verify the captured bow transform reaches the original native spawn submission unchanged."""
import argparse,json,struct
from pathlib import Path
from check_bow_spawn_observer import audit as base_audit

def audit(p):
 base=base_audit(p);events=[json.loads(s) for s in (p/'view-resource-probe.jsonl').read_text().splitlines()];ev=lambda n:[e for e in events if e['event']==n]
 shots={e['sample']:e for e in ev('bow_spawn_sample')};dispatch={e['sample']:e for e in ev('bow_spawn_dispatch_result')};submit={e['sample']:e for e in ev('bow_spawn_submit_result')}
 checks=dict(crosshair=base['status']=='PASS',complete_path=bool(shots) and set(shots)==set(dispatch)==set(submit) and len(dispatch)==len(ev('bow_spawn_dispatch_result')) and len(submit)==len(ev('bow_spawn_submit_result')),native_identity=True,transform_unchanged=True,accepted=True,retired=False)
 rows=[]
 for k,x in shots.items():
  near=[v for v in ev('input_bow_sample') if v['input']==x['input']];near=min(near,key=lambda v:abs(v['us']-x['us'])) if near else None
  cached=bytes.fromhex(x['observed_weapon_bytes']);current=bytes.fromhex(x['native_weapon_bytes'])
  checks['native_identity'] &= bool(near) and x['native_identity_observed'] and x['native_world']==near['world'] and x['native_entity']==near['entity']==x['local_entity'] and 0<=x['identity_age_ms']<=100 and cached[0]==current[0]==3 and cached[0x4c:0x50]==current[0x4c:0x50]==struct.pack('<I',327157552) and cached[0x60:0x64]==current[0x60:0x64]
  d=dispatch.get(k,{});s=submit.get(k,{})
  checks['transform_unchanged'] &= d.get('same_transform',False) and bytes.fromhex(d.get('params_bytes',''))[0x18:0x4c]==bytes.fromhex(x['transform_bytes'])[:52]
  checks['accepted'] &= d.get('result',False) and s.get('result',False) and s.get('source_unchanged',False) and d.get('read_only',False) and s.get('read_only',False) and d.get('params')==s.get('params') and x['us']<=d.get('us',0)<=s.get('us',0)
  rows.append(dict(sample=k,world=x['native_world'],entity=x['native_entity'],identity_age_ms=x['identity_age_ms'],submission_delay_us=s.get('us',x['us'])-x['us']))
 removed=ev('bow_spawn_removed');checks['retired']=len(removed)==1 and removed[0]['dispatch_samples']==len(dispatch) and removed[0]['submit_samples']==len(submit) and removed[0]['restored'] and not removed[0]['callbacks']
 return dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,shots=rows,limitation='Read-only same-thread native Crosshair->dispatch->spawn acceptance witness. Cached component identity is not a world-lifetime or shot-authority token. Final ECS projectile identity/collision and live tracked aim remain unverified.')
if __name__=='__main__':
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);v=a.parse_args();r=audit(v.probe);(v.probe/'bow-spawn-submission-check.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2));raise SystemExit(r['status']!='PASS')
