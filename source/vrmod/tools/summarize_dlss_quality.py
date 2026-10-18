"""Read-only DLSS dimension and API audit for a completed Mock quality comparison."""
import argparse,collections,json,statistics
from pathlib import Path

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('launch',type=Path);a=p.parse_args()
 settings=json.loads((a.launch/'vr-render-settings.json').read_text(encoding='utf-8-sig'))
 quality='Dlaa' if settings['applied']['antiAliasing']=='Dlaa' else settings['applied']['dlss']
 expected={'Balanced':1139,'Quality':1309,'Dlaa':1963}.get(quality)
 groups={};beats=[]
 for log in a.launch.glob('view-resource-probe-*/view-resource-probe.jsonl'):
  for line in log.open():
   e=json.loads(line)
   if e.get('event')=='session_heartbeat':beats.append(e)
   if e.get('event')!='dlss_eval':continue
   out=e.get('output') or {}
   if out.get('w')!=1963 or out.get('h')!=1963:continue
   key=out['image'];g=groups.setdefault(key,dict(samples=0,subrect=collections.Counter(),results=collections.Counter(),reset_samples=0,features=set(),color_sizes=set(),jitter=set()))
   g['samples']+=1;g['subrect'][tuple(e['subrect'])]+=1;g['results'][e['result']]+=1;g['reset_samples']+=bool(e['reset'])
   g['features'].add(e['feature']);g['color_sizes'].add((e['color']['w'],e['color']['h']));g['jitter'].add(tuple(e['jitter']))
 results=[]
 for image,g in groups.items():
  results.append(dict(output_image=image,output_size=[1963,1963],samples=g['samples'],input_subrect=[dict(size=k,samples=v) for k,v in g['subrect'].items()],result_counts=dict(g['results']),reset_samples=g['reset_samples'],features=sorted(g['features']),color_sizes=sorted(g['color_sizes']),unique_jitter=len(g['jitter'])))
 play=json.loads((a.launch/'play-session-check.json').read_text(encoding='utf-8-sig'))
 close=json.loads((a.launch/'ui-close-request.json').read_text(encoding='utf-8-sig'))
 checks=dict(play_pass=play['status']=='PASS',normal_close=close.get('status')=='exited_after_request' and close.get('exit_code')==0,requested_input_size=expected is not None and all(set(g['subrect'])=={(expected,expected)} for g in groups.values()),two_eye_outputs=len(results)==2,enough_samples=all(g['samples']>=64 for g in results),successful_api=bool(results) and all(set(g['result_counts'])=={'0x1'} for g in results),independent_features=len(set(f for g in results for f in g['features']))==2,restored_settings=settings.get('restore',{}).get('sha256_final')==settings['sha256_before'])
 report=dict(status='PASS' if all(checks.values()) else 'FAIL',checks=checks,quality=quality,eyes=results,heartbeats=beats,scope='Dimensions/API evidence, not image-quality PASS. Capture uses GPU completion waits. Short Mock rates are not real-headset performance.')
 (a.launch/'dlss-quality-audit.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
 print(json.dumps(report,indent=2));return int(report['status']!='PASS')
if __name__=='__main__':raise SystemExit(main())
