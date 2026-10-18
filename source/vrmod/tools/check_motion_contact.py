"""Audit read-only native Impact callbacks and extract contact geometry evidence.
Does not assert that a traced query belongs to the local actor or applies damage.
"""
import argparse,collections,copy,json,math,pathlib,struct

def audit(rows):
 samples=[r for r in rows if r.get('event')=='motion_contact_sample']
 enabled=[r for r in rows if r.get('event')=='motion_contact_enabled']
 removed=[r for r in rows if r.get('event')=='motion_contact_removed']
 checks=[]
 def check(name,ok):checks.append(dict(name=name,status='PASS' if ok else 'FAIL'))
 check('three_expected_callbacks', {r['name'] for r in enabled}=={'startQuery','GetSlotTransform','fetchQueryResult'} and len(enabled)==3)
 check('bounded_observations',0<len(samples)<=2051)
 check('captured_queries_during_local_action',any(r.get('latest_actor_active') and r['name']=='startQuery' for r in samples))
 check('no_observation_failures',not any(r.get('event')=='motion_contact_failure' for r in rows))
 check('native_return_forwarded',bool(samples) and all(r.get('return_matches_output') for r in samples))
 check('vm_argument_storage_stable',bool(samples) and all(r.get('argument_storage_stable') and all(a.get('descriptor_stable') for a in r['arguments']) for r in samples))
 check('observation_only_no_owner_claim',bool(samples) and all(r.get('read_only') and r.get('owner_proven') is False for r in samples))
 check('all_hooks_restored_quiescent',len(removed)==1 and removed[0].get('original_bytes_restored') and removed[0].get('remaining_callbacks')==0 and removed[0].get('read_failures')==0)
 # Native outputs may change; declared input slots must not be altered by either
 # the observed native routine or our wrapper during this call.
 inputs={'startQuery':[1,2,3,4],'GetSlotTransform':[2,3],'fetchQueryResult':[6,7]}
 sizes={'startQuery':[4,2,1,56,4],'GetSlotTransform':[56,12,1,4],'fetchQueryResult':[4,12,12,1,1,1,4,4]}
 check('native_input_bytes_unchanged',bool(samples) and all(all(r['arguments'][i]['before']==r['arguments'][i]['after'] for i in inputs[r['name']]) for r in samples))
 check('argument_byte_widths',bool(samples) and all(len(r['arguments'])==len(sizes[r['name']]) and all(len(a['before'])==len(a['after'])==(2*n if a['id'] else 0) for a,n in zip(r['arguments'],sizes[r['name']])) for r in samples))
 check('restoration_sample_count',len(removed)==1 and removed[0]['samples']==len(samples))
 return checks

def decode_arg(row,index,kind='I',when='after'):
 value=row['arguments'][index][when]
 if not value:return None
 b=bytes.fromhex(value)
 if kind=='world':
  pos=struct.unpack_from('<3q',b);q=struct.unpack_from('<4f',b,24);scale=struct.unpack_from('<3f',b,40)
  return dict(position=[v/2**32 for v in pos],q32=pos,orientation=q,scale=scale)
 v=struct.unpack('<'+kind,b);return v[0] if len(v)==1 else v

def analyze(rows):
 samples=[r for r in rows if r.get('event')=='motion_contact_sample'];start=[];slots=[];hits=[]
 for r in samples:
  shared={k:r[k] for k in ['sample','us','thread','scope','vm_data','latest_actor_active','latest_actor_trigger']}
  if r['name']=='GetSlotTransform':slots.append(dict(**shared,result=r['result_word'],entity=decode_arg(r,3),slot=decode_arg(r,2,'B'),transform=decode_arg(r,0,'world'),address=r['arguments'][0]['address']))
  if r['name']=='startQuery':
   v=dict(**shared,query_before=decode_arg(r,0,when='before'),query_after=decode_arg(r,0),flags=decode_arg(r,1,'H'),sort=decode_arg(r,2,'B'),transform=decode_arg(r,3,'world'),range=decode_arg(r,4,'f'),address=r['arguments'][3]['address'])
   recent=[s for s in slots if s['vm_data']==r['vm_data'] and s['thread']==r['thread'] and s['us']<=r['us'] and s['result']==256]
   if recent:
    s=recent[-1];v['preceding_slot']=dict(sample=s['sample'],entity=s['entity'],slot=s['slot'],delta_us=r['us']-s['us'],same_address=s['address']==v['address'],same_transform=s['transform']==v['transform'])
   start.append(v)
  if r['name']=='fetchQueryResult':hits.append(dict(**shared,result=r['result_word'],query=decode_arg(r,6),index=decode_arg(r,7),target=decode_arg(r,0),position=decode_arg(r,1,'3f'),normal=decode_arg(r,2,'3f'),flags=[decode_arg(r,i,'B') for i in [3,4,5]]))
 return dict(counts=dict(collections.Counter(r['name'] for r in samples)),during_local_action=dict(collections.Counter(r['name'] for r in samples if r['latest_actor_active'])),slot_entities=dict(collections.Counter(str(s['entity']) for s in slots)),slots=slots,queries=start,hits=hits,limit='Temporal correlation is not proof of source owner, actual damage, or controller collision integration.')

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args()
 rows=[json.loads(s) for s in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];checks=audit(rows);summary=analyze(rows)
 if a.negative_tests:
  cases=[('lost_restore',lambda r: r.update(original_bytes_restored=False),'motion_contact_removed'),('wrong_return',lambda r:r.update(return_matches_output=False),'motion_contact_sample'),('changed_input',lambda r:r['arguments'][3 if r['name']=='startQuery' else 2 if r['name']=='GetSlotTransform' else 6].update(after='ff'),'motion_contact_sample'),('claimed_owner',lambda r:r.update(owner_proven=True),'motion_contact_sample')]
  for name,change,event in cases:
   altered=copy.deepcopy(rows);target=next((r for r in altered if r.get('event')==event),None)
   if target:change(target)
   checks.append(dict(name='reject_'+name,status='PASS' if target and any(c['status']=='FAIL' for c in audit(altered)) else 'FAIL'))
 result=dict(checks=checks,passed=sum(c['status']=='PASS' for c in checks),failed=sum(c['status']=='FAIL' for c in checks),counts=summary['counts'],during_local_action=summary['during_local_action'])
 (a.probe/'motion-contact-checks.json').write_text(json.dumps(result,indent=2)+'\n');(a.probe/'motion-contact-analysis.json').write_text(json.dumps(summary,indent=2)+'\n')
 print(json.dumps(result,indent=2));return bool(result['failed'])
if __name__=='__main__':raise SystemExit(main())
