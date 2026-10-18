"""Request one bounded raw dump from an opted-in Mock navigation launch."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);p.add_argument('indices',type=int,nargs='+');a=p.parse_args()
launch=json.loads((a.launch/'launch.json').read_text(encoding='utf-8-sig'))
if launch.get('runtime')!='mock' or not launch.get('mock_navigation') or not launch.get('eye_image_sweep'):p.error('requires opted-in Mock navigation and sweep')
if (a.launch/'ui-close-request.json').exists():p.error('session closing or closed')
if not 1<=len(a.indices)<=16 or len(set(a.indices))!=len(a.indices) or min(a.indices)<0:p.error('require 1..16 unique indices')
log=a.launch/'native-xr/openxr.jsonl';inventory=None;used=False
for line in log.open():
 try:e=json.loads(line)
 except ValueError:continue
 if e.get('event')=='native_eye_sweep_inventory':inventory=e
 if e.get('event')=='manual_eye_dump':used=True
if used or (a.launch/'eye-dump.request').exists():p.error('only one dump request per session')
if not inventory:p.error('wait for sweep inventory')
images={e['i']:e for e in inventory['images']}
if any(i not in images or not images[i]['readable'] for i in a.indices):p.error('selection not readable in inventory')
selected=[images[i] for i in a.indices]
(a.launch/'eye-dump-selection.json').write_text(json.dumps(dict(inventory_frame=inventory['frame'],selected=selected,scope='Read post-pair image contents; original request and per-image manifest are preserved by the runtime.'),indent=2)+'\n',encoding='utf-8')
request=a.launch/'eye-dump.request'
pending=a.launch/'eye-dump.request.pending'
with pending.open('x',encoding='ascii') as f:f.write('EVR_EYE_IMAGE_DUMP_1\n'+' '.join(map(str,a.indices))+'\n')
pending.rename(request)
print(json.dumps(dict(requested=a.indices)))
