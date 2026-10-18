"""Audit a resident item catalog and compare it with same-session equipped evidence."""
import argparse,collections,hashlib,json,struct
from pathlib import Path

def audit(probe):
 path=probe/'item-catalog.jsonl';data=path.read_bytes();rows=[json.loads(x) for x in data.splitlines()];header=rows.pop(0)
 events=[json.loads(x) for x in (probe/'view-resource-probe.jsonl').read_text().splitlines()];done=[e for e in events if e['event']=='item_catalog_complete'];ev=lambda n:[e for e in events if e['event']==n]
 ids={r['id']:r for r in rows};selected=[r for r in rows if r['selected']]
 c=dict(explicit=bool(ev('item_catalog_enabled')),complete=len(done)==1 and done[0]['items']==len(rows)==header['count'] and done[0]['selected']==len(selected),table_bounds=header['format']=='EVR_ITEM_CATALOG_1' and 0<header['count']<=32768 and header['count']<=header['capacity']<=131072 and header['capacity']&(header['capacity']-1)==0,source_preserved=header['read_only'] and header['source_unchanged'] and all(e['read_only'] and e['source_unchanged'] for e in done),unique_id_bucket=len(ids)==len(rows)==len({r['bucket'] for r in rows}) and all(r['id'] and 0<=r['bucket']<header['capacity'] for r in rows),selected_exact=all(r['selected']==(r['category']==2 or r['slot'] in (18,2,3)) for r in rows),descriptor_abi=True,relative_payload_sizes=True,equipped_prefix_matches=True,completed=any(e['result']==0 for e in ev('view_resource_hook_complete')),no_catalog_failure=not ev('item_catalog_failure'))
 output=[];types=collections.Counter();unknown=[]
 for r in selected:
  b=bytes.fromhex(r['item_bytes']);models=bytes.fromhex(r['models_bytes']);name=bytes.fromhex(r['name_bytes']);wc=bytes.fromhex(r['weapon_bytes'])
  c['descriptor_abi'] &= len(b)==1944 and struct.unpack_from('<I',b)[0]==r['id'] and b[0xcc]==r['category'] and b[0x104]==r['slot']
  c['relative_payload_sizes'] &= len(models)==struct.unpack_from('<I',b,0x4a0)[0]*20 and len(name)==struct.unpack_from('<I',b,0x794)[0] and len(wc)==88
  if r['category']==2:
   if r['weapon_status']=='found':
    c['descriptor_abi'] &= wc[:16]==b[0xd0:0xe0] and wc[84]==r['weapon_type'];types[r['weapon_type']]+=1
   else:unknown.append(dict(id=r['id'],status=r['weapon_status']))
  output.append(dict(id=r['id'],name=name.decode('utf-8',errors='replace').rstrip('\0'),category=r['category'],slot=r['slot'],weapon_type=r['weapon_type'] if r['weapon_status']=='found' else None,visual_model=b[0x158:0x168].hex(),visual_entity=b[0x148:0x158].hex(),models=[dict(mask=struct.unpack_from('<I',models,k)[0],guid=models[k+4:k+20].hex()) for k in range(0,len(models),20)]))
 observed={e['item']:e for e in ev('attachment_weapon_category')};matches=[]
 for ident,e in observed.items():
  row=ids.get(ident,{});ok=row.get('selected',False) and bytes.fromhex(row['item_bytes'])[:0x108]==bytes.fromhex(e['raw_item']) and (row['weapon_type']==e['type'] if e['status']=='found' else True)
  c['equipped_prefix_matches'] &= ok;matches.append(dict(id=ident,match=ok))
 c['equipped_prefix_matches'] &= len(matches)>=6
 model_matches=[]
 for e in ev('native_grip_sample'):
  row=next((r for r in output if r['id']==e['item']),None)
  if row:model_matches.append(dict(item=e['item'],frame=e['frame'],render_model=e['weapon_guid'],direct_match=e['weapon_guid']==row['visual_model'],variant_match=e['weapon_guid'] in [m['guid'] for m in row['models']]))
 return dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,items=len(rows),selected=len(selected),weapon_types=dict(types),unresolved_weapon_categories=unknown,equipped_matches=matches,observed_model_matches=model_matches,catalog_sha256=hashlib.sha256(data).hexdigest(),entries=output,limitation='Resident ItemInfo definitions and model references only. Does not certify every geometry asset, grip, spawnability, animation, or VR handling.')
if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);a=p.parse_args();r=audit(a.probe);(a.probe/'item-catalog-check.json').write_text(json.dumps(r,indent=2,ensure_ascii=False)+'\n',encoding='utf-8');print(json.dumps({k:v for k,v in r.items() if k not in ['entries','observed_model_matches']},indent=2));raise SystemExit(r['status']!='PASS')
