#!/usr/bin/env python3
"""Mock hand script for throwing (bombs of the home save's second action bar page), after VHVR ThrowableManager.

On the default (derived) home save the script starts with the bow of the first page; the page swap alone selects the
bomb (same slot of the other page):
  420..539    wheel held, left stick click 460..466: second page (--page-click right: the click before 2026-10-12 evening)
  700..744    right trigger held (wind up); the hand moves forward and up from 730 and is still moving at the release
              in 745: throw 1 (forward, upward)
  1000..1044  wind up; the hand moves forward and to the left from 1030, release in 1045: throw 2
  1300..1339  wind up with a still hand, release in 1340: nothing leaves the hand (VHVR: 6.25 cm within 0.4 s)
  1600..1655  wind up; the hand moves forward and up from 1630 for 20 frames, then sinks for 10 (the follow-through);
              release in 1656, six frames into the sinking: throw 3 has to go forward and up - along the throw's
              fastest moment (include/evr/live_throw.hpp since 2026-10-13; before, it went straight down, as in the
              headset run game-20261013-005905)
Hand positions: x forward, y up, z right; Mock hand frames are 1/90 s apart.
"""
import argparse, json, pathlib

RIGHT_X, RIGHT_Y, RIGHT_Z = 14, 15, 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
LEFT_STICK_CLICK = 31
THROWS = [dict(wind=700, move=730, release=745, step=(.03, .015, 0.0)), dict(wind=1000, move=1030, release=1045, step=(.02, 0.0, -.03)),
          dict(wind=1300, move=None, release=1340, step=(0.0, 0.0, 0.0)),
          dict(wind=1600, move=1630, release=1656, step=(.03, .015, 0.0), follow=(0.0, -.015, 0.0))]
MOVE_FRAMES, FOLLOW_FRAMES, RETURN_FRAMES = 20, 10, 80


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-throw-20261012.csv'))
    p.add_argument('--page-click', choices=('left', 'right'), default='left', help='which stick click swaps the action bar page while the wheel is held')
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_X:RIGHT_Z + 1] != ['0.38', '1.25', '0.25']:
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK, LEFT_STICK_CLICK):
            r[c] = '0'
    for f in range(420, 540):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(460, 467):
        rows[f - 1][LEFT_STICK_CLICK if a.page_click == 'left' else RIGHT_STICK_CLICK] = '1'
    moves = {}
    for t in THROWS:
        for f in range(t['wind'], t['release']):
            rows[f - 1][RIGHT_TRIGGER] = '1'
        if t['move'] is None:
            continue
        for f in range(t['move'], t['move'] + MOVE_FRAMES):
            moves[f] = t['step']
        follow = t.get('follow', (0.0, 0.0, 0.0))
        for f in range(t['move'] + MOVE_FRAMES, t['move'] + MOVE_FRAMES + FOLLOW_FRAMES):
            if 'follow' in t:
                moves[f] = follow
        back = tuple(-(s * MOVE_FRAMES + g * FOLLOW_FRAMES) / RETURN_FRAMES for s, g in zip(t['step'], follow))
        for f in range(t['move'] + MOVE_FRAMES + 30, t['move'] + MOVE_FRAMES + 30 + RETURN_FRAMES):
            moves[f] = back
    at = [0.38, 1.25, 0.25]
    for f in range(1, len(rows) + 1):
        step = moves.get(f, (0.0, 0.0, 0.0))
        at = [at[i] + step[i] for i in range(3)]
        rows[f - 1][RIGHT_X], rows[f - 1][RIGHT_Y], rows[f - 1][RIGHT_Z] = (repr(round(v, 9)) for v in at)
    if max(abs(at[0] - .38), abs(at[1] - 1.25), abs(at[2] - .25)) > 1e-6:
        raise SystemExit(f'hand does not return: {at}')
    meta = dict(page_swap=[420, 540], throws=[dict(release=t['release'], step=t['step'], thrown=t['move'] is not None, follow=t.get('follow')) for t in THROWS], stop_frame=1900)
    out = ['# Throw (bombs): second page 420; wind up 700, release 745 moving forward/up; wind up 1000, release 1045 moving forward/left; still release 1340; wind up 1600, release 1656 in the follow-through (tools/make_throw_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
