#!/usr/bin/env python3
"""Mock hand script for -LiveGuard (live_guard.hpp) with -LiveMelee, from the still-hand home script.

The Mock head is at (0, 1.65, 0) looking along +X. The left hand starts 40 cm below the head
and 38 cm ahead with the back of the hand pointing sideways (yaw -90). Sword and shield.
  A  560..589  turn the back of the hand to the front (yaw -90 -> -180)  -> block held
  B  700..729  turn it back                                             -> released
  C  800..839  lower the hand 40 cm; from 850 the back of the hand faces front again -> nothing (arm hangs)
  D  900..915  raise it at 2.25 m/s                                     -> block, and that press is a parry press
  E 1000..1003 push the raised shield forward at 2.7 m/s                -> released about 70 ms and pressed again
  F 1100..1107 right hand sword stroke at 4.5 m/s                       -> attack press; block released while it is down
  G 1200..1229 turn the back of the hand away                           -> released
"""
import argparse, pathlib

LEFT_X, LEFT_Y, LEFT_YAW, RIGHT_Z = 1, 2, 4, 16
RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_TRIGGER, RIGHT_TRIGGER = 24, 32, 33, 10, 23
SWORD = ('-0.6293203910498374', '0.7771459614569709')


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-live-guard-20261011.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[1:5] != ['0.38', '1.25', '-0.25', '-90'] or r[14:17] != ['0.38', '1.25', '0.25'] for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = '0'
    for frame in range(400, 460):
        rows[frame - 1][RIGHT_GRAB] = '1'
    for frame in range(420, 470):
        rows[frame - 1][RIGHT_STICK_X], rows[frame - 1][RIGHT_STICK_Y] = SWORD
    state = {LEFT_X: 0.38, LEFT_Y: 1.25, LEFT_YAW: -90.0, RIGHT_Z: 0.25}
    moves = {}

    def move(first, frames, column, step):
        for frame in range(first, first + frames):
            moves.setdefault(frame, []).append((column, step))

    move(560, 30, LEFT_YAW, -3.0)                                   # A
    move(700, 30, LEFT_YAW, 3.0)                                    # B
    move(800, 40, LEFT_Y, -.01); move(850, 1, LEFT_YAW, -90.0)       # C
    move(900, 16, LEFT_Y, .025)                                     # D
    move(1000, 4, LEFT_X, .03); move(1010, 12, LEFT_X, -.01)         # E
    move(1100, 8, RIGHT_Z, -.05); move(1120, 40, RIGHT_Z, .01)       # F
    move(1200, 30, LEFT_YAW, 3.0)                                   # G
    for frame in range(1, 4301):
        for column, step in moves.get(frame, []):
            state[column] += step
        for column, value in state.items():
            rows[frame - 1][column] = repr(round(value, 9))
    if abs(state[LEFT_X] - .38) > 1e-6 or abs(state[LEFT_Y] - 1.25) > 1e-6 or abs(state[LEFT_YAW] + 90) > 1e-6 or abs(state[RIGHT_Z] - .25) > 1e-6:
        raise SystemExit(f'hands do not return: {state}')
    out = ['# -LiveGuard with -LiveMelee regression (see tools/make_live_guard_fixture.py): sword and shield selected at 400.',
           '# Block expected from about 571 to 723, from about 910 (parry press), re-pressed after the push at 1000 and after the stroke at 1100, released about 1223.']
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
