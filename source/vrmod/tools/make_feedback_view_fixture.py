#!/usr/bin/env python3
"""Mock hand script for the second look after the headset run of 2026-10-12 (game145015):
the game's "first person mode" of a building tool (right stick click; cameras 36 / 37, now first person here), the
watering can's carried mount, a bomb's mark on the floor (run it with -ShareDecals) and walking right after a stroke
of the felling axe (its movement factors are withheld).

On the default (derived) home save the script starts with the bow of the first action bar page.
  420..539    wheel held, the left stick's click 460..466 (--page-click): second page (the bomb of that page is selected by it)
  700..744    right trigger held, the hand moves forward and a little down from 730, release in 745: a bomb to the floor
  1100..1169  wheel towards the watering can (slot 8 of the page, -1 degrees)
  1200..1650  the right hand's pointer straight ahead, 30 degrees down
  1300..1306  right stick click with the wheel closed: the game's first person mode on (camera 38 -> 36)
  1500..1506  right stick click again: off
  1700..1819  wheel held, the same click 1740..1746: first page; the same slot there is the felling axe
  2000        a stroke across (8 frames of 0.05 m along -Z); left stick forward 2015..2074, a third of a second
              after it: with the game's own stroke the character stands still from 0.4 s to 1.8 s
Stop at 2400.
"""
import argparse, json, math, pathlib

RIGHT_X, RIGHT_Y, RIGHT_Z, RIGHT_YAW, RIGHT_PITCH = 14, 15, 16, 20, 21
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 29, 30, 31, 32, 33, 34


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-feedback-view-20261012.csv'))
    p.add_argument('--page-click', choices=('left', 'right'), default='left', help='which stick click swaps the action bar page while the wheel is held (user decision 2026-10-12: the left one)')
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][RIGHT_Z] != '0.25':
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    page = LEFT_STICK_CLICK if a.page_click == 'left' else RIGHT_STICK_CLICK
    hold(RIGHT_GRAB, 420, 540); hold(page, 460, 467)
    hold(RIGHT_TRIGGER, 700, 745)
    x0, y0, z0 = float(rows[0][RIGHT_X]), float(rows[0][RIGHT_Y]), .25; moves = {}
    for f in range(730, 750):
        moves[f] = (.03, -.01, 0.0)
    for f in range(800, 880):
        moves[f] = (-.03 * 20 / 80, .01 * 20 / 80, 0.0)
    for f in range(1180, 1200):          # the hand up for the watering can: the can hangs under the fist
        moves[f] = (0.0, .0125, 0.0)
    for f in range(1660, 1680):
        moves[f] = (0.0, -.0125, 0.0)
    for f in range(2000, 2008):
        moves[f] = (0.0, 0.0, -.05)
    for f in range(2150, 2190):
        moves[f] = (0.0, 0.0, .05 * 8 / 40)
    x, y, z = x0, y0, z0
    for f in range(1, len(rows) + 1):
        dx, dy, dz = moves.get(f, (0.0, 0.0, 0.0)); x += dx; y += dy; z += dz
        rows[f - 1][RIGHT_X], rows[f - 1][RIGHT_Y], rows[f - 1][RIGHT_Z] = repr(round(x, 6)), repr(round(y, 6)), repr(round(z, 6))
    if abs(x - x0) > 1e-6 or abs(y - y0) > 1e-6 or abs(z - z0) > 1e-6:
        raise SystemExit('the hand does not end where it started')
    angle = math.radians(-1.0)
    hold(RIGHT_GRAB, 1100, 1160)
    for f in range(1120, 1170):
        rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    for f in range(1200, 1651):
        rows[f - 1][RIGHT_YAW], rows[f - 1][RIGHT_PITCH] = '-90', '-30'
    hold(RIGHT_STICK_CLICK, 1300, 1307)
    hold(RIGHT_STICK_CLICK, 1500, 1507)
    hold(RIGHT_GRAB, 1700, 1820); hold(page, 1740, 1747)
    hold(LEFT_STICK_Y, 2015, 2075, '1.0')
    meta = dict(page_swaps=[[420, 540], [1700, 1820]], throw=dict(wind=700, release=745), watering_can=1100, first_person_mode=[[1300, 1307], [1500, 1507]], stroke=2000, walk=[2015, 2075], stop_frame=2400,
                items=dict(bomb=2283229289, watering_can=2395034423, felling_axe=497414641), cameras=dict(building_third_person=38, building_first_person=36))
    out = ['# Second look after the headset run game145015: bomb to the floor 745, watering can 1100, first person mode 1300 / 1500, felling axe stroke 2000 with a walk from 2015 (tools/make_feedback_view_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
