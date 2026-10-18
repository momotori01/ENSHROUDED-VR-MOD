#!/usr/bin/env python3
"""Mock hand script for -LiveMelee (live_melee.hpp), from the still-hand home script.

Mock hand frames are 1/90 s apart in predicted time, so 0.05 m per frame is 4.5 m/s.
Sword (one-hand, needs more than 3 m/s):
  A 560..567   stroke across the blade at 4.5 m/s             -> press
  B 680..689   wrist turn 5 degrees per frame, hand still      -> nothing (about 1 m/s at the 12.5 cm lever)
  C 760..771   stroke, two still frames, stroke back at once   -> one press (second stroke is inside 400 ms)
  D 860..869   sweep at 2.25 m/s                               -> nothing
Axe (two-hand group, needs more than 4.5 m/s, or a 4 m/s thrust):
  E 1160..1167 stroke across at 4.23 m/s                       -> nothing
  F 1260..1267 thrust along the weapon at 4.23 m/s             -> press (thrust)
  G 1360..1367 stroke across at 4.95 m/s                       -> press
"""
import argparse, math, pathlib

RIGHT_X, RIGHT_Y, RIGHT_Z, RIGHT_YAW = 14, 15, 16, 17
RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_TRIGGER, RIGHT_TRIGGER = 24, 32, 33, 10, 23
SWORD = ('-0.6293203910498374', '0.7771459614569709')
AXE = ('0.6293203910498374', '0.7771459614569709')
WEAPON_PITCH = math.radians(-35)
# Right grip yaw -90 with the -35 degree mount pitch: the weapon's +Y axis in tracking space.
FORWARD = (math.sin(-WEAPON_PITCH), math.cos(WEAPON_PITCH), 0.0)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-live-melee-20261011.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[RIGHT_X:RIGHT_YAW + 1] != ['0.38', '1.25', '0.25', '-90'] for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = '0'
    position, yaw = [0.38, 1.25, 0.25], -90.0
    moves = {}   # frame -> (dx, dy, dz, dyaw) applied on that frame

    def move(first, frames, step, turn=0.0):
        for frame in range(first, first + frames):
            if frame in moves:
                raise SystemExit('overlapping move')
            moves[frame] = (*step, turn)

    def select(start, direction):
        for frame in range(start, start + 60):
            rows[frame - 1][RIGHT_GRAB] = '1'
        for frame in range(start + 20, start + 70):
            rows[frame - 1][RIGHT_STICK_X], rows[frame - 1][RIGHT_STICK_Y] = direction

    select(400, SWORD)
    move(560, 8, (0, 0, -.05)); move(580, 40, (0, 0, .01))                    # A, slow return
    move(680, 10, (0, 0, 0), 5.0); move(700, 50, (0, 0, 0), -1.0)              # B, slow return
    move(760, 6, (0, 0, -.05)); move(768, 4, (0, 0, .05)); move(790, 10, (0, 0, .01))   # C
    move(860, 10, (0, 0, -.025)); move(880, 25, (0, 0, .01))                   # D
    select(1000, AXE)
    move(1160, 8, (0, 0, -.047)); move(1180, 47, (0, 0, .008))                 # E
    move(1260, 8, tuple(.047 * f for f in FORWARD)); move(1280, 47, tuple(-.008 * f for f in FORWARD))   # F
    move(1360, 8, (0, 0, -.055)); move(1380, 44, (0, 0, .01))                  # G
    for frame in range(1, 4301):
        if frame in moves:
            dx, dy, dz, turn = moves[frame]
            position = [position[0] + dx, position[1] + dy, position[2] + dz]
            yaw += turn
        r = rows[frame - 1]
        r[RIGHT_X], r[RIGHT_Y], r[RIGHT_Z], r[RIGHT_YAW] = repr(round(position[0], 9)), repr(round(position[1], 9)), repr(round(position[2], 9)), repr(round(yaw, 9))
    if max(abs(a - b) for a, b in zip(position, (0.38, 1.25, 0.25))) > 1e-6 or abs(yaw + 90) > 1e-6:
        raise SystemExit(f'hand does not return: {position} {yaw}')
    out = ['# -LiveMelee regression (see tools/make_live_melee_fixture.py): sword selected at 400, axe at 1000.',
           '# Presses expected at strokes A 560, C 760, F 1260 (thrust), G 1360; none at B 680 (wrist turn), D 860 (slow), E 1160 (below the heavy requirement).']
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
