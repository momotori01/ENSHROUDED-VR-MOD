#!/usr/bin/env python3
"""Mock hand script for building mode with the VHVR-style controls (controller_ui_fixture.hpp, screen 28).

On the default (derived) home save the script starts with the bow of the first action bar page:
  420..539    wheel held, right stick click 460..466: second page
  600         wheel towards the building hammer (slot 5 of the page)
  760..789    left trigger: building mode
  850..859    right stick down (no grip): the game's camera pitch, the preview comes to the floor in front
  950..1069   right grip held (the modifier): right stick up 970..979 (farther), right 1020..1029 (turn by 90 degrees)
  1150..1249  upper right button held: the piece menu ("other shapes"; open only while held, run game095858); the
              right stick chooses in it: right 1180..1189 and 1200..1209 (two shapes on), down 1220..1229 (next category)
  1350..1359  right trigger: build (the home save has no blocks: the game greys the button out, only the press is seen)
  1450..1479  left trigger: leave building mode
"""
import argparse, json, math, pathlib

LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY = 35, 36, 37, 38
HOLDS = [(LEFT_TRIGGER, 760, 790), (RIGHT_GRAB, 950, 1070), (RIGHT_SECONDARY, 1150, 1250),
         (RIGHT_TRIGGER, 1350, 1360), (LEFT_TRIGGER, 1450, 1480)]
STICK = [(850, 860, 0, -1), (970, 980, 0, 1), (1020, 1030, 1, 0), (1180, 1190, 1, 0), (1200, 1210, 1, 0), (1220, 1230, 0, -1)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-build-mode-20261012.csv'))
    p.add_argument('--menu-hand', choices=('right', 'left'), default='right', help='whose upper button holds the piece menu open 1150..1249 (2026-10-13: the left one, so that the right thumb is free for the stick)')
    a = p.parse_args()
    holds = [(LEFT_SECONDARY if a.menu_hand == 'left' and column == RIGHT_SECONDARY else column, first, end) for column, first, end in HOLDS]
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK, LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY):
            r[c] = '0'
    for f in range(420, 540):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(460, 467):
        rows[f - 1][RIGHT_STICK_CLICK] = '1'
    angle = math.radians(181 - 26 * 4)
    for f in range(600, 660):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    for column, first, end in holds:
        for f in range(first, end):
            rows[f - 1][column] = '1'
    for first, end, x, y in STICK:
        for f in range(first, end):
            rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = str(x), str(y)
    meta = dict(page_swap=[420, 540], hammer=600, holds=holds, menu_hand=a.menu_hand, stick=STICK, stop_frame=1600)
    out = ['# Building mode: hammer 600, left trigger 760 (enter), stick down 850, grip 950..1069 with stick up 970 and right 1020, upper right button held 1150..1249 with stick right 1180 and 1200 and down 1220, right trigger 1350, left trigger 1450 (tools/make_build_mode_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
