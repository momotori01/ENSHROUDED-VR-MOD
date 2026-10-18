"""Verify a real Sky window-close request during the exact native attack.
This proves request timing and active Actor destruction, NOT successful VR cleanup.
"""
import argparse,copy,hashlib,json,re
from datetime import datetime
from pathlib import Path

def seconds(value):return datetime.fromisoformat(re.sub(r'(\.\d{6})\d+',r'\1',value.replace('Z','+00:00'))).timestamp()
def audit(rows,trace,anchor):
 ev=lambda n:[r for r in rows if r['event']==n]
 app=[r for r in ev('motion_event_retime') if r['operation']=='append'];b=ev('contact_actor_owner_destroy_begin');e=ev('contact_actor_owner_destroy_end')
 c={'single_native_attack_and_actor_end':len(app)==len(b)==len(e)==1}
 c['ordinary_UI_close_sent']=trace.get('status')=='UI_CLOSE_SENT' and trace.get('action')=='Sky Alt_L+F4 ordinary window close' and trace.get('native_action_invoked') is False and trace.get('force_kill') is False
 if not c['single_native_attack_and_actor_end'] or not c['ordinary_UI_close_sent']:return c,{}
 app,b,e=app[0],b[0],e[0]
 before,after=seconds(anchor['utc_before']),seconds(anchor['utc_after']);qpc=int(anchor['qpc_ticks'])*1e6/anchor['frequency'];start,finish=seconds(trace['request_begin_utc']),seconds(trace['request_end_utc'])
 low=qpc+(start-after)*1e6;high=qpc+(finish-before)*1e6
 c['bounded_clock_and_input_interval']=anchor['high_resolution'] is True and 1e6<=anchor['frequency']<=1e9 and 0<=after-before<=.05 and 0<=finish-start<=1 and low<=high
 c['exact_detected_append']=trace['trigger']==app and trace['anchor']==anchor and 0<=trace['estimated_detection_age_ms']<=250
 c['native_action_still_active_after_input_delivery']=app['us']<low<=high<b['us'] and any(r['us']>=high and r['us']<b['us'] and r['active'] and all(r[k]==app[k] for k in ('world','sequence','item','trigger')) for r in ev('contact_quiescence_sample')) and not any(app['us']<r['us']<high and not r['active'] for r in ev('contact_quiescence_sample'))
 c['same_active_actor_genuinely_destroyed']=b['active'] and e['active'] and b['present'] and e['native_result']==1 and not e['after_present'] and not e['reads_destroyed_actor'] and all(app[k]==b[k]==e[k] for k in ('world','entity','sequence','item','trigger','epoch_provider','epoch_serial')) and b['owner_call']==e['owner_call'] and b['thread']==e['thread'] and b['us']<e['us']
 return c,dict(detection_age_ms=trace['estimated_detection_age_ms'],input_begin_earliest_us=low,input_end_latest_us=high,clock_uncertainty_us=(after-before)*1e6,append_to_actor_entry_us=b['us']-app['us'],scope=__doc__)

def load(probe):
 paths=[probe/'view-resource-probe.jsonl',probe.parent/'ui-attack-exit.json',probe.parent/'ui-exit-clock-anchor.json'];raw=[p.read_bytes() for p in paths]
 rows=[json.loads(l) for l in raw[0].splitlines()];trace=json.loads(raw[1].decode('utf-8-sig'));anchor=json.loads(raw[2].decode('utf-8-sig'))
 c,m=audit(rows,trace,anchor);c['same_probe_path']=Path(trace['probe']).resolve()==paths[0].resolve()
 return c,m,paths,raw,rows,trace,anchor

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--negative-tests',action='store_true');a=p.parse_args();c,m,paths,raw,rows,trace,anchor=load(a.probe)
 if a.negative_tests:
  for n,where,change,target in [('no_UI','trace',{'status':'DETECTED'},'ordinary_UI_close_sent'),('force_kill','trace',{'force_kill':True},'ordinary_UI_close_sent'),('stale_detection','trace',{'estimated_detection_age_ms':999},'exact_detected_append'),('idle_actor','begin',{'active':False},'same_active_actor_genuinely_destroyed'),('different_action','begin',{'trigger':0},'same_active_actor_genuinely_destroyed'),('failed_original','end',{'native_result':0},'same_active_actor_genuinely_destroyed')]:
   rr=list(rows);tt=copy.deepcopy(trace);aa=dict(anchor)
   if where=='trace':tt.update(change)
   else:
    event='contact_actor_owner_destroy_begin' if where=='begin' else 'contact_actor_owner_destroy_end';i=next(i for i,r in enumerate(rows) if r['event']==event);rr[i]=dict(rr[i],**change)
   c['reject_'+n]=bool(c.get(target)) and not audit(rr,tt,aa)[0].get(target,True)
 report=dict(status='PASS' if all(c.values()) else 'FAIL',checks=c,metrics=m,sources=[dict(path=str(p),sha256=hashlib.sha256(b).hexdigest()) for p,b in zip(paths,raw)])
 (a.probe/'ui-attack-exit-checks.json').write_text(json.dumps(report,indent=2)+'\n')
 for k,v in c.items():print('PASS' if v else 'FAIL',k)
 print(m);return int(not all(c.values()))
if __name__=='__main__':raise SystemExit(main())
