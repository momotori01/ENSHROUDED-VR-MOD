#!/usr/bin/env python3
"""Mock hand script for the strokes the headset run of 2026-10-12 (game145015) found wanting: the pickaxe (a tool's
stroke without an Impact: its SpawnEntityEvent goes early), the felling axe (movement factors withheld), and the pair
of daggers' second step and heavy attack (two Impacts at one time; before, such attacks kept the game's timing, step
and swing trail). The swing's sound (SfxNotifierEvent) is withheld in all of them.

On the default (derived) home save the script starts with the bow of the first action bar page.
  600..669    wheel towards the pickaxe (slot 7 of the page, 25 degrees; item 2671192505, stroke 1808128177)
  700..1790   the right hand's pointer straight ahead, 30 degrees down (the aim circle on the floor in front)
  800, 1000   a stroke across (8 frames of 0.05 m along -Z, 4.5 m/s); left stick forward 1005..1034 and back
              1100..1129: the pickaxe's action lets the character walk and is not broken off by it
  1200..1269  wheel towards the felling axe (slot 8, -1 degrees; item 497414641, stroke 1659104355)
  1400, 1600  the same stroke; left stick forward 1612..1671 and back 1700..1759: the walk breaks the felling axe's
              action off once the early stroke is done and its use is paid for (0.27 s; the game's own: 1.8 s)
  1800..1919  wheel held, the left stick's click 1840..1846 (--page-click): second page
  1980..2049  wheel towards the daggers (slot 6 of the page, 51 degrees; item 3974938802: first attack 3838423204,
              second step 2736483269, heavy attack on the ground 2257955339)
  2200        a stroke of the right hand; 2236 one of the left hand (the second step of the chain)
  2600        a stroke of the right hand with the right trigger held 2560..2699: the heavy attack
Each stroke is followed by a slow return.
"""
import argparse, json, math, pathlib

LEFT_Z, RIGHT_Z, RIGHT_YAW, RIGHT_PITCH = 3, 16, 20, 21
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 29, 30, 31, 32, 33, 34
WALKS = [(1005, 1035, '1.0'), (1100, 1130, '-1.0'), (1612, 1672, '1.0'), (1700, 1760, '-1.0')]
RIGHT_STROKES = [800, 1000, 1400, 1600, 2200, 2600]
LEFT_STROKES = [2236]
WHEEL = [(600, 25.0), (1200, -1.0), (1980, 51.0)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-feedback-strokes-20261012.csv'))
    p.add_argument('--page-click', choices=('left', 'right'), default='left', help='which stick click swaps the action bar page while the wheel is held (user decision 2026-10-12: the left one)')
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_Z] != '0.25':
        raise SystemExit('unexpected source script')
    left0 = float(rows[0][LEFT_Z])
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    for first, degrees in WHEEL:
        angle = math.radians(degrees)
        hold(RIGHT_GRAB, first, first + 60)
        for f in range(first + 20, first + 70):
            rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    hold(RIGHT_GRAB, 1800, 1920); hold(LEFT_STICK_CLICK if a.page_click == 'left' else RIGHT_STICK_CLICK, 1840, 1847)
    for f in range(700, 1791):
        rows[f - 1][RIGHT_YAW], rows[f - 1][RIGHT_PITCH] = '-90', '-30'
    for first, end, value in WALKS:
        hold(LEFT_STICK_Y, first, end, value)
    hold(RIGHT_TRIGGER, 2560, 2700)

    def strokes(column, start, firsts):
        moves = {}
        for first in firsts:
            for f in range(first, first + 8):
                moves[f] = -.05
            for f in range(first + 100, first + 140):
                moves[f] = moves.get(f, 0.0) + .05 * 8 / 40
        z = start
        for f in range(1, len(rows) + 1):
            z += moves.get(f, 0.0)
            rows[f - 1][column] = repr(round(z, 6))
        if abs(z - start) > 1e-6:
            raise SystemExit('a hand does not end where it started')

    strokes(RIGHT_Z, .25, RIGHT_STROKES)
    strokes(LEFT_Z, left0, LEFT_STROKES)
    meta = dict(wheel=WHEEL, page_swap=[1800, 1920], right_strokes=RIGHT_STROKES, left_strokes=LEFT_STROKES, trigger=[2560, 2700], walks=[[w[0], w[1]] for w in WALKS], stop_frame=3000,
                items=dict(pickaxe=2671192505, felling_axe=497414641, daggers=3974938802),
                sequences=dict(pickaxe=1808128177, felling_axe=1659104355, daggers_first=3838423204, daggers_second=2736483269, daggers_heavy=2257955339))
    out = ['# Strokes after the headset run game145015: pickaxe 800 1000 (walk 1005), felling axe 1400 1600 (walk 1612), daggers right 2200 left 2236, heavy with the trigger 2600 (tools/make_feedback_strokes_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
