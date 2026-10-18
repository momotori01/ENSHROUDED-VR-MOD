"""Validate BaseActor effective state decoding with original 1dfa50 instructions.

Local PE in isolated Unicorn only. No imports, syscalls, game process or writes
to the executable. StateFlag indices are separate from PlayerInputType indices.
"""
import json,pathlib,struct
from emulate_camera_math import CameraMath,SHA256
from unicorn.x86_const import UC_X86_REG_RAX


def main():
    engine=CameraMath(pathlib.Path('enshrouded.exe'));engine.allowed.append((0x1dfa50,0x1dfa90))
    masks=[0,2**64-1,0xaaaaaaaaaaaaaaaa,0x5555555555555555,1<<63]
    checks=[];cases=0;raw_mismatches=0
    for sequence in (0,1,2,3,255):
        matches=unchanged=True
        for i,raw in enumerate(masks):
            add=masks[(i+1)%len(masks)];remove=masks[(i+2)%len(masks)]
            actor=bytearray(3104);actor[0x1b1]=sequence
            struct.pack_into('<Q',actor,0xbf8,raw);struct.pack_into('<Q',actor,0xbd0,add);struct.pack_into('<Q',actor,0xbd8,remove)
            expected=(raw|add)&~remove if sequence&1 else raw
            for bit in range(64):
                engine.cpu.mem_write(engine.data,bytes(actor));engine.invoke(0x1dfa50,engine.data,bit)
                actual=bool(engine.cpu.reg_read(UC_X86_REG_RAX)&255)
                matches&=actual==bool(expected&(1<<bit));unchanged&=bytes(engine.cpu.mem_read(engine.data,len(actor)))==bytes(actor)
                raw_mismatches+=actual!=bool(raw&(1<<bit));cases+=1
        checks.append(dict(name=f'sequence_{sequence}_native_state_and_read_only',status='PASS' if matches and unchanged else 'FAIL'))
    checks.append(dict(name='negative_control_raw_mask_ignores_sequence_overrides',status='PASS' if raw_mismatches>0 else 'FAIL',mismatches=raw_mismatches))
    out=dict(exe_sha256=SHA256,scope=__doc__,native_calls=cases,substitutions=engine.calls,checks=checks)
    pathlib.Path('docs/analysis/emulated_actor_state.json').write_text(json.dumps(out,indent=2),encoding='utf8')
    for c in checks:print(c['status'],c['name'])
    print('Native calls',cases);return int(any(c['status']!='PASS' for c in checks))


if __name__=='__main__':raise SystemExit(main())
