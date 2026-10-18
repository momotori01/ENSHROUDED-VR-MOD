"""Measure horizontal image translation in an explicit menu projection calibration.

The selected rock ROI avoids the menu text/animated character, and the title ROI
checks the UI separately. This is a scene-specific calibration, not a general
visual quality metric, stereo proof, or comfort test.
"""
import argparse
import json
import pathlib
import statistics
from check_capture import ppm


def shift(reference, target, width, height, rect, radius):
    x0, y0, x1, y1 = rect
    # Color differences on a sparse grid keep this standard-library-only tool small.
    points = [(x, y) for y in range(y0, y1, 7) for x in range(x0, x1, 7)]
    scores = []
    for dx in range(-radius, radius+1):
        values = []
        for x, y in points:
            if not 0 <= x+dx < width: continue
            a = (y*width+x)*3; b = (y*width+x+dx)*3
            values.append(sum(abs(reference[a+c]-target[b+c]) for c in range(3))/3)
        if len(values) == len(points): scores.append((statistics.mean(values), dx))
    best = min(scores)
    return dict(shift_pixels=best[1], mean_absolute_rgb_error=best[0], sampled_points=len(points),
                zero_shift_error=next(score for score, dx in scores if dx == 0))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('directory', type=pathlib.Path)
    a = p.parse_args()
    captures = [ppm(a.directory/f'phase{i}.ppm') for i in range(5)]
    width, height, reference = captures[1]
    if (width, height) != (1920, 1080): raise ValueError('ROI calibration is documented only for this 1920x1080 menu capture')
    if any((w,h) != (width,height) for w,h,_ in captures): raise ValueError('capture dimensions changed')
    rows = []
    for phase, expected in [(0,0), (2,-240), (3,240), (4,0)]:
        target = captures[phase][2]
        measured = shift(reference, target, width, height, (840, 620, 1080, 770), 300)
        ok = abs(measured['shift_pixels']-expected) <= 3
        rows.append(dict(name=f'phase{phase}_world_translation', status='PASS' if ok else 'FAIL',
                         expected_shift_pixels=expected, **measured))
        ui = shift(reference, target, width, height, (235, 165, 730, 240), 30)
        rows.append(dict(name=f'phase{phase}_ui_stationary', status='PASS' if abs(ui['shift_pixels']) <= 1 else 'FAIL', **ui))
    for row in rows: print(row['status'], row['name'], row['shift_pixels'], 'px', row['mean_absolute_rgb_error'])
    (a.directory/'projection-displacement.json').write_text(json.dumps(dict(scope=__doc__, checks=rows), indent=2), encoding='utf8')
    return int(any(row['status'] == 'FAIL' for row in rows))


if __name__ == '__main__':
    raise SystemExit(main())
