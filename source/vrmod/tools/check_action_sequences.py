"""Read-only sequence evidence; raw native Time units, not combat cooldown proof."""
import argparse,json,pathlib,struct,statistics
ROOT=pathlib.Path(__file__).resolve().parents[2]
TYPES={x['qualified_name']:x for x in json.loads((ROOT/'docs/analysis/reflection.json').read_text(encoding='utf-8'))}
BASE='keen::ecs::BaseActor'
def field(root,path):
    offset=0
    for name in path.split('.'):
        matches=[f for f in TYPES[root]['fields'] if f['name']==name]
        if len(matches)!=1:raise ValueError('reflection field missing/ambiguous: '+root+'.'+name)
        f=matches[0];offset+=f['offset'];root=f['type']
    return offset,TYPES[root]['size']
def value(raw,path,fmt,root=BASE,base=0):
    offset,size=field(root,path)
    if struct.calcsize('<'+fmt)!=size:raise ValueError('reflection width mismatch: '+path)
    if base+offset+size>len(raw):raise ValueError('truncated record')
    return struct.unpack_from('<'+fmt,raw,base+offset)[0]
def decode(raw):
    if len(raw)!=TYPES[BASE]['size']:raise ValueError('wrong BaseActor size')
    out={}
    for prefix,path in [('trigger','currentAction.triggerContext.sequenceRuntimeId'),('runtime','sequenceRuntimeId')]:
        for name,fmt in [('sequenceId','I'),('triggerTime','q'),('actorEntityId','I'),('callIndex','B')]:out[prefix+'_'+name]=value(raw,path+'.'+name,fmt)
    trigger='currentAction.triggerContext.';play='currentAction.playbackContext.'
    for name,fmt in [('triggerType','H'),('inputType','B'),('jumpType','B'),('startTime','q')]:out[name]=value(raw,trigger+name,fmt)
    for name,fmt in [('subSequenceId','B'),('playedTime','q'),('currentTime','q'),('sequenceLength','q'),('scopeStartTime','q'),('scopeEndTime','q'),('state','B'),('setStateMask','Q'),('unsetStateMask','Q'),('abilityMask','I')]:out[name]=value(raw,play+name,fmt)
    out['usedItem']=value(raw,'currentAction.triggerContext.usedItem.itemId','I')
    out['currentState']=value(raw,'currentState','Q');out['currentAbilityMask']=value(raw,'currentAbilityMask','I')
    out['effectiveState']=(out['currentState']|out['setStateMask'])&~out['unsetStateMask'] if out['state']&1 else out['currentState']
    start,size=field(BASE,play+'followUps');stride=TYPES['keen::actor::FollowUp']['size']
    if size!=6*stride:raise ValueError('follow up array width changed')
    out['followUps']=[{name:value(raw,name,fmt,'keen::actor::FollowUp',start+i*stride) for name,fmt in [('preconfiguredSequence','I'),('localExecutionTime','q'),('wasTriggered','B')]} for i in range(6)]
    return out

def check(rows):
    ev=lambda n:[e for e in rows if e['event']==n]
    samples=ev('action_sequence_snapshot');end=ev('action_sequence_removed');begin=ev('action_sequence_enabled')
    checks={'action_explicit_readonly_policy':len(begin)==1 and begin[0]['read_only'] and begin[0]['bytes']==3104 and begin[0]['time_unit_known'] is False,
      'action_hooks_restored':len(end)==1 and end[0]['restored'] and end[0]['callbacks']==0,
      'action_no_read_failures':not ev('action_sequence_failure') and len(end)==1 and end[0]['failures']==0,
      'action_samples_accounted':len(end)==1 and 0<len(samples)==end[0]['samples']<=4096,
      'action_local_stable_boundary':bool(samples) and all(e['read_only'] and e['stable_read'] and e['entity']==e['local_entity']!=0 and e['caller']=='0x2894fb' for e in samples),
      'action_contiguous_sample_numbers':[e['sample'] for e in samples]==list(range(1,len(samples)+1)),
      'action_observation_completed':len(ev('input_observation_complete'))==1 and ev('input_observation_complete')[0]['result']==0}
    decoded=[];errors=[]
    for e in samples:
        try:d=decode(bytes.fromhex(e['base_actor_bytes']));d.update(sample=e['sample'],us=e['us'],tick=e['tick'],entity=e['entity']);decoded.append(d)
        except (ValueError,KeyError,struct.error) as ex:errors.append(str(ex))
    checks['action_reflection_sizes_decode']=bool(decoded) and not errors and len(decoded)==len(samples)
    checks['action_known_state_bits']=bool(decoded) and all(d['state']&~63==0 for d in decoded)
    checks['action_followup_bool_width']=bool(decoded) and all(all(f['wasTriggered'] in (0,1) for f in d['followUps']) for d in decoded)
    # Inactive records may retain the previous action; do not count them as playing.
    active=[d for d in decoded if d['state']&1]
    checks['action_active_trigger_entity_local']=bool(decoded) and all(d['trigger_actorEntityId']==d['entity'] for d in active)
    changes=[];last=None;rates=[]
    for d in decoded:
        key=tuple(d[k] for k in ['trigger_sequenceId','trigger_triggerTime','subSequenceId','state','effectiveState'])
        if key!=last:changes.append(d);last=key
    # Callback wall-time spacing interleaves native prediction/server domains.
    # Do not infer a clock frequency by dividing adjacent samples across them.
    # Raw timing is sufficient to observe action progress; unit remains unknown.
    metrics=dict(samples=len(samples),active_samples=len(active),sequences=sorted({d['trigger_sequenceId'] for d in active}),
        state_values=sorted({d['state'] for d in decoded}),unstable_reads=end[0]['unstable_reads'] if end else None,
        time_unit_known=False,clock_rate_inference='not attempted: interleaved prediction domains',decode_errors=errors)
    return checks,metrics,changes

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=pathlib.Path);a=p.parse_args()
    rows=[json.loads(x) for x in (a.probe/'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines()]
    checks,metrics,changes=check(rows)
    (a.probe/'action-sequences-checks.json').write_text(json.dumps(dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},metrics=metrics,changes=changes,limitation=__doc__),indent=2)+'\n',encoding='utf-8')
    for k,v in checks.items():print('PASS' if v else 'FAIL',k)
    print(json.dumps(metrics));print('PASS',sum(checks.values()),'FAIL',sum(not v for v in checks.values()));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
