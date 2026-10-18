#!/usr/bin/env python3
"""Mock scripts (hands and head) for the water of the watering can, outdoors, for both eyes.

Headset run game-20261013-141010: "the bright reflection of the water is seen with the left eye only". Indoors the
forearm and the can cover the place where the water falls and the room is dark (game-20261013-173102), so the can is
taken outdoors:
  ..1179      home-can-look-20261013.csv: second action bar page, the watering can selected at 1100..1169
  1200..1309  left stick right, 1310..1437 forward (the walk of make_decal_outdoor_fixture.py, out to the grass)
  1500..1789  the right hand low and to the right, pointing 35 degrees down, the right trigger held: the can pours
  head        level until 1460, then 40 degrees down
Stop at 1850. Pictures: a burst of 16 from 1650, singles at 1560 and 1760.
"""
import argparse, pathlib

RIGHT, RIGHT_TRIGGER, LEFT_STICK_X, LEFT_STICK_Y = 14, 23, 29, 30
HAND = ('0.45', '1.1', '0.3', '-90', '-35', '0')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-can-look-20261013.csv'))
    p.add_argument('--head', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012-head.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-can-pour-outdoor-20261013.csv'))
    p.add_argument('--down', type=float, default=40.)
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    head = [line.split(',') for line in a.head.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) < 1850 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    rest = rows[1178]; out = []
    for frame in range(1, 1851):
        r = list(rows[frame - 1] if frame < 1180 else rest); r[0] = str(frame)
        if 1200 <= frame < 1310:
            r[LEFT_STICK_X] = '1.0'
        elif 1310 <= frame < 1438:
            r[LEFT_STICK_Y] = '1.0'
        if frame >= 1480:
            r[RIGHT:RIGHT + 6] = HAND
        if 1500 <= frame < 1790:
            r[RIGHT_TRIGGER] = '1'
        out.append(','.join(r))
    a.output.write_text('\n'.join(['# The watering can pours outdoors: selected by 1169, walk 1200..1437, trigger 1500..1789 (tools/make_can_pour_fixture.py). Stop at 1850.'] + out) + '\n', encoding='utf-8', newline='\n')
    level = head[0]; down = next(r for r in head if abs(float(r[5]) + 35.) < 1e-6)
    lines = []
    for frame in range(1, len(head) + 1):
        r = list(level if frame < 1460 else down); r[0] = str(frame)
        if frame >= 1460:
            r[5] = '%g' % -a.down
        lines.append(','.join(r))
    target = a.output.with_name(a.output.stem + '-head.csv')
    target.write_text('\n'.join(['# Head script for the watering can outdoors (tools/make_can_pour_fixture.py): %g degrees down from 1460.' % a.down] + lines) + '\n', encoding='utf-8', newline='\n')
    print(a.output, target, 'stop 1840')


if __name__ == '__main__':
    main()
