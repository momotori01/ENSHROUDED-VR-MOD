"""Replay archived native event arrays with different evidence budgets; never calls or edits the game."""
import argparse,hashlib,json,subprocess
from pathlib import Path

def key(row):
 return tuple(row[k] for k in ('world','entity','sequence','item','trigger'))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('output',type=Path);a=p.parse_args()
 if a.output.exists():raise ValueError('preserve existing output')
 proof=json.loads((a.probe/'melee-contact-checks.json').read_text())
 if not proof['checks'] or set(proof['checks'].values())!={'PASS'}:raise ValueError('successful native contact proof required')
 source=a.probe/'view-resource-probe.jsonl';rows=[json.loads(l) for l in source.read_text().splitlines()]
 selected=[r for r in rows if r['event']=='motion_event_selection'];deltas=[r for r in rows if r['event']=='motion_event_retime'];shadows=[r for r in rows if r['event']=='motion_event_selection_shadow']
 actions={};records=[];used=[]
 for delta in deltas:
  if delta['operation']!='append':continue
  added=set(delta['after'])-set(delta['before'])
  if len(added)!=1:raise ValueError('ambiguous appended event')
  variant=added.pop();shadow=[r for r in shadows if r['us']<delta['us'] and r['world']==delta['world'] and r['entity']==delta['entity'] and any(e['variant']==variant for e in r['events'])][-1]
  event=next(e for e in shadow['events'] if e['variant']==variant);guid=bytes.fromhex(event['spawn_impact'])[0x158:0x168].hex()
  if key(delta) in actions:raise ValueError('duplicate native action')
  actions[key(delta)]=(delta['call'],delta['sub'],int(variant,16),guid)
 for index,row in enumerate(selected):
  stop=selected[index+1]['us'] if index+1<len(selected) else float('inf')
  changes=[d for d in deltas if row['us']<d['us']<stop and key(d)==key(row)]
  original=[int(e['variant'],16) for e in row['events']];expected=original[:]
  for delta in changes:
   if expected!=[int(v,16) for v in delta['before']]:raise ValueError('native delta chain mismatch')
   expected=[int(v,16) for v in delta['after']];used.append(delta['us'])
  k=key(row)
  if k not in actions:raise ValueError('unqualified action in source')
  early=int(row['range'][0]==0)
  if early and not any(d['operation']=='append' for d in changes):raise ValueError('early selection without measured append')
  values=[int(k[0],16),*k[1:],*actions[k],early,early,row['capacity'],len(original),*original,len(expected),*expected]
  records.append(' '.join(map(str,values)))
 if len(actions)!=2 or len(used)!=len(deltas) or len(set(used))!=len(used):raise ValueError('incomplete native evidence')
 a.output.mkdir(parents=True);data=a.output/'native-events.tsv';data.write_text('\n'.join(records)+'\n')
 executable=Path('vrmod/build/bin/Release/evr_contact_recording_replay.exe').resolve();results={};checks={}
 for limit in (0,1,3,64,256,len(records)+1):
  run=subprocess.run([str(executable),str(data.resolve()),str(limit)],capture_output=True,text=True);report=json.loads(run.stdout);results[str(limit)]=report
  checks['budget_'+str(limit)]=run.returncode==0 and report['rows']==len(records) and report['recorded']==min(limit,len(records)) and report['appended']==report['suppressed']==2 and report['mismatches']==0
 old=subprocess.run([str(executable),str(data.resolve()),'3','simulate-old-cutoff'],capture_output=True,text=True);results['counterfactual_old_cutoff_3']=json.loads(old.stdout)
 checks['counterfactual_processing_cutoff_detected']=old.returncode==2 and results['counterfactual_old_cutoff_3']['mismatches']>0
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},source=str(a.probe),source_sha256=hashlib.sha256(source.read_bytes()).hexdigest(),results=results,limitation='Archived native event replay. Old cutoff is an explicit counterfactual simulation, not a newly observed game failure. No continuous session or native entity lifetime claim.')
 (a.output/'checks.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
