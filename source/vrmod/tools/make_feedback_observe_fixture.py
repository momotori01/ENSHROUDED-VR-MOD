#!/usr/bin/env python3
"""Mock hand script to look at what the headset run of 2026-10-12 (game145015) reported and the mod has no answer for
yet: the staff that summons a craftsman (NPC_Crafter_Alchemist01: "after summoning I cannot move, the camera is odd,
aim it with the right hand, the staff is not in the hand"), the watering can ("not attached to the hand"), and the
scorch mark of a bomb ("in one eye only"; run it with -ShareDecals).

On the default (derived) home save the script starts with the bow of the first action bar page.
  420..539    wheel held, right stick click 460..466: second page (the bomb of that page is selected by it)
  700..744    right trigger held, the hand moves forward and up from 730, release in 745: a bomb forward
  1100..1169  wheel towards the watering can (slot 8 of the page, -1 degrees)
  1200..1500  the right hand's pointer straight ahead, 30 degrees down; right trigger 1300..1329: water
  1600..1669  wheel towards the craftsman's staff (slot 3 of the page, 129 degrees)
  1700..2900  the pointer 30 degrees down; from 1900 turned 30 degrees to one side
  2000..2009  right trigger: use (summon)
  2300..2329  left stick forward: can the character walk after it?
  2500..2509  left trigger (what the building hammer enters its mode with)
  2700..2729  left stick forward again
Stop at 3000. Captures of the eyes and of the UI tell the rest (the game's own key hints are in the UI image).
"""
import argparse, json, math, pathlib

RIGHT_X, RIGHT_Y, RIGHT_YAW, RIGHT_PITCH = 14, 15, 20, 21
LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 23, 24, 29, 30, 32, 33, 34
WHEEL = [(1100, -1.0), (1600, 129.0)]


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-feedback-observe-20261012.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows):
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    hold(RIGHT_GRAB, 420, 540); hold(RIGHT_STICK_CLICK, 460, 467)
    # the throw of tools/make_throw_fixture.py: forward and up, still moving at the release
    hold(RIGHT_TRIGGER, 700, 745)
    x0, y0 = float(rows[0][RIGHT_X]), float(rows[0][RIGHT_Y]); moves = {}
    for f in range(730, 750):
        moves[f] = (.03, .015)
    for f in range(800, 880):
        moves[f] = (-.03 * 20 / 80, -.015 * 20 / 80)
    x, y = x0, y0
    for f in range(1, len(rows) + 1):
        dx, dy = moves.get(f, (0.0, 0.0)); x += dx; y += dy
        rows[f - 1][RIGHT_X], rows[f - 1][RIGHT_Y] = repr(round(x, 6)), repr(round(y, 6))
    if abs(x - x0) > 1e-6 or abs(y - y0) > 1e-6:
        raise SystemExit('the hand does not end where it started')
    for first, degrees in WHEEL:
        angle = math.radians(degrees)
        hold(RIGHT_GRAB, first, first + 60)
        for f in range(first + 20, first + 70):
            rows[f - 1][RIGHT_STICK_X], rows[f - 1][RIGHT_STICK_Y] = repr(math.cos(angle)), repr(math.sin(angle))
    for f in range(1200, 1501):
        rows[f - 1][RIGHT_YAW], rows[f - 1][RIGHT_PITCH] = '-90', '-30'
    for f in range(1700, 2901):
        rows[f - 1][RIGHT_YAW], rows[f - 1][RIGHT_PITCH] = ('-90' if f < 1900 else '-60'), '-30'
    hold(RIGHT_TRIGGER, 1300, 1330)
    hold(RIGHT_TRIGGER, 2000, 2010)
    hold(LEFT_STICK_Y, 2300, 2330, '1.0')
    hold(LEFT_TRIGGER, 2500, 2510)
    hold(LEFT_STICK_Y, 2700, 2730, '1.0')
    meta = dict(page_swap=[420, 540], throw=dict(wind=700, release=745), wheel=WHEEL, water=[1300, 1330], use=[2000, 2010], left_trigger=[2500, 2510], walks=[[2300, 2330], [2700, 2730]], stop_frame=3000,
                items=dict(bomb=2283229289, watering_can=2395034423, craftsman_staff=4146744324))
    out = ['# Looking at the craftsman staff, the watering can and a bomb mark after the headset run game145015 (tools/make_feedback_observe_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output)


if __name__ == '__main__':
    main()
