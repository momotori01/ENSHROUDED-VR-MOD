#!/usr/bin/env python3
"""Mock scripts (hands and head) for the grass that an arrow pushes aside, for both eyes.

Headset runs of 2026-10-13: "the weeds that sway after an arrow has passed do so in the left eye only". The walk out
of the home is the one of make_decal_outdoor_fixture.py (right until the wall, then forward through the door to the
grass); the bow of the first action bar page stays in the hand (no page swap); then the three shots of
make_bow_draw_fixture.py, 500 frames later than there.
  600..709    left stick right, 710..837 forward (out to the grass)
  900..       the bow script from its frame 400 on: releases at 1220, 1510 and 1730
  head        level until 880, then 25 degrees down (the grass under the arrow's way)
Stop at 1900. Pictures: a burst of 32 from 1222 (the first arrow is on its way).
"""
import argparse, pathlib

SHIFT = 500


def rows_of(path, width):
    rows = [line.split(',') for line in path.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if not rows or any(len(r) != width for r in rows) or [int(r[0]) for r in rows[:5]] != [1, 2, 3, 4, 5]:
        raise SystemExit('unexpected script ' + str(path))
    return rows


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--walk', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012.csv'))
    p.add_argument('--bow', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-bow-draw-three-shots-20261010.csv'))
    p.add_argument('--head', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012-head.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-grass-arrow-20261013.csv'))
    p.add_argument('--frames', type=int, default=1900)
    p.add_argument('--down', type=float, default=25.)
    a = p.parse_args()
    walk, bow = rows_of(a.walk, 39), rows_of(a.bow, 39)
    head = [line.split(',') for line in a.head.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    rest = walk[0]
    out = []
    for frame in range(1, a.frames + 1):
        if frame < 900:
            r = list(rest)
            # Only the walk is taken from that script: the left stick.
            r[29:31] = walk[frame - 1][29:31]
        else:
            r = list(bow[frame - SHIFT - 1])
        r[0] = str(frame); out.append(','.join(r))
    a.output.write_text('\n'.join(['# Grass pushed by an arrow: walk 600..837, bow releases 1220, 1510, 1730 (tools/make_grass_arrow_fixture.py). Stop at %d.' % a.frames] + out) + '\n', encoding='utf-8', newline='\n')
    # The head: the first row (level) until 880, then the row that looks 35 degrees down with the pitch asked for.
    level = head[0]; down = next(r for r in head if abs(float(r[5]) + 35.) < 1e-6)
    rows = []
    for frame in range(1, len(head) + 1):
        r = list(level if frame < 880 else down); r[0] = str(frame)
        if frame >= 880:
            r[5] = '%g' % -a.down
        rows.append(','.join(r))
    target = a.output.with_name(a.output.stem + '-head.csv')
    target.write_text('\n'.join(['# Head script for the grass under an arrow (tools/make_grass_arrow_fixture.py): %g degrees down from 880.' % a.down] + rows) + '\n', encoding='utf-8', newline='\n')
    print(a.output, target, 'stop', a.frames - 10)


if __name__ == '__main__':
    main()
