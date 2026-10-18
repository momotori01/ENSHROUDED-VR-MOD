#!/usr/bin/env python3
"""Mock scripts (hands and head) for the headset run 20261014-124515: "from the current save position turn round by
180 degrees and walk forward for a second: distant trees are shown to one eye only, here and there". The same walk
also sets the way the player goes against the heading the HUD's compass shows (hooks/compass_head.hpp).

  600..659    forward with the head as it starts (the way of the game's own camera)
  660..689    the head turns to the side by 90 degrees, level
  700..739    forward (the way of the head turned by 90 degrees)
  750..779    the head turns on by another 90 degrees: 180 from the start
  790..834    forward for about a second
Pictures of both eyes (handoff_helpers/frame_caps.sh) at the frames printed. Stop at 1100.
The rest of the hands is the first row of home-decal-outdoor-20261012.csv; the head is level throughout.
"""
import argparse, pathlib

LEFT_STICK_X, LEFT_STICK_Y = 29, 30
STOP = 1100
WALKS = ((600, 660), (700, 740), (790, 835))
TURNS = ((660, 690, -90., 0.), (750, 780, 0., 90.))
PICTURES = (650, 735, 800, 820, 835, 850, 870, 900, 940, 980, 1020, 1060)


def head_yaw(frame):
    yaw = -90.
    for start, end, a, b in TURNS:
        if frame >= end: yaw = b
        elif frame >= start: return a + (b - a) * (frame - start) / (end - start)
    return yaw


# --direct: the user's own steps - turn round at once (600..629) and walk for a second (640..684), pictures every 15
# frames from 645 to 900.
DIRECT_WALKS = ((640, 685),)
DIRECT_TURNS = ((600, 630, -90., 90.),)
DIRECT_PICTURES = tuple(range(645, 901, 15))


def main():
    global WALKS, TURNS, PICTURES
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--direct', action='store_true', help='turn round at once and walk a second (no walk before)')
    p.add_argument('--hands', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012.csv'))
    p.add_argument('--head', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-decal-outdoor-20261012-head.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/turn-back-trees-20261014.csv'))
    a = p.parse_args()
    if a.direct:
        WALKS, TURNS, PICTURES = DIRECT_WALKS, DIRECT_TURNS, DIRECT_PICTURES
        if a.output.name == 'turn-back-trees-20261014.csv': a.output = a.output.with_name('turn-back-direct-20261014.csv')
    target = a.output.with_name(a.output.stem + '-head.csv')
    for path in (a.output, target):
        if path.exists(): raise SystemExit('%s exists already' % path)
    hands = [line.split(',') for line in a.hands.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    head = [line.split(',') for line in a.head.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if any(len(r) != 39 for r in hands[:5]) or head[0][4] != '-90' or hands[0][LEFT_STICK_Y] != '0':
        raise SystemExit('unexpected source scripts')
    rest = hands[0]; rows = []
    for frame in range(1, STOP + 20):
        r = list(rest); r[0] = str(frame)
        if any(s <= frame < e for s, e in WALKS): r[LEFT_STICK_X] = '0'; r[LEFT_STICK_Y] = '1.0'
        rows.append(','.join(r))
    a.output.write_text('\n'.join(['# Turn round and walk (tools/make_turn_back_fixture.py): walks %s, head turns %s. Stop at %d.' % (list(WALKS), list(TURNS), STOP)] + rows) + '\n', encoding='utf-8', newline='\n')
    level = list(head[0]); level[5] = '0'; out = []
    for frame in range(1, STOP + 20):
        r = list(level); r[0] = str(frame); r[4] = '%g' % head_yaw(frame); out.append(','.join(r))
    target.write_text('\n'.join(['# Head script of the turn round (tools/make_turn_back_fixture.py): level, turns %s.' % (list(TURNS),)] + out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, target, 'stop', STOP)
    print('pictures', ' '.join(str(f) for f in PICTURES))


if __name__ == '__main__':
    main()
