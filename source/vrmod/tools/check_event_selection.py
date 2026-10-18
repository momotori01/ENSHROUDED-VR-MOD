"""Independently audit local native event timing without changing gameplay."""
import argparse,copy,json,pathlib,struct
from emulate_event_selection import allowed,select_reference,IMPACT

def audit(rows,shadow=False,retime=False):
 ev=lambda n:[r for r in rows if r['event']==n]
 samples=ev('motion_event_selection');assets=ev('sequence_asset_snapshot');enabled=ev('motion_event_selection_enabled');removed=ev('motion_event_selection_removed')
 c={'selection_samples':len(samples)>10,'selection_enabled_once':len(enabled)==1 and enabled[0]['read_only']==(not retime),'selection_restored_drained':len(removed)==1 and removed[0]['restored'] and removed[0]['callbacks']==removed[0]['failures']==0 and removed[0]['samples']==len(samples),'no_failures':not any('failure' in r['event'] for r in rows),'exact_native_caller':bool(samples) and all(r['caller']=='0x1e88ad' and r['read_only'] and r['return_unchanged'] and r['actor_unchanged'] for r in samples),'one_local_action':bool(samples) and len({(r['world'],r['entity'],r['sequence'],r['item'],r['trigger']) for r in samples})==1 and all(r['world']!='0x0' and r['entity'] and r['sequence'] and r['item'] for r in samples),'contiguous_samples':bool(samples) and [r['sample'] for r in samples]==list(range(1,len(samples)+1))}
 if not samples or len(assets)!=1:return {**c,'one_asset':False},{}
 asset=assets[0];events=asset['subsequences'][0]['events'];n=len(samples)
 c['asset_matches_action']=all(r['sequence']==asset['sequence'] and r['item']==asset['item'] and r['entity']==asset['entity'] and r['trigger']==asset['trigger'] for r in samples)
 c['native_ranges_contiguous']=samples[0]['range'][0]==0 and all(r['range'][0]<r['range'][1] and not r['second_range'] and not r['include_overlap'] and not r['filter_present'] for r in samples) and all(a['range'][1]==b['range'][0] for a,b in zip(samples,samples[1:]))
 active=[r for r in ev('action_sequence_snapshot') if bytes.fromhex(r['base_actor_bytes'])[0x1b1]&1];tagsset={bytes.fromhex(r['base_actor_bytes'])[0xec:0x16c] for r in active}
 c['tags_explicit_or_constant_observation']=all('state_tags' in r for r in samples) or len(tagsset)==1
 fallback=next(iter(tagsset),bytes(128));same=True;selection=True;allselected=[];impacts=[]
 for r in samples:
  raw=bytes.fromhex(r['state_tags']) if 'state_tags' in r else fallback;tags=[raw[i:i+16] for i in range(0,len(raw),16)];expected=select_reference(events,*r['range'],tags)
  indices=[]
  for e in r['events']:
   matches=[(i,x) for i,x in enumerate(events) if x['address']==e['payload']]
   if len(matches)!=1:same=False;continue
   i,x=matches[0];indices.append(i);same &= all(x[k]==e[k] for k in ['type_hash','variant_bytes','base_event'])
   if e['type_hash']==IMPACT:
    same &= x['spawn_impact']==e['spawn_impact'];impacts.append((r,e))
  selection &= expected==indices;allselected+=indices
 c['event_assets_byte_identical']=same and bool(allselected);c['native_time_and_condition_selection_exact']=selection;c['no_duplicate_event_emission']=len(set(allselected))==len(allselected);c['one_authored_impact_selected']=len(impacts)==1
 collider=ev('sequence_collider_ready_sample');owned=[]
 if impacts:
  row,e=impacts[0];guid=bytes.fromhex(e['spawn_impact'])[0x158:0x168]
  for r in collider:
   b=bytes.fromhex(r['event_bytes'])
   if b[8:24]==guid and r['sequence']==row['sequence']:owned.append(r)
  c['impact_before_native_collider']=bool(owned) and row['us']<owned[0]['us'] and owned[0]['collider_id_before_native']==0
  c['one_native_impact_entity']=bool(owned) and len({r['entity'] for r in owned})==1
 else:c['impact_before_native_collider']=c['one_native_impact_entity']=False
 bindings=ev('impact_query_world_binding');c['world_qualified_by_exact_actor_storage']=bool(samples) and any(r['world']==samples[0]['world'] and r['entity']==samples[0]['entity'] and r['bytes']==3104 and r['query_pointer_match'] for r in bindings)
 metrics=dict(samples=n,selected_events=len(allselected),impact_count=len(impacts),native_collider_updates=len(owned))
 if impacts:metrics.update(impact_start=struct.unpack_from('<q',bytes.fromhex(impacts[0][1]['base_event']))[0],duration=struct.unpack_from('<q',bytes.fromhex(impacts[0][1]['base_event']),8)[0])
 if shadow:
  s=ev('motion_event_selection_shadow');c['one_private_shadow_query']=len(s)==1
  c['shadow_does_not_write_native_sources']=len(s)==1 and all(s[0][k] for k in ['actor_unchanged','output_unchanged','private_actor_unchanged','input_filter_unchanged','return_success']) and not s[0]['applied']
  if s:
   r=s[0];c['shadow_at_native_start']=r['source_range'][0]==0 and r['candidate_range']==[0,asset['native_length']] and r['us']<impacts[0][0]['us'] if impacts else False
   c['shadow_same_impact_before_authored_time']=len(impacts)==1 and len(r['events'])==1 and r['events'][0]['spawn_impact']==impacts[0][1]['spawn_impact'] and r['events'][0]['type_hash']==IMPACT and r['source_range'][1]<metrics['impact_start']
  else:c['shadow_at_native_start']=c['shadow_same_impact_before_authored_time']=False
 return c,metrics

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--shadow',action='store_true');p.add_argument('--negative-tests',action='store_true');a=p.parse_args();rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf8').splitlines()];c,m=audit(rows,a.shadow)
 if a.negative_tests:
  mutations=[('wrong_caller','motion_event_selection','caller','0x0'),('source_changed','motion_event_selection','actor_unchanged',False),('wrong_world','motion_event_selection','world','0x0'),('bad_range','motion_event_selection','range',[123,124]),('hook_not_restored','motion_event_selection_removed','restored',False)]
  if a.shadow:mutations += [('shadow_applied','motion_event_selection_shadow','applied',True),('private_source_changed','motion_event_selection_shadow','private_actor_unchanged',False)]
  for name,event,key,value in mutations:
   bad=copy.deepcopy(rows);r=next((r for r in bad if r['event']==event),None)
   if r:r[key]=value
   c['reject_'+name]=r is not None and not all(audit(bad,a.shadow)[0].values())
 out=dict(checks={k:'PASS' if v else 'FAIL' for k,v in c.items()},passed=sum(c.values()),failed=sum(not v for v in c.values()),metrics=m,limitation=__doc__)
 (a.probe/'event-selection-checks.json').write_text(json.dumps(out,indent=2)+'\n',encoding='utf8')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print('PASS',out['passed'],'FAIL',out['failed'],m);return bool(out['failed'])
if __name__=='__main__':raise SystemExit(main())
