#!/usr/bin/env python3
"""Mock hand script for the heavy attack by "right trigger held while swinging" (user decision 2026-10-12) with the
one-hand sword of the home save, and for what the game's own RT does with a melee weapon.

On the default (derived) home save the script starts with the bow of the first action bar page:
  600..669    wheel towards the third slot (129 degrees): the sword (item 1125155603; first attack sequence
              2496979158, heavy attack on the ground 2522099871 - docs/analysis/weapon_mount_20261010/item-sequences)
  800..889    right trigger held, the hand still: does the game's RT start anything by itself with a sword?
  1100..1107  a stroke across (8 frames of 0.05 m along -Z, 4.5 m/s) with the right trigger held 1060..1199:
              expected the heavy attack
  1500..1507  the same stroke without the trigger: expected the first attack
  1800..1807  the same stroke, the trigger pressed only after it (1830..1859): expected the first attack
Each stroke is followed by a slow return.
"""
import argparse, json, math, pathlib

RIGHT_Z = 16
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 32, 33, 34
SWORD = 129.0
STROKES = [1100, 1500, 1800]
TRIGGERS = [(800, 890), (1060, 1200), (1830, 1860)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-trigger-heavy-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_Z] != '0.25':
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'
    angle = math.radians(SWORD)
    for f in range(600, 660):
        rows[f - 1][RIGHT_GRAB] = '1'
    for f in range(620, 670):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    for first, end in TRIGGERS:
        for f in range(first, end):
            rows[f - 1][RIGHT_TRIGGER] = '1'
    moves = {}
    for first in STROKES:
        for f in range(first, first + 8):
            moves[f] = -.05
        for f in range(first + 120, first + 160):
            moves[f] = .05 * 8 / 40
    z = .25
    for f in range(1, len(rows) + 1):
        z += moves.get(f, 0.0)
        rows[f - 1][RIGHT_Z] = repr(round(z, 6))
    if abs(z - .25) > 1e-6:
        raise SystemExit('the hand does not end where it started')
    meta = dict(select=600, triggers=TRIGGERS, strokes=STROKES, stop_frame=2100,
                expected=dict(trigger_alone=800, heavy=1100, first=[1500, 1800]), sword=dict(item=1125155603, primary=2496979158, heavy_grounded=2522099871))
    out = ['# Heavy attack by the right trigger held while swinging: trigger alone 800, stroke with trigger 1100, without 1500, trigger after the stroke 1800 (tools/make_trigger_heavy_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
