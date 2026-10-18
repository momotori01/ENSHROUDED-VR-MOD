#!/usr/bin/env python3
"""Mock hand script for the held item's own entity while walking (hooks/attachment_observer.hpp: the light and the flame
of the wand and of the torch hang on that entity; it trailed the shown item by one frame of walking).

On the default (derived) home save the script starts with the bow of the first action bar page.
  600..669    wheel towards the wand (slot 5 of the page, 77 degrees; item 3851016100)
  900..1019   left stick forward
  1300..1369  wheel towards the torch (slot 4, 103 degrees; item 3474096535)
  1500..1619  left stick back (the way it came)
Stop at 1800. held_follow_state (once a second) gives the metres between the game's own model and the shown one;
held_follow_write counts the writes that were moved along with the player.

--turn: instead of walking the player turns by the right stick (to the right 900..1019 with the wand, to the left
1500..1619 with the torch) - the item's own entity then trailed the shown one by one frame of turning
(held_follow_turn counts the writes that were turned along).
"""
import argparse, json, math, pathlib

LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 29, 30, 31, 32, 33, 34
WHEEL = [(600, 77.0), (1300, 103.0)]
WALKS = [(900, 1020, '1.0'), (1500, 1620, '-1.0')]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-follow-walk-20261012.csv'))
    p.add_argument('--turn', action='store_true', help='turn by the right stick instead of walking')
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    for first, degrees in WHEEL:
        angle = math.radians(degrees)
        for f in range(first, first + 60):
            rows[f - 1][RIGHT_GRAB] = '1'
        for f in range(first + 20, first + 70):
            rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    for first, end, value in WALKS:
        for f in range(first, end):
            if a.turn:
                rows[f - 1][RIGHT_STICK_X] = value        # 1.0 turns to the right, -1.0 to the left
            else:
                rows[f - 1][LEFT_STICK_Y] = value
    meta = dict(wheel=WHEEL, walks=[[w[0], w[1]] for w in WALKS], stop_frame=1800, items=dict(wand=3851016100, torch=3474096535))
    out = ['# The held item while walking: wand 600, walk 900..1019, torch 1300, walk back 1500..1619 (tools/make_follow_walk_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
