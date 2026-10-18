#!/usr/bin/env python3
"""Mock hand script for the HUD auto-hide (-UiAutoHide, include/evr/hud_fade.hpp): long still stretches, between them
the things that have to bring the HUD back.

On the default (derived) home save the script starts with the bow of the first action bar page:
  ..699        nothing: the HUD is shown at the start and fades out five seconds later
  700..769     wheel towards the fifth slot (77 degrees): the wand - the quick wheel and the changed action bar show it
  770..1299    nothing: it fades out again
  1300..1349   two shots of the wand (right trigger 1300..1309, 1340..1349): mana changes
  1350..       nothing: shown while the mana comes back, then it fades out
"""
import argparse, json, math, pathlib

LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
WAND = 181 - 26 * 4


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ui-hide-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    angle = math.radians(WAND)
    for f in range(700, 760):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(720, 770):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    for first in (1300, 1340):
        for f in range(first, first + 10):
            rows[f - 1][RIGHT_TRIGGER] = '1'
    meta = dict(wheel=[700, 770], shots=[1300, 1340], stop_frame=2000)
    out = ['# HUD auto-hide: still until 700, wand by the wheel 700..769, still, two wand shots 1300 and 1340, still (tools/make_ui_hide_fixture.py).', '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
