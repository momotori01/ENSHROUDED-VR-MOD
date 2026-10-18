#!/usr/bin/env python3
"""Mock hand script that selects one quick wheel slot of the home save and then keeps still.

The wheel of the home save puts its eight slots 26 degrees apart: staff 181, bow 155, sword 129,
torch 103, wand 77, great axe 51, pickaxe 25, stone felling axe -1 degrees (game202617; the names
'fourth' and 'hammer' below are from before the items were identified). Used to see what the
game does with a tool in hand (headset run game100334: the pickaxe switched the view to third person).
"""
import argparse, math, pathlib

RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_GRAB, LEFT_TRIGGER, RIGHT_TRIGGER = 24, 32, 33, 11, 10, 23
SLOTS = dict(staff=181, bow=155, sword=129, fourth=103, wand=77, axe=51, pickaxe=25, hammer=-1)


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--slot', choices=tuple(SLOTS), default='pickaxe')
    p.add_argument('--then', choices=tuple(SLOTS), default='sword', help='second selection at frame 900')
    p.add_argument('--output', type=pathlib.Path)
    a = p.parse_args()
    output = a.output or pathlib.Path(f'vrmod/test_env/home-select-{a.slot}-20261011.csv')
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[LEFT_GRAB] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = '0'

    def select(start, slot):
        angle = math.radians(SLOTS[slot])
        for frame in range(start, start + 60):
            rows[frame - 1][RIGHT_GRAB] = '1'
        for frame in range(start + 20, start + 70):
            rows[frame - 1][RIGHT_STICK_X], rows[frame - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))

    select(400, a.slot)
    select(900, a.then)
    out = [f'# Quick wheel: {a.slot} at 400, {a.then} at 900, hands still (tools/make_tool_select_fixture.py).']
    out += [','.join(r) for r in rows]
    output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(output)


if __name__ == '__main__':
    main()
