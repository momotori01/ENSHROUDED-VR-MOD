#!/usr/bin/env python3
"""Mock hand script that looks for the building hammer's controls (home save, second action bar page).

  420..539    wheel held, right stick click 460..466: second page
  600         wheel towards the building hammer (slot 5 of the page)
  760..789    left trigger: the hammer becomes the blueprint used last (building mode, run game071211)
  850..859    right stick down: building mode is a screen of its own (run game071648), the stick keeps its raw axis
              there, which is the game's camera pitch (run game083144: 50 frames went all the way down) - the
              preview comes nearer
  then, one after the other, what might place the piece: right trigger 1000, left lower (X) 1150, right lower (A) 1300
"""
import argparse, json, math, pathlib

LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY = 35, 36, 37, 38
PRESSES = [(LEFT_TRIGGER, 760, 790), (RIGHT_TRIGGER, 1000, 1010), (LEFT_PRIMARY, 1150, 1160), (RIGHT_PRIMARY, 1300, 1310)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-build-survey-20261012.csv'))
    a = p.parse_args()
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
    for column, first, end in PRESSES:
        for f in range(first, end):
            rows[f - 1][column] = '1'
    for f in range(850, 860):          # about 2.4 degrees a frame at full deflection (run game083144): 15 -> about 39 degrees down
        rows[f - 1][RIGHT_STICK_Y] = '-1'
    meta = dict(page_swap=[420, 540], hammer=600, presses=PRESSES, stop_frame=1600)
    out = ['# Building hammer survey: second page 420, hammer 600; left trigger 760 (building mode), right stick down 850 (10 frames), right trigger 1000, left lower 1150, right lower 1300 (tools/make_build_survey_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
