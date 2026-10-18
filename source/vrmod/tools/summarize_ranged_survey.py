"""Summarise the read-only native ranged-weapon survey of one Mock probe.

Prints, per native World, every change of the local ranged weapon's item/state/queued
flag and the ranged input bits (Arm 47, Shoot 48, Tap 49, Mainhand 0..3), then every
Crosshair spawn sample. Reading aid only; it decides nothing.
"""
import argparse, json, struct
from pathlib import Path

BITS = {0: 'Main', 1: 'MainTap', 2: 'MainHold', 3: 'MainRelease', 28: 'Block', 47: 'Arm', 48: 'Shoot', 49: 'ShootTap'}


def main():
    a = argparse.ArgumentParser(description=__doc__)
    a.add_argument('probe', type=Path)
    v = a.parse_args()
    worlds, spawns, pads, others = {}, [], [], []
    for line in (v.probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8').splitlines():
        if '"input_bow_sample"' in line:
            e = json.loads(line)
            b = bytes.fromhex(e['weapon_bytes'])
            key = (struct.unpack_from('<I', b, 0x4c)[0], b[0], b[0x39], tuple(n for bit, n in BITS.items() if e['digital'] >> bit & 1))
            rows = worlds.setdefault(e['world'], [])
            if not rows or rows[-1][1] != key:
                rows.append((e['frame'], key, struct.unpack_from('<I', b, 0x60)[0]))
        elif '"bow_spawn_sample"' in line:
            e = json.loads(line)
            b = bytes.fromhex(e['native_weapon_bytes'])
            evt = bytes.fromhex(e['event_bytes'])
            spawns.append(dict(frame=e['last_xr_frame'], item=struct.unpack_from('<I', b, 0x4c)[0], state=b[0], kind=e['spawn_kind'],
                               projectile=e['is_projectile'], tracked=e['tracked_aim'], result=e['result'],
                               local_offset=[round(x, 4) for x in struct.unpack_from('<3f', evt, 0xc0)]))
        elif '"crosshair_survey_sample"' in line:
            e = json.loads(line)
            evt = bytes.fromhex(e['event_bytes'])
            out = bytes.fromhex(e['transform_bytes'])
            others.append(dict(frame=e['last_xr_frame'], parent=e['parent_caller'], mode=e['mode'], item=e['item'], state=e['weapon_state'],
                               result=e['result'], kind=evt[0x90], projectile=evt[0x148],
                               local_offset=[round(x, 4) for x in struct.unpack_from('<3f', evt, 0xc0)],
                               world_offset=[round(x, 4) for x in struct.unpack_from('<3f', evt, 0xcc)],
                               orientation_offset=[round(x, 5) for x in struct.unpack_from('<4f', evt, 0xd8)],
                               spread=[round(x, 4) for x in struct.unpack_from('<8f', evt, 0x104)],
                               camera_distance=round(struct.unpack_from('<f', bytes.fromhex(e['camera_bytes']), 40)[0], 4),
                               out_quat=[round(x, 5) for x in struct.unpack_from('<4f', out, 24)], head=evt[:16].hex()))
        elif '"controller_live_sample"' in line:
            e = json.loads(line)
            state = (e['triggers'], bool(e['buttons'] & 0x4000))
            if not pads or pads[-1][1] != state:
                pads.append((e['frame'], state))
    print('pad ([LT,RT], X) changes:', pads)
    print('other Crosshair events:', len(others))
    for o in others:
        print(' ', o)
    for world, rows in worlds.items():
        print('world', world)
        for frame, (item, state, queued, bits), ammo in rows:
            print(f'  frame {frame:5d} item {item:10d} state {state} queued {queued} ammo {ammo:10d} input {",".join(bits) or "-"}')
    print('spawn samples:', len(spawns))
    for s in spawns:
        print(' ', s)


if __name__ == '__main__':
    main()
