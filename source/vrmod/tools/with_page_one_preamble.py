#!/usr/bin/env python3
"""SUPERSEDED (2026-10-12): frames 100..309 come before the game accepts input (game053553: no page swap happened).
The older hand scripts run on the derived save instead (run_home_equipment_trial.ps1, default reference); a page
swap inside a script works from about frame 400 on (test_env/home-page-swap-20261012.csv).

Put a hand script made for the old home save onto the save of 2026-10-11 22:07.

That save starts on the second action bar page with the construction hammer in hand (slot 12; the page holds the
T4 bow, bombs, the summoning staff, the fishing rod, the hammer, the daggers, a crucible and the watering can).
The older scripts expect the first page (staff, bow, sword, torch, wand, great axe, pickaxe, felling axe) and some
of them the bow in hand. In frames the source leaves idle, this adds:

  100..219  right grip held (the wheel), right stick click 140..146: the page swap, as the wheel's own hint
            shows it (RB + right stick click; game053217: slot 12 -> 4);
  240..299  right grip held, right stick towards a slot of the first page 260..309: that item in hand
            (--select none leaves the item of the same slot number, the wand).

Only the right hand's grip, stick and stick click change, and only in frames 100..309.
"""
import argparse, json, math, pathlib

RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 23, 24, 32, 33, 34
SLOTS = dict(staff=181, bow=155, sword=129, torch=103, wand=77, axe=51, pickaxe=25, felling_axe=-1)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=pathlib.Path)
    p.add_argument('--select', choices=tuple(SLOTS) + ('none',), default='none')
    p.add_argument('--output', type=pathlib.Path)
    a = p.parse_args()
    lines = a.source.read_text(encoding='utf-8').splitlines()
    comments = [l for l in lines if l.startswith('#')]
    rows = [l.split(',') for l in lines if l and not l.startswith('#')]
    if any(len(r) != 39 for r in rows) or len(rows) < 400:
        raise SystemExit('unexpected hand script')
    for r in rows[89:340]:
        if any(float(r[c]) != 0 for c in (RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK)):
            raise SystemExit(f'the right hand is busy at frame {r[0]}; the preamble needs frames 90..339 idle')
    for f in range(100, 220):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(140, 147):
        rows[f - 1][RIGHT_STICK_CLICK] = '1'
    if a.select != 'none':
        angle = math.radians(SLOTS[a.select])
        for f in range(240, 300):
            rows[f - 1][RIGHT_GRAB] = '1'
        for f in range(260, 310):
            rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    output = a.output or a.source.with_name(a.source.stem + '-page1.csv')
    head = [f'# First action bar page for the save of 2026-10-11 22:07 (tools/with_page_one_preamble.py --select {a.select}): '
            'page swap 100..219, selection 240..309. Source: ' + a.source.name]
    output.write_text('\n'.join(head + comments + [','.join(r) for r in rows]) + '\n', encoding='utf-8', newline='\n')
    sidecar = a.source.with_suffix('.json')
    if sidecar.exists():
        output.with_suffix('.json').write_text(sidecar.read_text(encoding='utf-8'), encoding='utf-8', newline='\n')
    print(output)


if __name__ == '__main__':
    main()
