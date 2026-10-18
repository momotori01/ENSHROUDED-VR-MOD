#!/usr/bin/env python3
"""Mock hand script for VHVR's secondary attack by motion (include/evr/live_secondary.hpp) with the one-hand sword of
the home save: a strong thrust, then an ordinary long stroke.

Run with -LiveMeleeTravel 1 (the stroke attacks after a metre above the swing speed - in the Mock room there is no
target that would delay the attack until the blade arrives, as VHVR's contact does) and -LiveMeleeHeavyHoldMs <ms>
(a stroke judged as the secondary attack holds the attack button that long).

On the default (derived) home save the script starts with the bow of the first action bar page:
  600..669   wheel towards the third slot (129 degrees): the sword (item 1125155603; its first attack is sequence
             2496979158, its heavy attack on the ground 2522099871 - docs/analysis/weapon_mount_20261010/item-sequences)
  800..825   a thrust along the blade: 26 frames of 0.045 m (4.05 m/s at 90 hand frames a second, 1.17 m). The blade
             points along (0.574, 0.819, 0) in tracking space with the script's level grip (run game081841).
             Expected: live_melee_press with secondary / strong_thrust true.
  1100..1123 an ordinary stroke across: 24 frames of 0.05 m along -Z (4.5 m/s, 1.2 m). Expected: secondary false.
Each is followed by a slow return.
"""
import argparse, json, math, pathlib

RIGHT_X, RIGHT_Y, RIGHT_Z = 14, 15, 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
SWORD = 129.0
BLADE = (0.57357657, 0.819152, 0.0)
MOVES = [(800, 26, tuple(.045 * c for c in BLADE)), (1100, 24, (0.0, 0.0, -.05))]   # first frame, frames, metres a frame


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-heavy-attack-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or (rows[0][RIGHT_X], rows[0][RIGHT_Y], rows[0][RIGHT_Z]) != ('0.38', '1.25', '0.25'):
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    angle = math.radians(SWORD)
    for f in range(600, 660):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    step = {}
    for first, frames, delta in MOVES:
        for f in range(first, first + frames):
            step[f] = delta
        back = tuple(-c * frames / 120 for c in delta)          # the return: 120 frames, far below the swing speed
        for f in range(first + frames + 60, first + frames + 180):
            step[f] = back
    position = [0.38, 1.25, 0.25]
    for f in range(1, len(rows) + 1):
        d = step.get(f)
        if d:
            position = [position[i] + d[i] for i in range(3)]
        rows[f - 1][RIGHT_X], rows[f - 1][RIGHT_Y], rows[f - 1][RIGHT_Z] = (repr(round(c, 6)) for c in position)
    if max(abs(position[i] - (0.38, 1.25, 0.25)[i]) for i in range(3)) > 1e-4:
        raise SystemExit('the hand does not end where it started')
    meta = dict(select=600, moves=[dict(first=m[0], frames=m[1], metres_per_frame=m[2]) for m in MOVES], stop_frame=1500,
                sword=dict(item=1125155603, primary=2496979158, heavy_grounded=2522099871))
    out = ['# Secondary attack by motion with the sword: strong thrust 800, ordinary long stroke 1100 (tools/make_heavy_attack_fixture.py).', '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
