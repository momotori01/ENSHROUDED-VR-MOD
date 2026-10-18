#!/usr/bin/env python3
"""Mock hand script for "use with the right trigger": the hand items of the home save's second action bar page that
are neither weapons nor swung (VHVR uses tools and building with the trigger).

On the default (derived) home save the script starts with the bow of the first page:
  420..539   wheel held, right stick click 460..466: second page
  then per item: wheel towards its slot (slot k of a page lies at 181 - 26 k degrees), right trigger presses
    600   building hammer (slot 5 of the page)      trigger 760..789, 900..909
    1000  watering can (slot 8)                     trigger 1160..1219
    1300  crucible, a building piece (slot 7)       trigger 1460..1489
    1600  summoning staff of the alchemist (slot 3) trigger 1760..1789
    1900  fishing rod (slot 4)                      trigger 2060..2089, 2200..2229
The fishing rod comes last: its cast keeps the game busy for a while (the wheel does not open during it).
"""
import argparse, json, math, pathlib

LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
ITEMS = [dict(name='building hammer', select=600, slot=4, triggers=[(760, 790), (900, 910)]),
         dict(name='watering can', select=1000, slot=7, triggers=[(1160, 1220)]),
         dict(name='crucible', select=1300, slot=6, triggers=[(1460, 1490)]),
         dict(name='summoning staff', select=1600, slot=2, triggers=[(1760, 1790)]),
         dict(name='fishing rod', select=1900, slot=3, triggers=[(2060, 2090), (2200, 2230)])]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-hand-use-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    for f in range(420, 540):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(460, 467):
        rows[f - 1][RIGHT_STICK_CLICK] = '1'
    for item in ITEMS:
        angle = math.radians(181 - 26 * item['slot'])
        for f in range(item['select'], item['select'] + 60):
            rows[f - 1][RIGHT_GRAB] = '1'
        for f in range(item['select'] + 20, item['select'] + 70):
            rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
        for first, end in item['triggers']:
            for f in range(first, end):
                rows[f - 1][RIGHT_TRIGGER] = '1'
    meta = dict(page_swap=[420, 540], items=ITEMS, stop_frame=2500)
    out = ['# Use with the right trigger: second page 420; hammer 600, watering can 1000, crucible 1300, summoning staff 1600, fishing rod 1900 (tools/make_hand_use_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
