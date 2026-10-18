"""Original CanTriggerPrimary/CanBlock ability checks on synthetic and archived actors.
Read-only local PE in isolated Unicorn. No game process, native state mutation or proof of simultaneous actions.
"""
import argparse,json,struct
from pathlib import Path
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX

def expected(blob,bit):
 active=bool(blob[0x1b1]&1);scope=struct.unpack_from('<I',blob,0xbc8)[0];raw=struct.unpack_from('<I',blob,0xc00)[0]
 # The native routine intersects three special abilities while a sequence is
 # active. Other abilities use that sequence's mask. IDs are AbilityFlag IDs,
 # never StateFlag, PlayerInputType or ActionSequenceTrigger IDs.
 selected=(raw&scope if bit in (6,9,13) else scope) if active else raw
 return bool(selected&(1<<bit))

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('probe',type=Path);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
 engine=CameraMath(Path('enshrouded.exe'));engine.allowed=[(0x1dd570,0x1dd5cf)];checks={};calls=0
 def run(blob,bit):
  nonlocal calls
  engine.cpu.mem_write(engine.data,blob);engine.invoke(0x1dd570,engine.data,bit);calls+=1
  actual=bool(engine.cpu.reg_read(UC_X86_REG_RAX)&255)
  if bytes(engine.cpu.mem_read(engine.data,len(blob)))!=blob:raise ValueError('native ability getter wrote actor')
  if actual!=expected(blob,bit):raise ValueError('native ability getter differs from independent mask decode')
  return actual
 masks=[0,0xffffffff,0xaaaaaaaa,0x55555555,0x2240]
 for state in (0,1,2,3,255):
  for raw in masks:
   for scope in masks:
    b=bytearray(3104);b[0x1b1]=state;struct.pack_into('<I',b,0xbc8,scope);struct.pack_into('<I',b,0xc00,raw)
    for bit in range(28):run(bytes(b),bit)
 checks['all_native_ability_bits_and_sequence_modes_read_only']='PASS'
 rows=[json.loads(l) for l in (a.probe/'view-resource-probe.jsonl').read_text().splitlines()];snapshots=[r for r in rows if r['event']=='action_sequence_snapshot'];summary=[]
 for s in snapshots:
  b=bytes.fromhex(s['base_actor_bytes'])
  if len(b)!=3104 or not s['stable_read'] or not s['read_only'] or s['entity']!=s['local_entity']:raise ValueError('invalid archived local actor proof')
  primary=run(b,0);block=run(b,6);evade=run(b,5)
  summary.append(dict(us=s['us'],sample=s['sample'],sequence_active=bool(b[0x1b1]&1),primary=primary,block=block,evade=evade,digital=s['input_digital']))
 checks['archived_samples_present']='PASS' if snapshots else 'FAIL'
 checks['native_helpers_not_substituted']='PASS' if not engine.calls else 'FAIL'
 groups={}
 for s in summary:
  key='active' if s['sequence_active'] else 'idle';group=groups.setdefault(key,dict(samples=0,primary_allowed=0,block_allowed=0,both_allowed=0,evade_allowed=0))
  group['samples']+=1;group['primary_allowed']+=s['primary'];group['block_allowed']+=s['block'];group['both_allowed']+=s['primary'] and s['block'];group['evade_allowed']+=s['evade']
 report=dict(exe_sha256=SHA256,scope=__doc__,probe=str(a.probe),checks=checks,native_calls=calls,groups=groups,samples=summary,
 limitation='Ability availability is not a command or a parry window. These samples are one axe attack in empty space; native input arbitration, target selection and shield/enemy behavior remain unverified.')
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=2)+'\n')
 print(json.dumps(dict(checks=checks,native_calls=calls,groups=groups),indent=2));return any(v!='PASS' for v in checks.values())
if __name__=='__main__':raise SystemExit(main())
