"""Strict per-pose eye coverage, with explicit unsubmitted warmup evidence only."""
from collections import Counter

def audit(events,poses):
 ev=lambda n:[x for x in events if x['event']==n]
 eyes=ev('motion_bow_eye_payload');counts=Counter((x['serial'],x['eye']) for x in eyes);warm=[]
 preparing=Counter(x['serial'] for x in ev('world_pair_preparing') if x['submitted'] is False)
 ended=Counter((x['serial'],x['frame']) for x in ev('world_warmup_frame_ended') if x['layers']==0)
 pipelines={}
 for x in ev('world_pipeline_state'):
  if x['eye'] in [0,1]:pipelines.setdefault(x['serial'],[]).append(x)
 for s,p in poses.items():
  if counts[(s,0)]==counts[(s,1)]==0:
   states=pipelines.get(s,[])
   if preparing[s]==1 and ended[(s,p['frame'])]==1 and len(states)==2 and {x['eye'] for x in states}=={0,1} and all(x['ready']==0 and x['result_flags']==0 for x in states):
    warm.append(s)
 expected=Counter((s,eye) for s in poses if s not in warm for eye in [0,1])
 payloads=all(x['serial'] in poses and x['pose_handle']==poses[x['serial']]['pose_handle'] and x['hand_instance']==(poses[x['serial']]['handle']&0xffffff) and (poses[x['serial']]['source_handle']&0xffffff) in x['indices'] for x in eyes)
 return dict(status='PASS' if counts==expected and payloads and bool(expected) else 'FAIL',presented_poses=len(poses)-len(warm),unsubmitted_warmup_serials=warm,eye_payloads=len(eyes))
