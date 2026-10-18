"""Write the Mock controller sequence for the live input check (2026-10-09, real-device phase 1).

test_env/world-live-input-hands.csv drives the live pad table (include/evr/live_pad.hpp) through ordinary
play on the Mock runtime, one row per XR frame (the last row holds after the end):

  900-1019   left stick forward            -> walk (left stick passes through)
  1080-1139  right stick right 0.8         -> turn (right stick X passes through)
  1200-1205  right A                       -> A (jump)
  1300-1306  right stick down              -> B (dodge; right Y never reaches the game in play)
  1400-1625  right hand at the right temple -> gesture (right stick becomes the D-pad)
  1460-1465  right stick click (gesture)   -> Start (menu opens)
  1540-1545  right B in the menu           -> B (back) and it stays B while the menu closes (press-edge latch)
  1700-1705  left X in play                -> nothing
  1800-1919  right grip held               -> RB (native quick wheel opens; no stick, nothing selected)
  2000-      neutral

Hands are tracked Touch controllers (profile 0), away from both temple regions except the gesture span.
`--verify` checks that the committed file matches this generator.

`--scenario system` (2026-10-09, the two real-device stops) writes test_env/world-live-input-system-hands.csv:
  900-1400   right hand at the right temple (gesture)
  960-965    left stick click (gesture)    -> Select (Back): the full-screen system screen, frontend screen 2
  1260-1265  right B on that screen        -> B (back)
  1500-1620  right grip held               -> RB (quick wheel)
  1540-1600  right stick right in the wheel -> picks the right slot (the equipment changes, as in the first run)
  1700-      neutral

`--scenario glide` (third real-device run: the glider cape covered half the view; jump/dodge went diagonally) writes
test_env/world-live-input-glide-hands.csv: left stick forward 900-1300, right A (jump) 1000-1005, right A held
1020-1300 (glide in the air), right stick down (dodge) 1500-1506 while walking 1450-1560, then neutral from 1600.
"""
import argparse
import pathlib
import sys

ENV = pathlib.Path(__file__).resolve().parent.parent / 'test_env'
OUT = ENV / 'world-live-input-hands.csv'
LAST = 2100
SYSTEM_OUT = ENV / 'world-live-input-system-hands.csv'
SYSTEM_LAST = 1800
GLIDE_OUT = ENV / 'world-live-input-glide-hands.csv'
GLIDE_LAST = 1700


def system_spans(f, left, right):
    if 900 <= f <= 1400:
        right['pos'] = (0.18, 1.65, 0.0)
    if 960 <= f <= 965:
        left['click'] = 1
    if 1260 <= f <= 1265:
        right['faces'] = (0, 1)
    if 1500 <= f <= 1620:
        right['grip'] = 1
    if 1540 <= f <= 1600:
        right['stick'] = (1.0, 0.0)


def glide_spans(f, left, right):
    if 900 <= f <= 1300 or 1450 <= f <= 1560:
        left['stick'] = (0.0, 1.0)
    if 1000 <= f <= 1005 or 1020 <= f <= 1300:
        right['faces'] = (1, 0)
    if 1500 <= f <= 1506:
        right['stick'] = (0.0, -1.0)


def row(f, scenario='basic'):
    left = dict(pos=(-0.25, 1.25, -0.38), stick=(0.0, 0.0), click=0, faces=(0, 0), grip=0)
    right = dict(pos=(0.5, 1.65, 0.0), stick=(0.0, 0.0), click=0, faces=(0, 0), grip=0)
    if scenario == 'glide':
        glide_spans(f, left, right)
        return encode(f, left, right)
    if scenario == 'system':
        system_spans(f, left, right)
        return encode(f, left, right)
    if 900 <= f <= 1019:
        left['stick'] = (0.0, 1.0)
    if 1080 <= f <= 1139:
        right['stick'] = (0.8, 0.0)
    if 1200 <= f <= 1205:
        right['faces'] = (1, 0)
    if 1300 <= f <= 1306:
        right['stick'] = (0.0, -1.0)
    if 1400 <= f <= 1625:
        right['pos'] = (0.18, 1.65, 0.0)
    if 1460 <= f <= 1465:
        right['click'] = 1
    if 1540 <= f <= 1545:
        right['faces'] = (0, 1)
    if 1700 <= f <= 1705:
        left['faces'] = (1, 0)
    if 1800 <= f <= 1919:
        right['grip'] = 1
    return encode(f, left, right)


def encode(f, left, right):
    def hand(h):
        x, y, z = h['pos']
        return [x, y, z, 0, 0, 0, 0, 0, 0, 0, h['grip'], 1, 1]
    values = [f] + hand(left) + hand(right) + [1, 0]
    for h in (left, right):
        values += [h['stick'][0], h['stick'][1], h['click']]
    for h in (left, right):
        values += list(h['faces'])
    return ','.join(f'{v:g}' if isinstance(v, float) else str(v) for v in values)


def text(scenario='basic'):
    if scenario == 'glide':
        lines = ['# Live input glide/jump/dodge check (generate_live_input_hands.py --scenario glide): walk900-1300 jumpA1000 '
                 'holdA1020-1300 walk1450-1560 dodge1500, then neutral.']
        lines += [row(f, 'glide') for f in range(1, GLIDE_LAST + 1)]
        return '\n'.join(lines) + '\n'
    if scenario == 'system':
        lines = ['# Live input system screen/wheel check (generate_live_input_hands.py --scenario system): temple900-1400 '
                 'Select960 B-back1260 wheel-grip1500-1620 stick-right1540-1600, then neutral.']
        lines += [row(f, 'system') for f in range(1, SYSTEM_LAST + 1)]
        return '\n'.join(lines) + '\n'
    lines = ['# Live input check (generate_live_input_hands.py): walk900 turn1080 jumpA1200 dodge1300 temple1400-1625 '
             'Start1460 B-back1540 leftX1700(none) wheel-grip1800-1919, then neutral.']
    lines += [row(f) for f in range(1, LAST + 1)]
    return '\n'.join(lines) + '\n'


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--verify', action='store_true')
    p.add_argument('--scenario', choices=('basic', 'system', 'glide'), default='basic')
    args = p.parse_args()
    out, last = {'system': (SYSTEM_OUT, SYSTEM_LAST), 'glide': (GLIDE_OUT, GLIDE_LAST)}.get(args.scenario, (OUT, LAST))
    if args.verify:
        ok = out.read_text(encoding='ascii') == text(args.scenario)
        print('PASS' if ok else 'FAIL', out)
        return 0 if ok else 1
    out.write_text(text(args.scenario), encoding='ascii', newline='\n')
    print('wrote', out, last, 'rows')
    return 0


if __name__ == '__main__':
    sys.exit(main())
