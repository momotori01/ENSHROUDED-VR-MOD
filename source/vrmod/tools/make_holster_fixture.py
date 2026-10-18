#!/usr/bin/env python3
"""Mock hand script for the holsters (include/evr/holster.hpp): a grip pressed at a shoulder or at the left hip.

The head of the Mock stands at (0, 1.65, 0) and looks along +X, 15 degrees down: ahead is +X, the right is +Z.
  right shoulder  0.08 m behind the head, 0.05 m under it, 0.20 m to the right
  left shoulder   the same to the left
  left hip        0.25 m behind, 0.50 m under, 0.25 m to the left
A press: the hand goes there in 20 frames with its grip let go, the grip is down for 20 frames from the frame given,
the hand goes back.

  --plan <frame>:<place>,...   places: rs (right shoulder, right grip), ls (left shoulder, left grip), lh (left hip, left grip),
                               lf / rf (the left / right grip pressed in front of the body, at rest: no holster)
The default plan begins with the test save's bow in the hand (slot 2 of the action bar):
  480 rs  the bow off (bare hands)        600 rs  on again
  720 ls  a bow in the hand: bare hands   840 ls  the ranged weapon of the equipment
  960 lh  the shield off                  1080 lh on again
  1200 ls bare hands                      1320 rs the remembered slot again
  1440 lf nothing
Stop at 1560.
"""
import argparse, pathlib

LEFT, RIGHT = 1, 14            # first column of a hand: x, y, z, yaw, pitch, roll, aim yaw, pitch, roll, trigger, grip, active, tracked
PLACES = {'rs': (RIGHT, (-.08, 1.60, .20)), 'ls': (LEFT, (-.08, 1.60, -.20)), 'lh': (LEFT, (-.25, 1.15, -.25)), 'lf': (LEFT, None), 'rf': (RIGHT, None)}
TRAVEL, HELD = 20, 20


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-idle-hands-20261013.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-holsters-20261013.csv'))
    p.add_argument('--plan', default='480:rs,600:rs,720:ls,840:ls,960:lh,1080:lh,1200:ls,1320:rs,1440:lf')
    p.add_argument('--frames', type=int, default=0)
    p.add_argument('--follow', type=int, default=-1, help='the hands follow the source script, begun this many frames later (default: the first row of the source throughout)')
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if not rows or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    rest = rows[0]
    plan = [(int(f), place) for f, place in (step.split(':') for step in a.plan.split(','))]
    if any(place not in PLACES for _, place in plan) or any(b[0] - a_[0] < 2 * TRAVEL + HELD + 10 for a_, b in zip(plan, plan[1:])) or plan[0][0] < 420 + TRAVEL:
        raise SystemExit('plan: unknown place, steps closer than %d frames, or a first step before %d' % (2 * TRAVEL + HELD + 10, 420 + TRAVEL))
    frames = a.frames or plan[-1][0] + 120
    out = []
    for frame in range(1, frames + 1):
        r = list(rows[min(max(frame - a.follow, 1), len(rows)) - 1] if a.follow >= 0 else rest); r[0] = str(frame)
        for at, place in plan:
            first, target = PLACES[place]
            if not at - TRAVEL <= frame < at + HELD + TRAVEL:
                continue
            if target:
                home = [float(v) for v in r[first:first + 3]]
                share = (frame - (at - TRAVEL) + 1) / TRAVEL if frame < at else 1. if frame < at + HELD else 1 - (frame - (at + HELD) + 1) / TRAVEL
                share = max(0., min(1., share))
                r[first:first + 3] = ['%.4f' % (h + (t - h) * share) for h, t in zip(home, target)]
            # Let go on the way there (a source script may hold the grip throughout), down for the press.
            r[first + 10] = '1' if at <= frame < at + HELD else '0' if frame < at else r[first + 10]
        out.append(','.join(r))
    note = '# Holsters: ' + ', '.join('%d %s' % step for step in plan) + ' (tools/make_holster_fixture.py). Stop at %d.' % frames
    a.output.write_text('\n'.join([note] + out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, len(out), 'stop', frames - 10)


if __name__ == '__main__':
    main()
