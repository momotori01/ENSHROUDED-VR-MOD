#!/usr/bin/env python3
"""Mock hand script for the quick slot by hand motion (-QuickSlotMotion, quick_slot.hpp), from the still-hand home script.

The Mock head looks along +X, so the user's right is +Z. The wheel of the home save has the great axe at
51 degrees and the sword at 129 degrees (make_tool_select_fixture.py).
   400..469   right B held; 425..444 the right hand moves 12 cm toward the axe (right and up); B up at 470
              -> wheel opens about 250 ms in, the axe is taken on release
   480..499   hand back
   700..759   right B held, hand still        -> wheel opens and closes, nothing changes
   900..969   right B held; 925..944 the hand moves 12 cm toward the sword (left and up); B up at 970
              -> the sword is taken
   980..999   hand back
   1100..1159 right grip held                  -> nothing: the grip no longer opens the wheel
"""
import argparse, json, math, pathlib

RIGHT = (14, 15, 16)
RIGHT_GRAB, RIGHT_SECONDARY, LEFT_GRAB, LEFT_TRIGGER, RIGHT_TRIGGER, RIGHT_STICK_X, RIGHT_STICK_Y = 24, 38, 11, 10, 23, 32, 33
AXE_DEGREES, SWORD_DEGREES, REACH = 51, 129, .12
PARTS = dict(axe=(400, 469), cancel=(700, 759), sword=(900, 969), grip=(1100, 1159), stop_frame=1300)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-quick-slot-20261011.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or any(r[14:17] != ['0.38', '1.25', '0.25'] for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[LEFT_GRAB] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = r[RIGHT_SECONDARY] = '0'
    moves = {}

    def hold(column, first, last):
        for frame in range(first, last + 1):
            rows[frame - 1][column] = '1'

    def reach(first, degrees, back):
        angle = math.radians(degrees)
        step = (0.0, math.sin(angle) * REACH / 20, math.cos(angle) * REACH / 20)   # up = +Y, the user's right = +Z
        for frame in range(first, first + 20):
            moves[frame] = step
        for frame in range(back, back + 20):
            moves[frame] = tuple(-s for s in step)

    hold(RIGHT_SECONDARY, *PARTS['axe']); reach(425, AXE_DEGREES, 480)
    hold(RIGHT_SECONDARY, *PARTS['cancel'])
    hold(RIGHT_SECONDARY, *PARTS['sword']); reach(925, SWORD_DEGREES, 980)
    hold(RIGHT_GRAB, *PARTS['grip'])
    position = [0.38, 1.25, 0.25]
    for frame in range(1, 4301):
        if frame in moves:
            position = [v + s for v, s in zip(position, moves[frame])]
        for column, v in zip(RIGHT, position):
            rows[frame - 1][column] = repr(round(v, 9))
    if max(abs(u - v) for u, v in zip(position, (0.38, 1.25, 0.25))) > 1e-6:
        raise SystemExit('hand does not return')
    out = ['# Quick slot by hand motion (tools/make_quick_slot_fixture.py): right B held, the right hand points, release takes.', '# ' + json.dumps(PARTS)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
