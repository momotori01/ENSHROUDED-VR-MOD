"""Read-only live weapon category registry and independent Guid/resource checks."""
import argparse,copy,json,struct
from pathlib import Path

def fnv(g):
 if not any(g):return 0
 h=0x811c9dc5
 for b in g:h=((h^b)*0x1000193)&0xffffffff
 return h

def verify(rows):
 ev=lambda n:[r for r in rows if r['event']==n]
 cat=ev('input_weapon_category_entry');summary=ev('input_weapon_category_catalog');items=ev('input_item_weapon_category');catalog=ev('input_item_catalog_entry');byGuid={x['guid']:x for x in cat};types={x['type'] for x in cat};s=summary[0] if len(summary)==1 else {}
 checks=dict(one_stable_registry=len(summary)==1 and s.get('stable_header') and s.get('read_only') and s.get('entries')==s.get('count')==len(cat) and 0<len(cat)<=64,
  unique_resources=len(byGuid)==len(cat)==len({x['index'] for x in cat})==len({x['resource'] for x in cat}) and bool(cat),
  reflected_weapon_type_coverage={0,2,5,7,10,13}<=types and types<=set(range(16)),
  read_only_all_resources=bool(cat) and all(x['read_only'] for x in cat),
  item_catalog_same_identity=len(items)==len(catalog)==24 and [x['item'] for x in items]==[x['metadata']['item_id'] for x in catalog],
  no_native_failure=not any('failure' in x['event'] or 'failed' in x['event'] for x in rows),
  observation_completed=any(x['result']==0 for x in ev('input_observation_complete')),
  no_input_fixture=not ev('controller_fixture_connected'))
 rawOK=bool(cat);itemOK=bool(items);found=0
 for x in cat:
  b=bytes.fromhex(x['bytes']);g=bytes.fromhex(x['guid'])
  rawOK &= len(b)==88 and len(g)==16 and b[:16]==g and fnv(g)==x['key'] and b[84]==x['type'] and x['two_hand_melee']==(7<=x['type']<=10) and x['one_hand_melee']==(x['type']<=3 or x['type']==13)
  rawOK &= x['index']<s.get('capacity',0)
 for x in items:
  b=bytes.fromhex(x['raw_item']);g=b[208:224]
  itemOK &= len(b)==264 and struct.unpack_from('<I',b)[0]==x['item'] and g.hex()==x['guid'] and fnv(g)==x['hash'] and x['read_only']
  if not any(g):itemOK &= x['status']=='no_item' and x['type']==255
  else:
   c=byGuid.get(g.hex());itemOK &= bool(c) and x['status']=='found'
   if c:itemOK &= x['type']==c['type'] and x['index']==c['index'] and x['resource']==c['resource'];found+=1
 checks['independent_guid_hash_and_resource_bytes']=bool(rawOK);checks['independent_item_reference_resolution']=bool(itemOK) and found>0
 for n in ['input_ui_observer_removed','input_direction_observer_removed','input_sender_observer_removed','input_pipeline_removed','input_actor_final_removed','input_ranged_removed','input_picking_removed']:
  e=ev(n);checks[n]=len(e)==1 and e[0]['restored'] and e[0].get('failures',0)==0 and e[0].get('callbacks',e[0].get('remaining_callbacks',0))==0
 return checks,dict(resources=len(cat),types=sorted(types),sampled_items=len(items),resolved_items=found)

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');p.add_argument('--attachments',action='store_true');a=p.parse_args();root=a.probe.parent
 r=[json.loads(l) for l in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];c,m=verify(r)
 c['normal_exit']=json.loads((root/'process-exit.json').read_text(encoding='utf-8-sig'))['exit_code']==0;c['Core_Sync_clean']=(root/'validation.log').stat().st_size==0
 c['post_survived']=json.loads((a.probe/'probe-postcheck.json').read_text())['status']=='PASS'
 if a.attachments:
  attached=[x for x in r if x['event']=='attachment_weapon_category'];visual=[x for x in r if x['event']=='attachment_visual_item'];cats={x['guid']:x for x in r if x['event']=='input_weapon_category_entry'}
  valid=bool(attached)
  for x in attached:
   raw=bytes.fromhex(x['raw_item']);guid=raw[208:224];target=cats.get(guid.hex())
   valid &= len(raw)==264 and struct.unpack_from('<I',raw)[0]==x['item'] and raw[204]==2 and raw[260]==x['item_slot'] and guid.hex()==x['guid'] and fnv(guid)==x['hash'] and x['read_only']
   valid &= x['status']=='found' and bool(target)
   if target:valid &= x['type']==target['type'] and x['resource']==target['resource']
   valid &= any(v['parent']==x['parent'] and v['entity']==x['entity'] and v['item_id']==x['item'] and v['visual_slot']==x['visual_slot'] and v['item_slot']==x['item_slot'] for v in visual)
  c['equipped_visual_to_full_category_reference']=bool(valid)
  c['attachment_observer_clean']=not any(x['event']=='attachment_observer_failure' for x in r) and any(x['event']=='attachment_observer_removed' and x.get('restored') and not x.get('callbacks',1) and not x.get('failures',1) for x in r)
  m['equipped']=[{k:x[k] for k in ('parent','entity','item','visual_slot','item_slot','type')} for x in attached]
 if a.negative_tests:
  for event,key,v in [('input_weapon_category_entry','key',0),('input_weapon_category_entry','type',255),('input_weapon_category_entry','two_hand_melee',True),('input_item_weapon_category','hash',0),('input_item_weapon_category','resource','0x0'),('input_weapon_category_catalog','stable_header',False)]:
   bad=copy.deepcopy(r)
   for x in bad:
    if x['event']==event:x[key]=v
   c['reject_'+event+'_'+key]=not all(verify(bad)[0].values())
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);print('PASS',sum(c.values()),'FAIL',sum(not v for v in c.values()))
 (a.probe/'weapon-category-checks.json').write_text(json.dumps(dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},metrics=m,scope=__doc__,limitations='Classification does not calibrate weapon geometry or prove all attacks. No equipment/state writes.'),indent=2)+'\n')
 return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
