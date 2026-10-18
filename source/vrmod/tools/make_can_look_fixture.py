#!/usr/bin/env python3
"""Mock hand script for the look of the watering can in the hand (include/evr/weapon_mount.hpp mountHangingTool).

Until 1179 it is home-can-natural-grip-20261013.csv (tools/make_feedback_view_fixture.py): second action bar page,
a bomb, and the watering can selected at 1100..1169. Then the right hand comes up in front of the eyes, 0.55 m ahead
and 0.05 m under them, and is held in three ways (the grip's angles are yaw, pitch, roll in degrees):
  1200..1399  level, the thumb up (-90, 0, 0): the can hangs under the fist, the spout ahead
  1400..1599  level, the back of the hand up (roll -90): the can has to hang the same way
  1600..1799  pointing 35 degrees down, the thumb up: the spout goes down with the hand (pouring), the body swings back
Pictures at 1300, 1500 and 1700; stop at 1850.
"""
import argparse, pathlib

RIGHT = 14          # x, y, z, then the grip's yaw, pitch, roll
PLACE = (.55, 1.6, -.05)   # a little left of the middle: the forearm, coming from the right, leaves the can in sight
HOLDS = [(1200, 1400, ('-90', '0', '0')), (1400, 1600, ('-90', '0', '-90')), (1600, 1800, ('-90', '-35', '0'))]


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-can-natural-grip-20261013.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-can-look-20261013.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) < 1900 or any(len(r) != 39 for r in rows) or rows[1178][RIGHT:RIGHT + 6] != ['0.38', '1.25', '0.25', '-90', '0', '0']:
        raise SystemExit('unexpected source script')
    rows = rows[:1900]
    rest = [float(v) for v in rows[1178][RIGHT:RIGHT + 3]]
    for frame in range(1180, 1901):
        r = rows[frame - 1]
        t = min(1., (frame - 1179) / 20.)                    # the hand moves there in 20 frames
        r[RIGHT:RIGHT + 3] = ['%g' % round(rest[i] + (PLACE[i] - rest[i]) * t, 6) for i in range(3)]
        r[RIGHT + 3:RIGHT + 6] = ['-90', '0', '0']
        for first, end, angles in HOLDS:
            if first <= frame < end:
                r[RIGHT + 3:RIGHT + 6] = list(angles)
        for column in (23, 24, 32, 33, 34):                  # no trigger, grip, stick or click after the selection
            r[column] = '0'
    a.output.write_text('\n'.join(['# The watering can in the hand: level 1200, back of the hand up 1400, 35 degrees down 1600 (tools/make_can_look_fixture.py). Stop at 1850; pictures 1300, 1500, 1700.'] + [','.join(r) for r in rows]) + '\n', encoding='utf-8', newline='\n')
    print(a.output, len(rows), 'stop 1850, pictures 1300,1500,1700')


if __name__ == '__main__':
    main()
