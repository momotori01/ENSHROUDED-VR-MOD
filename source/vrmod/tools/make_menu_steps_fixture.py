#!/usr/bin/env python3
"""Mock hand script from a list of menu steps, for looking around the game's menus with the menu pointer
(-MenuPointer; tools/make_menu_pointer_fixture.py has the geometry and the first checks).

The menu is opened as there: the left hand at the left temple 280..489, right stick click 400..406 (Start).
--steps is a list "frame:what[:arguments]" separated by ';':
  point:x,y      from this frame on the pointer rests on this pixel of the 1920x1080 image (it comes from 16 pixels
                 to its left within 20 frames)
  away           the pointer leaves the plate
  click          right trigger for 10 frames (the left mouse button while the pointer is on the plate)
  hold:n         right trigger for n frames
  right          right secondary button (B) for 7 frames (the right mouse button on the plate, the pad's B off it)
  a / x / y      right primary (A), left primary (X), left secondary (Y) for 7 frames - pad buttons
  press:name:n   one of a, b, x, y for n frames
  lstick:x,y:n   left stick for n frames;  rstick:x,y:n the right one
  start          right stick click for 7 frames with the left hand at the temple (Start: opens / closes the menu)
Example: --steps "520:point:525,58;600:click;760:point:959,540" --stop 1000
"""
import argparse, json, pathlib
import make_menu_pointer_fixture as base

LEFT_X, LEFT_Y, LEFT_Z = 1, 2, 3
RIGHT_YAW, RIGHT_PITCH, RIGHT_TRIGGER = 20, 21, 23
LEFT_TRIGGER, LEFT_GRAB, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK = 10, 11, 24, 29, 30, 31, 32, 33, 34
LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY = 35, 36, 37, 38
BUTTONS = dict(a=RIGHT_PRIMARY, b=RIGHT_SECONDARY, x=LEFT_PRIMARY, y=LEFT_SECONDARY)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--source', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-ranged-animation-20261010.csv'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('vrmod/test_env/home-menu-steps-20261012.csv'))
    p.add_argument('--steps', required=True)
    p.add_argument('--stop', type=int, default=1500)
    a = p.parse_args()
    rows = [line.split(',') for line in a.source.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if len(rows) != 4300 or any(len(r) != 39 for r in rows) or rows[0][LEFT_X:LEFT_Z + 1] != ['0.38', '1.25', '-0.25']:
        raise SystemExit('unexpected source script')
    for r in rows:
        for c in (LEFT_TRIGGER, LEFT_GRAB, RIGHT_TRIGGER, RIGHT_GRAB, LEFT_STICK_X, LEFT_STICK_Y, LEFT_STICK_CLICK, RIGHT_STICK_X, RIGHT_STICK_Y, RIGHT_STICK_CLICK,
                  LEFT_PRIMARY, LEFT_SECONDARY, RIGHT_PRIMARY, RIGHT_SECONDARY):
            r[c] = '0'
        r[RIGHT_YAW], r[RIGHT_PITCH] = '0.0', '0.0'

    def hold(column, first, count, value='1'):
        for f in range(first, min(first + count, len(rows) + 1)):
            rows[f - 1][column] = value

    def temple(first, last):
        # The left hand to the left temple and back, 20 frames each way.
        rest, at = (0.38, 1.25, -0.25), (0.0, 1.65, -0.18)
        for f in range(first, last + 21):
            t = min(1.0, (f - first + 1) / 20) if f <= last else max(0.0, 1 - (f - last) / 20)
            x, y, z = (b + (g - b) * t for b, g in zip(rest, at))
            rows[f - 1][LEFT_X], rows[f - 1][LEFT_Y], rows[f - 1][LEFT_Z] = repr(round(x, 5)), repr(round(y, 5)), repr(round(z, 5))

    temple(280, 469); hold(RIGHT_STICK_CLICK, 400, 7)
    steps = []
    for item in a.steps.split(';'):
        parts = item.strip().split(':')
        steps.append((int(parts[0]), parts[1], parts[2:]))
    steps.sort(key=lambda s: s[0])
    pointer = [(f, w, args) for f, w, args in steps if w in ('point', 'away')]
    for i, (first, what, args) in enumerate(pointer):
        end = pointer[i + 1][0] if i + 1 < len(pointer) else len(rows) + 1
        if what == 'away':
            continue
        pixel = tuple(float(v) for v in args[0].split(','))
        for f in range(first, end):
            t = min(1.0, (f - first) / 20)
            yaw, pitch = base.aim((pixel[0] - 16 * (1 - t), pixel[1]))
            rows[f - 1][RIGHT_YAW], rows[f - 1][RIGHT_PITCH] = repr(round(yaw, 4)), repr(round(pitch, 4))
    for first, what, args in steps:
        if what == 'click':
            hold(RIGHT_TRIGGER, first, 10)
        elif what == 'hold':
            hold(RIGHT_TRIGGER, first, int(args[0]))
        elif what == 'right':
            hold(RIGHT_SECONDARY, first, 7)
        elif what in BUTTONS:
            hold(BUTTONS[what], first, 7)
        elif what == 'press':
            hold(BUTTONS[args[0]], first, int(args[1]))
        elif what in ('lstick', 'rstick'):
            x, y = args[0].split(','); cx, cy = (LEFT_STICK_X, LEFT_STICK_Y) if what == 'lstick' else (RIGHT_STICK_X, RIGHT_STICK_Y)
            hold(cx, first, int(args[1]), x); hold(cy, first, int(args[1]), y)
        elif what == 'start':
            temple(first - 60, first + 20); hold(RIGHT_STICK_CLICK, first, 7)
        elif what not in ('point', 'away'):
            raise SystemExit('unknown step ' + what)
    meta = dict(start=[400, 407], steps=a.steps, stop_frame=a.stop)
    out = ['# Menu steps with the menu pointer (tools/make_menu_steps_fixture.py): ' + a.steps, '# ' + json.dumps(meta)]
    out += [','.join(r) for r in rows]
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, json.dumps(meta))


if __name__ == '__main__':
    main()
