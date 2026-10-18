"""Check runtime-observed bindings against the independently published Khronos registry."""
import argparse,copy,hashlib,json,xml.etree.ElementTree as E
from collections import Counter
from pathlib import Path

def audit(rows,registry):
 profiles={p.get('name'):p for p in registry.findall('.//interaction_profile') if p.find('component') is not None}
 face=[r for r in rows if r.get('event')=='motion_binding' and r.get('action') in ('primary_face','secondary_face')]
 wanted={}
 for profile in ('oculus/touch_controller','valve/index_controller'):
  for hand in ('left','right'):
   labels=('x','y') if profile.startswith('oculus/') and hand=='left' else ('a','b')
   for action,label in zip(('primary_face','secondary_face'),labels):
    wanted[('/interaction_profiles/'+profile,action,'/user/hand/'+hand+'/input/'+label+'/click')]=1
 observed=Counter((r['profile'],r['action'],r['path']) for r in face)
 legal=True
 for r in face:
  p=profiles.get(r['profile']);legal_path=False
  if p is not None:
   for c in p.findall('component'):
    users=[c.get('user_path')] if c.get('user_path') else [u.get('path') for u in p.findall('user_path')]
    if r['path'] in [u+c.get('subpath','') for u in users]:
     legal_path|=c.get('type')=='XR_ACTION_TYPE_BOOLEAN_INPUT' and c.get('system')!='true'
  legal &= legal_path
 return dict(all_eight_hand_actions_once=observed==Counter(wanted),official_registry_boolean_non_system_paths=legal,
     no_substitute_system_or_menu_button=all('/system/' not in r['path'] and '/menu/' not in r['path'] for r in face),
     complete_instance=len([r for r in rows if r.get('event')=='instance_created'])==1 and any(r.get('event')=='instance_destroyed' for r in rows))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('log',type=Path);p.add_argument('--registry',type=Path,default=Path('references/OpenXR-SDK/specification/registry/xr.xml'));p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 raw=a.log.read_bytes();rows=[json.loads(l) for l in raw.splitlines() if l.strip()]
 starts=[i for i,r in enumerate(rows) if r.get('event')=='instance_created']
 if len(starts)<2:raise ValueError('two completed action/session cycles required')
 cycles=[rows[starts[-2]:starts[-1]],rows[starts[-1]:]];registry=E.parse(a.registry).getroot();checks={}
 for i,cycle in enumerate(cycles):checks.update({f'cycle{i+1}_{k}':v for k,v in audit(cycle,registry).items()})
 for label,change in [('wrong_touch_hand',{'path':'/user/hand/left/input/a/click'}),('system_button',{'path':'/user/hand/left/input/system/click'}),('menu_button',{'path':'/user/hand/left/input/menu/click'}),('unsupported_profile',{'profile':'/interaction_profiles/htc/vive_controller'}),('swapped_action',{'action':'secondary_face'})]:
  bad=copy.deepcopy(cycles[-1]);next(r for r in bad if r.get('event')=='motion_binding' and r.get('action')=='primary_face').update(change)
  checks['reject_'+label]=not all(audit(bad,registry).values())
 report=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,source_log=str(a.log),source_sha256=hashlib.sha256(raw).hexdigest(),registry=str(a.registry),registry_sha256=hashlib.sha256(a.registry.read_bytes()).hexdigest(),scope='Real client -> OpenXR loader -> process-local mock; controller bindings and two session cycles only. No game use/attack mapping or hardware claim.')
 a.output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 for k,v in checks.items():print('PASS' if v else 'FAIL',k)
 return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
