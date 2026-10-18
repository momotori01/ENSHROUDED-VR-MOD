"""The hand script of the menu test (Mock runtime): from the game's main menu into the last world with the
controllers alone - the plate in the headset (layer/screen_xr.hpp), the right hand's ray as the mouse, its trigger as
the click, the stick as the arrow keys, B as the game's "back".

User, 2026-10-14: "from the start, consistently operable in VR is a must".

The steps are single presses at fixed XR frames (the Mock runtime gives 90 frames a second from the session's start,
which is in the game's first seconds; the menu is up some 20 s later):
   1800  left stick down, 1900 up      the highlight moves down and back (the arrow keys)
   2700  ray on "Play", trigger         main menu -> the play screen
   3000  B                              back to the main menu (ESC)
   3300  ray on "Play", trigger         the play screen again
   3750  ray on "Private", trigger      the list of worlds (the last one is selected)
   4200  ray on the world's "Play"      the world loads
Every press is one short pulse: a press that comes on another screen than it was made for meets nothing there.
The pixels are those of the 1920 x 1080 window (docs/analysis: main menu 500,566 / 430,360 / 1240,934).
After the last step the hands rest at the sides for the world's session.
"""
import argparse, math, pathlib

HEAD = (0.0, 1.65, 0.0)            # test_env/world-static-pose-1280x1440.csv
DISTANCE, WIDTH = 2.0, 3.0         # layer/screen_xr.hpp: plateDistance, plateWidth
PIXELS = (1920, 1080)
RIGHT = (0.20, 1.35, -0.30)
LEFT = (-0.20, 1.30, -0.30)


def aim(pixel, hand=RIGHT):
    """Yaw and pitch (degrees, evr::euler: yaw about +y, then pitch about +x; ahead is -z) of the ray from the hand to
    that pixel of the plate, which stands DISTANCE ahead of the head, upright, its middle at the head's height."""
    height = WIDTH * PIXELS[1] / PIXELS[0]
    target = (HEAD[0] + (pixel[0] / PIXELS[0] - .5) * WIDTH, HEAD[1] + (.5 - pixel[1] / PIXELS[1]) * height, HEAD[2] - DISTANCE)
    d = [target[i] - hand[i] for i in range(3)]; size = math.sqrt(sum(v * v for v in d))
    return math.degrees(math.atan2(-d[0], -d[2])), math.degrees(math.asin(d[1] / size))


def main():
    p = argparse.ArgumentParser(description=__doc__); p.add_argument('out', type=pathlib.Path); p.add_argument('--frames', type=int, default=9000); a = p.parse_args()
    steps = [(2700, (500, 566)), (3300, (500, 566)), (3750, (430, 360)), (4200, (1240, 934))]   # frame of the trigger, pixel
    rows = ['# The menu test: the plate, the ray, the trigger, the stick and B (tools/make_main_menu_fixture.py).',
            '# {"stick_down": 1800, "stick_up": 1900, "play": 2700, "back": 3000, "play_again": 3300, "private": 3750, "world_play": 4200}']
    for frame in range(1, a.frames + 1):
        yaw, pitch, trigger = 0.0, -80.0, 0.0                       # the ray at the floor: not on the plate
        for at, pixel in steps:
            if at - 90 <= frame < at + 60: yaw, pitch = aim(pixel)  # on the button a second before the press
            if at <= frame < at + 12: trigger = 1.0
        stick_y = -1.0 if 1800 <= frame < 1812 else 1.0 if 1900 <= frame < 1912 else 0.0
        back = 1 if 3000 <= frame < 3012 else 0
        left = [LEFT[0], LEFT[1], LEFT[2], 0, 0, 0, 0, -80, 0, 0.0, 0, 1, 1]
        right = [RIGHT[0], RIGHT[1], RIGHT[2], 0, 0, 0, round(yaw, 4), round(pitch, 4), 0, trigger, 0, 1, 1]
        tail = [1, 0, 0.0, stick_y, 0, 0.0, 0.0, 0, 0, 0, 0, back]
        rows.append(','.join(str(v) for v in [frame] + left + right + tail))
    a.out.write_text('\n'.join(rows) + '\n', encoding='utf-8')
    print('written', a.out, a.frames, 'frames; aims:', [(s[0], tuple(round(v, 2) for v in aim(s[1]))) for s in steps])


if __name__ == '__main__': raise SystemExit(main())
