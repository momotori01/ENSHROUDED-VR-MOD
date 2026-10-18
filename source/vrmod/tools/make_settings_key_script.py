#!/usr/bin/env python3
"""Key script (hooks/key_script.hpp, EVR_KEY_SCRIPT) that changes rows of the game's display settings and puts each
back, for checking that a change neither ends the VR session nor breaks the picture (user request 2026-10-13).

The way there, as the user went it by hand (game-20261013-025335/keyboard-operations.jsonl) and as seen in the Mock
runs game-20261013-030410 / 030723: ESC (pause menu), down, Enter (settings, on the "game" tab), Q twice (the display
tab). The list's first focus is "reset to defaults" - the script never presses Enter, Space or F there. Down goes
through the rows (disabled rows are skipped):
   1 display mode      2 resolution scale   3 NVIDIA Reflex     4 vertical sync      5 field of view
   6 sharpness         7 bloom              8 brightness        9 quality preset    10 anti-aliasing
  11 DLSS quality     12 texture resolution 13 shadow quality   14 contact shadows   15 indirect lighting
  16 reflections      17 water              18 fog quality      19 SSAO              20 distant objects
  21 voxel detail     22 small foliage      23 voxel mesh       24 tessellation      25 point light shadows
Each row gets the steps that bring it back to where it was, by which arrows the row showed on 2026-10-13 with the
user's settings (R = right only: right, left; L = left only: left, right; B = both: right, left, left, right).
If the settings differ, the rows may not come back - the caller compares enshrouded_local.json before and after.

  make_settings_key_script.py --rows 12-25 --output vrmod/test_env/keys-settings-rows-12-25.txt
"""
import argparse, pathlib

ARROWS = {1: 'L', 2: 'L', 3: 'B', 4: 'R', 5: 'R', 6: 'R', 7: 'R', 8: 'R', 10: 'B', 11: 'L', 12: 'L', 13: 'R', 14: 'R', 15: 'R', 16: 'R', 17: 'R', 18: 'R', 19: 'B', 20: 'B', 21: 'B', 22: 'B', 23: 'R', 24: 'B', 25: 'B'}
NAMES = {1: 'display mode', 2: 'resolution scale (slider at its top)', 3: 'NVIDIA Reflex', 4: 'vertical sync', 5: 'field of view (slider)', 6: 'sharpness (slider)', 7: 'bloom (slider)', 8: 'brightness (slider)', 10: 'anti-aliasing', 11: 'DLSS quality', 12: 'texture resolution', 13: 'shadow quality', 14: 'contact shadows',
         15: 'indirect lighting', 16: 'reflections', 17: 'water', 18: 'fog quality', 19: 'SSAO', 20: 'distant objects', 21: 'voxel detail', 22: 'small foliage', 23: 'voxel mesh',
         24: 'tessellation', 25: 'point light shadows'}
STEPS = {'R': ['RIGHT', 'LEFT'], 'L': ['LEFT', 'RIGHT'], 'B': ['RIGHT', 'LEFT', 'LEFT', 'RIGHT']}


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--rows', required=True, help='rows to change, e.g. 12-25 or 13,19,22')
    p.add_argument('--output', type=pathlib.Path, required=True)
    p.add_argument('--settle', type=int, default=90, help='XR frames between two changes')
    a = p.parse_args()
    rows = []
    for part in a.rows.split(','):
        if '-' in part:
            first, last = (int(v) for v in part.split('-')); rows += list(range(first, last + 1))
        else:
            rows.append(int(part))
    rows = sorted(set(rows))
    if any(r not in ARROWS for r in rows):
        raise SystemExit('rows without a known arrow state: %s' % [r for r in rows if r not in ARROWS])
    out = ['# Display settings: rows %s changed and put back (tools/make_settings_key_script.py). Never Enter / Space / F here.' % a.rows,
           '500 ESC', '560 DOWN', '620 ENTER', '700 Q', '760 Q']
    frame = 820; at = 0; pictures = []
    for row in rows:
        while at < row:
            out.append('%d DOWN' % frame); frame += 20; at += 1
        frame += 40
        out.append('# row %d: %s (%s)' % (row, NAMES[row], ARROWS[row]))
        for i, key in enumerate(STEPS[ARROWS[row]]):
            out.append('%d %s' % (frame, key))
            if i in (0, 2):
                pictures.append(frame + a.settle - 20)          # the row away from the user's value
            frame += a.settle
    out.append('# ends at %d; pictures of the changed state: %s' % (frame, ','.join(str(v) for v in pictures)))
    a.output.write_text('\n'.join(out) + '\n', encoding='utf-8', newline='\n')
    print(a.output, 'stop', frame + 100, 'pictures', ','.join(str(v) for v in pictures))


if __name__ == '__main__':
    main()
