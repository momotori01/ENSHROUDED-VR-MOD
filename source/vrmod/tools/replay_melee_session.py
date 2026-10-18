"""Replay an archived successful measured two-hand native trial through the unlimited coordinator. No live game calls."""
import argparse,json,struct,subprocess,bisect,hashlib
from pathlib import Path
from check_motion_two_hand import sequence
from check_recorded_xr_native import multiply
from math import sin,cos,pi

def quat(v):
 y,p,r=[x*pi/360 for x in v];return multiply(multiply([0,sin(y),0,cos(y)],[sin(p),0,0,cos(p)]),[0,0,sin(r),cos(r)])
def main():
 a=argparse.ArgumentParser(description=__doc__);a.add_argument('probe',type=Path);a.add_argument('output',type=Path);a.add_argument('--negative-tests',action='store_true');args=a.parse_args();probe=args.probe;out=args.output
 if out.exists():raise ValueError('preserve previous replay outputs')
 if json.loads((probe/'probe-status.json').read_text())['status']!='PASS':raise ValueError('successful native source required')
 rows=[json.loads(l) for l in (probe/'view-resource-probe.jsonl').read_text().splitlines()];poses=sequence(probe.parent/'mock-hands.csv')
 # Use renderer's qualified client-world/entity/equipment identity, because old
 # action snapshot-only probes intentionally logged world=0. No fake world ptr.
 selections={r['frame']:r for r in rows if r['event']=='motion_visual_selection'}
 hands={r['frame']:r for r in rows if r['event']=='motion_hand_pose'}
 raw=[r for r in rows if r['event']=='action_sequence_snapshot'];times=[r['us'] for r in raw]
 records=[];expected=[];mismatches=[];native_permissions=[]
 for r in (r for r in rows if r['event']=='motion_attack_sample'):
  idx=bisect.bisect_right(times,r['us'])-1
  if idx<0:raise ValueError('native snapshot absent')
  snap=raw[idx];b=bytes.fromhex(snap['base_actor_bytes']);active=bool(b[0x1b1]&1);trigger=struct.unpack_from('<H',b,0x18)[0]
  native_time=struct.unpack_from('<q',b,8)[0];native_sequence=struct.unpack_from('<I',b,0)[0]
  if (active,native_time,native_sequence)!=(r['active'],r['trigger_time'],r['sequence']):mismatches.append(r['frame']);continue
  selection=selections.get(r['frame']);hand=hands.get(r['frame'])
  if not selection or not hand or not selection['source_unchanged'] or selection['parent']!=r['entity'] or selection['item']!=r['item'] or (selection['child']<<32)|selection['item']!=r['binding']:raise ValueError('unqualified client equipment context')
  world=int(selection['world'],16)
  if not world:raise ValueError('unknown client world')
  now=snap['milliseconds']+(r['us']-snap['us'])//1000;receipt=now-(r['us']-hand['us'])//1000
  if not 0<=now-receipt<=200 or not 0<=now-snap['milliseconds']<=200:raise ValueError('stale archive row')
  v=poses[r['frame']-1];state=struct.unpack_from('<Q',b,0xbf8)[0]
  if active:state=(state|struct.unpack_from('<Q',b,0xbd0)[0])&~struct.unpack_from('<Q',b,0xbd8)[0]
  can_primary=bool(struct.unpack_from('<I',b,0xbc8 if active else 0xc00)[0]&1)
  # Archived fixture profile index is represented as a nonzero replay token.
  values=[now,receipt,r['frame'],r['time'],1,1,int(v[27])]
  for h in range(2):
   k=1+13*h;values.extend([*v[k:k+3],*quat(v[k+3:k+6]),15 if v[k+12] else 0,int(v[28])+1,int(v[k+11]),1,int(v[k+10])])
  values.extend([world,r['entity'],r['item'],r['binding'],snap['milliseconds'],int(r['observation_valid'] and snap['stable_read']),int(snap['entity']==snap['local_entity']),int(not(state&(1<<7))),int(r['normal_ui'] and snap['ready_for_gameplay']==1),int(can_primary),int(active),r['used_item'],r['sequence'],trigger,r['trigger_time'],r['played_time'],r['current_time'],r['length'],int(r.get('gesture_active',False) or r.get('gesture_reset',False))])
  records.append(' '.join(map(str,values)));native_permissions.append((active,can_primary))
  if r['gesture_request']:expected.append(r['frame'])
 if mismatches:raise ValueError(f'raw/status races need review; frames={mismatches}')
 out.mkdir(parents=True);(out/'native-input.tsv').write_text('\n'.join(records)+'\n')
 cmd=[str(Path('vrmod/build/bin/Release/evr_melee_session_replay.exe').resolve()),str((probe/'world-grip-preview.txt').resolve()),str((out/'native-input.tsv').resolve()),str((out/'coordinator.jsonl').resolve())]
 result=subprocess.run(cmd,capture_output=True,text=True);(out/'replay.log').write_text(result.stdout+result.stderr)
 if result.returncode:raise RuntimeError(result.stderr)
 replay=[json.loads(l) for l in (out/'coordinator.jsonl').read_text().splitlines()];fired=[r['frame'] for r in replay if r['issued']]
 checks=dict(same_physical_request_frames=fired==expected and bool(expected),all_accepted=sum(bool(r['accepted']) for r in replay)==len(expected),all_completed=sum(bool(r['retired']) for r in replay)==len(expected),no_revocations=not any(r['revoked'] for r in replay),no_requests_while_native_unavailable=len(replay)==len(native_permissions) and all(not r['issued'] for r,(active,allowed) in zip(replay,native_permissions) if active or not allowed),terminal_neutral=bool(replay) and replay[-1]['level']==replay[-1]['current']==0)
 if args.negative_tests:
  for name,column,value,after_request,expected_requests in [('reserved',49,'1',False,0),('native_denied',40,'0',False,0),('offhand_released',18,'0',False,0),('stale_native',35,None,False,0),('wrong_ack_item',42,'999',True,1),('new_origin_before_ack',5,'2',True,1)]:
   altered=[]
   for line in records:
    row=line.split()
    if not after_request or int(row[2])>expected[0]:row[column]=str(int(row[0])-201) if value is None else value
    altered.append(' '.join(row))
   data=out/(name+'.tsv');result_file=out/(name+'.jsonl');data.write_text('\n'.join(altered)+'\n')
   run=subprocess.run([cmd[0],cmd[1],str(data.resolve()),str(result_file.resolve())],capture_output=True,text=True)
   if run.returncode:raise RuntimeError(run.stderr)
   negative=[json.loads(l) for l in result_file.read_text().splitlines()]
   checks['reject_'+name]=sum(bool(r['issued']) for r in negative)==expected_requests and not any(r['accepted'] for r in negative) and sum(bool(r['revoked']) for r in negative)==expected_requests and negative[-1]['level']==0
 report=dict(checks={k:'PASS' if v else 'FAIL' for k,v in checks.items()},source=str(probe),source_sha256=hashlib.sha256((probe/'view-resource-probe.jsonl').read_bytes()).hexdigest(),rows=len(records),expected_frames=expected,observed_frames=fired,summary=result.stdout.strip(),limitation=__doc__)
 (out/'checks.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2));return int(not all(checks.values()))
if __name__=='__main__':raise SystemExit(main())
