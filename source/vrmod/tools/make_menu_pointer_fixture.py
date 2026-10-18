#!/usr/bin/env python3
"""Mock hand script for the menu pointer (hooks/menu_pointer.hpp, EVR_MENU_POINTER=1 in the game's environment; value
logic include/evr/menu_pointer.hpp).

The head of the default head script stands at (0, 1.65, 0), faces +X and looks 15 degrees down. The GUI plate of an
open menu stands upright 1.5 m ahead of where the head was when the menu opened, 2 m wide, in the level direction the
head looked (since 2026-10-13, include/evr/menu_pointer.hpp plateOrigin; before that it was fixed to the head and so
tilted 15 degrees with it: PLATE_PITCH = -15 gives those scripts again). The right hand is at (0.38, 1.25, 0.25); aim()
gives the pointer's yaw and pitch for a pixel of the 1920x1080 image (yaw -90 is straight ahead, more negative is to
the right; positive pitch is up).
The first look (game193917, sweeps across the plate) showed the backpack: tabs along the top (world 525, journal 706,
craft 886, backpack 1043, character 1225, skills 1404 at y 58), the flint arrows in the slot at (914, 572). The second
run (game195630) clicked 66 pixels to the left of the craft tab - its numbers were read off a scaled picture.

  280..489    the left hand at the left temple (the head gesture: stick clicks are Start / Select there)
  400..406    right stick click: Start - the backpack opens
  520..       the pointer comes to the craft tab (16 pixels in 20 frames); right trigger 600..609: a left click - the
              craft page
  720..       the same to the backpack tab; right trigger 800..809: back to the backpack
  920..       the same to the flint arrows; right secondary button (B) 1000..1006: a right click - the item's action
  1200..      the pointer away from the plate; B 1300..1306: the pad's "back" - the menu closes
Stop at 1500. menu_pointer_state gives the screen, the device the game listens to, the pointed pixel and what was
posted; the UI images sent to the headset show the dot.

--head-turn also writes <output>-head.csv (for -Pose): from frame 650 the head turns 30 degrees to the left and looks
5 degrees up instead of 15 down. The menu has been open since 400, so its plate must stay where it is: the clicks at
800 and 1000 and the "back" at 1300 do the same as with a still head. (With a plate that follows the head they miss.)
"""
import argparse, json, math, pathlib

LEFT_X, LEFT_Y, LEFT_Z = 1, 2, 3
RIGHT_YAW, RIGHT_PITCH, RIGHT_TRIGGER = 20, 21, 23
LEFT_TRIGGER, LEFT_GRAB, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 24, 29, 30, 31, 32, 33, 34
LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY = 35, 36, 37, 38
HEAD, HEAD_PITCH, PLATE_PITCH, HAND = (0.0, 1.65, 0.0), -15.0, 0.0, (0.38, 1.25, 0.25)
CRAFT_TAB, BACKPACK_TAB, ARROWS = (886, 58), (1043, 58), (914, 572)


def aim(pixel):
    """Yaw and pitch (degrees) of the right hand's pointer that meets this pixel of the plate."""
    p = math.radians(PLATE_PITCH)
    forward, up = (math.cos(p), math.sin(p), 0.0), (-math.sin(p), math.cos(p), 0.0)
    x, y = (pixel[0] / 1920 - .5) * 2.0, (.5 - pixel[1] / 1080) * 1.125
    point = [HEAD[i] + 1.5 * forward[i] + y * up[i] for i in range(3)]
    point[2] += x                                   # the plate's right is +Z of the room
    d = [point[i] - HAND[i] for i in range(3)]
    return math.degrees(math.atan2(-d[0], -d[2])), math.degrees(math.atan2(d[1], math.hypot(d[0], d[2])))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-menu-pointer-20261012.csv'))
    p.add_argument('--head-turn', action='store_true', help='also write <output>-head.csv: the head turns away at 650')
    p.add_argument('--pose-source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-mill-20261010.csv'))
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][LEFT_X:LEFT_Z + 1] != ['0.38', '1.25', '-0.25']:
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK,
                  LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY):
            r[c] = '0'

    def hold(column, first, end, value='1'):
        for f in range(first, end):
            rows[f - 1][column] = value

    def point(first, end, pixel):
        # (0, 0) points along -Z: away from the plate. Onto a pixel the pointer comes from 16 pixels to its left.
        for f in range(first, end):
            if pixel:
                t = min(1.0, (f - first) / 20)
                yaw, pitch = aim((pixel[0] - 16 * (1 - t), pixel[1]))
            else:
                yaw, pitch = 0.0, 0.0
            rows[f - 1][RIGHT_YAW], rows[f - 1][RIGHT_PITCH] = repr(round(yaw, 4)), repr(round(pitch, 4))

    # The left hand to the left temple and back, 20 frames each way.
    base, temple = (0.38, 1.25, -0.25), (0.0, 1.65, -0.18)
    for f in range(280, 490):
        t = min(1.0, (f - 279) / 20) if f < 470 else max(0.0, 1 - (f - 469) / 20)
        x, y, z = (b + (g - b) * t for b, g in zip(base, temple))
        rows[f - 1][LEFT_X], rows[f - 1][LEFT_Y], rows[f - 1][LEFT_Z] = repr(round(x, 5)), repr(round(y, 5)), repr(round(z, 5))
    hold(RIGHT_STICK_CLICK, 400, 407)
    point(1, 520, None)
    point(520, 720, CRAFT_TAB); hold(RIGHT_TRIGGER, 600, 610)
    point(720, 920, BACKPACK_TAB); hold(RIGHT_TRIGGER, 800, 810)
    point(920, 1200, ARROWS); hold(RIGHT_SECONDARY, 1000, 1007)
    point(1200, len(rows) + 1, None); hold(RIGHT_SECONDARY, 1300, 1307)
    meta = dict(temple=[280, 490], start=[400, 407], craft_tab=dict(pixel=CRAFT_TAB, aim=aim(CRAFT_TAB), click=[600, 610]), backpack_tab=dict(pixel=BACKPACK_TAB, aim=aim(BACKPACK_TAB), click=[800, 810]),
                arrows=dict(pixel=ARROWS, aim=aim(ARROWS), right_click=[1000, 1007]), back=[1300, 1307], stop_frame=1500)
    out = ['# Menu pointer: backpack by Start at the temple 400, left click on the craft tab 600 and on the backpack tab 800, right click on the flint arrows 1000, B away from the plate 1300 (tools/make_menu_pointer_fixture.py).',
           '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, json.dumps(meta))
    if a.head_turn:
        poses = [line.split(',') for line in a.pose_source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
        if len(poses) != 6000 or any(len(r) != 20 for r in poses) or poses[0][1:6] != ['0', '1.65', '0', '-90', '-15.000000849']:
            raise SystemExit('unexpected head source')
        for frame in range(650, len(poses) + 1):
            t = min(1.0, (frame - 649) / 50)
            poses[frame - 1][4], poses[frame - 1][5] = repr(round(-90 + 30 * t, 6)), repr(round(-15 + 20 * t, 6))
        head = a.output.with_name(a.output.stem + '-head.csv')
        head.write_text('\n'.join(['# Head script for the menu plate (tools/make_menu_pointer_fixture.py --head-turn): from 650 the head turns 30 degrees left and 20 up.',
                                   '# ' + json.dumps(dict(turn=[650, 700], yaw=[-90, -60], pitch=[-15, 5]))] + [','.join(r) for r in poses]) + '\n', encoding='utf-8', newline='\n')
        print(head)


if __name__ == '__main__':
    main()
