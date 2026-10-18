#!/usr/bin/env python3
"""Mock scripts (hands and head) for the grass that the player's own body pushes aside, for both eyes.

The grass around the player is pushed by the player's own effector (keen::ecs::GrassEffector) as it is by an arrow;
standing and walking in tall grass with the head turned down shows it without anything else moving in the picture.
The walk out of the home is the one of make_decal_outdoor_fixture.py (right until the wall, then forward through the
door to the grass); the hands stay at rest, nothing is pressed.
  600..709    left stick right, 710..837 forward (out to the grass)
  900..999    forward again, into the grass
  head        level until 880, then 70 degrees down (the grass at the feet)
Stop at 1300. Pictures: a burst of 32 from 920 (walking), singles at 1060 and 1200 (standing).
"""
import argparse, pathlib

LEFT_STICK_X, LEFT_STICK_Y = 29, 30


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--walk', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012.csv'))
    p.add_argument('--head', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012-head.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-grass-walk-20261013.csv'))
    p.add_argument('--frames', type=int, default=1300)
    p.add_argument('--down', type=float, default=70.)
    a = p.parse_args()
    walk = [line.split(',') for line in a.walk.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    head = [line.split(',') for line in a.head.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if any(len(r) != 39 for r in walk) or walk[757][LEFT_STICK_Y] != '1.0':
        raise SystemExit('unexpected walk script')
    rest = walk[0]; out = []
    for frame in range(1, a.frames + 1):
        r = list(rest); r[0] = str(frame)
        if frame < 900:
            r[LEFT_STICK_X:LEFT_STICK_Y + 1] = walk[frame - 1][LEFT_STICK_X:LEFT_STICK_Y + 1]
        elif frame < 1000:
            r[LEFT_STICK_Y] = '1.0'
        out.append(','.join(r))
    a.output.write_text('\n'.join(['# Grass pushed by the player: walk 600..837 and 900..999, nothing pressed (tools/make_grass_walk_fixture.py). Stop at %d.' % a.frames] + out) + '\n', encoding='utf-8', newline='\n')
    level = head[0]; down = next(r for r in head if abs(float(r[5]) + 55.) < 1e-6)
    rows = []
    for frame in range(1, len(head) + 1):
        r = list(level if frame < 880 else down); r[0] = str(frame)
        if frame >= 880:
            r[5] = '%g' % -a.down
        rows.append(','.join(r))
    target = a.output.with_name(a.output.stem + '-head.csv')
    target.write_text('\n'.join(['# Head script for the grass at the feet (tools/make_grass_walk_fixture.py): %g degrees down from 880.' % a.down] + rows) + '\n', encoding='utf-8', newline='\n')
    print(a.output, target, 'stop', a.frames - 10)


if __name__ == '__main__':
    main()
