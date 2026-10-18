"""Decode bounded native action-resource snapshots against local PE reflection."""
import argparse,json,pathlib,struct,sys
sys.path.insert(0,str(pathlib.Path(__file__).parent/'python'))
import pefile

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--output',type=pathlib.Path,help='Write a new analysis without replacing a historical report');a=p.parse_args()
 pe=pefile.PE('enshrouded.exe',fast_load=True);types=json.loads(pathlib.Path('docs/analysis/reflection.json').read_text(encoding='utf8'));byhash={}
 for t in types:
  h=struct.unpack('<I',pe.get_data(int(t['metadata_rva'],16)+80,4))[0];byhash.setdefault(h,[]).append(t)
 rs=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];snap=[r for r in rs if r['event']=='sequence_asset_snapshot'];out=[]
 for r in snap:
  ss=[]
  for s in r['subsequences']:
   sub_bytes=bytes.fromhex(s['bytes']);es=[]
   for e in s['events']:
    matches=byhash.get(e['type_hash'],[]);v=dict(index=e['index'],hash=hex(e['type_hash']),types=[t['qualified_name'] for t in matches],address=e['address'])
    if e.get('base_event'):
     d=bytes.fromhex(e['base_event']);v['time'],v['duration']=struct.unpack_from('<2q',d)
    if e.get('spawn_impact'):
     d=bytes.fromhex(e['spawn_impact']);v.update(spawn_transform=d[0x90],attachment_slot=d[0x91],spawn_slot=d[0x92],despawn_after_sequence=bool(d[0xfd]),transfer_used_item=bool(d[0xff]),impact_guid=d[0x148:0x158].hex(),event_guid=d[0x158:0x168].hex(),collider_count=struct.unpack_from('<I',d,0x1a0)[0],collider_attachment_slot=d[0x1c8],shapes=[dict(hash=hex(x['type_hash']),types=[t['qualified_name'] for t in byhash.get(x['type_hash'],[])],bytes=x['base_data']) for x in e['shapes']])
    if e.get('shapes'):
     for shape,raw in zip(v['shapes'],e['shapes']):
      b=bytes.fromhex(raw['base_data']);shape.update(offset=struct.unpack_from('<3f',b,8),orientation=struct.unpack_from('<4f',b,20))
      if raw.get('box_data'):
       b=bytes.fromhex(raw['box_data']);shape.update(half_size=struct.unpack_from('<3f',b,80),continuous=bool(b[92]))
    es.append(v)
   ss.append(dict(index=s['index'],length=struct.unpack_from('<q',sub_bytes,8)[0],loop=bool(sub_bytes[16]),events=es))
  out.append(dict(sequence=r['sequence'],item=r['item'],entity=r['entity'],native_length=r['native_length'],stable=r['stable_read'],read_only=r['read_only'],subsequences=ss))
 result=dict(snapshots=out,failures=[r for r in rs if r['event']=='sequence_asset_failure'],limit='Observed active sequence registry and event data, not proof that each authored event executed or caused damage.')
 (a.output or a.probe/'sequence-asset-analysis.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf8')
 print(json.dumps(result,indent=2));return not out or bool(result['failures'])
if __name__=='__main__':raise SystemExit(main())
