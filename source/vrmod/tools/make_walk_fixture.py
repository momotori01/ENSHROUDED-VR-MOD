#!/usr/bin/env python3
"""Mock hand script that walks the character in four directions with the left stick, hands still.

For the body heading (-BodyHeading): without it the game turns the character toward the way it walks
(backward = turned round); with it the character keeps facing where the headset looks and walks sideways
and backward as it does with a bow in hand.
   500..619   stick forward
   700..819   stick back
   900..1019  stick left
   1100..1219 stick right
The head script stays the still home pose; the stop frame is 1400.
"""
import argparse, json, pathlib

LEFT_STICK_X, LEFT_STICK_Y = 29, 30
RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, LEFT_GRAB, LEFT_TRIGGER, RIGHT_TRIGGER = 24, 32, 33, 11, 10, 23
PARTS = dict(forward=(500, 619), back=(700, 819), left=(900, 1019), right=(1100, 1219), stop_frame=1400)
STICK = dict(forward=(0, 1), back=(0, -1), left=(-1, 0), right=(1, 0))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-walk-four-ways-20261011.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        r[LEFT_TRIGGER] = r[RIGHT_TRIGGER] = r[LEFT_GRAB] = r[RIGHT_GRAB] = r[RIGHT_STICK_X] = r[RIGHT_STICK_Y] = r[LEFT_STICK_X] = r[LEFT_STICK_Y] = '0'
    for name, (x, y) in STICK.items():
        first, last = PARTS[name]
        for frame in range(first, last + 1):
            rows[frame - 1][LEFT_STICK_X], rows[frame - 1][LEFT_STICK_Y] = str(x), str(y)
    out = ['# Walks forward, back, left and right with the left stick, hands still (tools/make_walk_fixture.py).', '# ' + json.dumps(PARTS)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
