#!/usr/bin/env python3
"""Read a self-test along a kept route (tools/run_route_check.ps1): did it walk where the user walked, and do the two
eyes of its pictures show the same brightness (2026-10-14, user: "the brightness differs between the eyes here and
there - that is what feels wrong"; "I press F8 where it shows; the self-test turns round there taking pictures").

  check_route_run.py <route name> <game folder under vrmod/output> [--plain]

The walk: the anchor's position in the run (recorded_xr_anchor) against the route's track.csv at the same point of the
walk - in a self-test every look round delays the later frames (route.json check.spots) - every 150 frames and at
every spot, with the heading.
The pictures: every CAPTURE of the script is the first picture of both eyes taken at or after its frame (the layer
looks for the request on every 15th pair). Each pair is compared as tools/check_eye_brightness.py does (blocks found
in the other eye by their shape, then their brightness compared); pairs with marked blocks get a sheet.
Writes <game folder>/route-check/report.txt (and brightness-<frame>.png sheets). Reads only.
"""
import argparse, json, math, pathlib, sys

root = pathlib.Path('S:/SteamLibrary/steamapps/common/Enshrouded')
sys.path.insert(0, str(pathlib.Path(__file__).parent))
import check_eye_brightness as eyes  # noqa: E402
from PIL import Image, ImageDraw, ImageStat  # noqa: E402

QUARTER_OF = 1963 / 4      # the Mock eye's width at a quarter: the block grid of check_eye_brightness.py


def anchors(game):
    probe = sorted(game.glob('view-resource-probe-*'))[-1] / 'view-resource-probe.jsonl'
    out = {}
    for line in open(probe, encoding='utf-8'):
        if '"recorded_xr_anchor"' not in line: continue
        e = json.loads(line); qx, qy, qz, qw = e['orientation']
        out[e['recorded_xr_frame']] = tuple(v / 4294967296.0 for v in e['camera_q32']) + (
            math.degrees(math.atan2(2 * (qw * qy + qx * qz), 1 - 2 * (qy * qy + qx * qx))),)
    return out


def at(table, f):
    if f in table: return table[f]
    keys = [k for k in table if abs(k - f) <= 3]
    return table[min(keys, key=lambda k: abs(k - f))] if keys else None


def turn(a, b):
    return (b - a + 180) % 360 - 180


def shifted(f, plans):
    """The frame of the self-test where the user's frame f is walked."""
    s = 0
    for p in plans:
        if p['spot'] < f: s += p['look_to'] - p['look_from']
    return f + s


def compare_pair(folder, f, out):
    left = eyes.luma(folder / f'frame-{f}-eye-0.ppm'); right = eyes.luma(folder / f'frame-{f}-eye-1.ppm')
    eyes.SCALE = max(1, round(left.size[0] / QUARTER_OF))
    ml, mr = ImageStat.Stat(left).mean[0], ImageStat.Stat(right).mean[0]
    sl, sr = eyes.small(left), eyes.small(right)
    blocks = eyes.compare(sl, sr)
    marked = [(x, y, d) for x, y, d in blocks if abs(d) > eyes.BRIGHT]
    lb = sum(1 for *_, d in marked if d > 0)
    mean_abs = sum(abs(d) for *_, d in blocks) / max(1, len(blocks))
    if marked:
        w, h = sl.size; sheet = Image.new('RGB', (w * 2, h), (0, 0, 0))
        sheet.paste(sl.convert('RGB'), (0, 0)); sheet.paste(sr.convert('RGB'), (w, 0)); draw = ImageDraw.Draw(sheet)
        for x, y, d in marked:
            c = (255, 40, 40) if d > 0 else (60, 120, 255)
            draw.rectangle((x, y, x + eyes.BLOCK - 1, y + eyes.BLOCK - 1), outline=c); draw.text((x + 2, y + 2), f'{d:+.0f}', fill=c)
        sheet.save(out / f'brightness-{f}.png')
    return dict(left=ml, right=mr, blocks=len(blocks), brighter_left=lb, brighter_right=len(marked) - lb, mean_abs=mean_abs,
                strongest=max((abs(d) for *_, d in marked), default=0.))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('route'); p.add_argument('game'); p.add_argument('--plain', action='store_true')
    a = p.parse_args()
    route = root / 'vrmod/test_env/routes' / a.route; game = root / 'vrmod/output' / a.game
    info = json.loads((route / 'route.json').read_text(encoding='utf-8'))
    plans = [] if a.plain else info['check']['spots']
    track = [tuple(float(v) for v in l.split(',')) for l in (route / 'track.csv').read_text(encoding='utf-8').splitlines()[1:] if l]
    run = anchors(game); out = game / 'route-check'; out.mkdir(exist_ok=True)
    lines = [f'route {a.route} ({info["game"]}, save {info["save"]}) run {a.game} {"replay" if a.plain else "self-test"}']
    # The walk.
    worst = 0.; checked = 0
    for f, x, y, z, h in track:
        f = int(f); here = at(run, shifted(f, plans))
        if here is None: continue
        d = math.dist((x, y, z), here[:3]); checked += 1; worst = max(worst, d)
        if f % 150 == 0: lines.append(f'  walk frame {f:5d} (run {shifted(f, plans):5d}): {d:6.2f} m apart, heading {h:7.1f} / {here[3]:7.1f} ({turn(h, here[3]):+.1f})')
    lines.insert(1, f'walk: {checked} points compared, at most {worst:.2f} m from the user\'s')
    for s in info['spots']:
        user = min(track, key=lambda t: abs(t[0] - s)); here = at(run, shifted(s, plans) if a.plain else next(p['check_frame'] for p in plans if p['spot'] == s))
        lines.append(f'spot {s}: ' + ('not reached' if here is None else f'{math.dist(user[1:4], here[:3]):.2f} m from where the user pressed F8, heading {user[4]:.1f} / {here[3]:.1f}'))
    # The pictures.
    folder = game / 'native-xr'
    taken = sorted({json.loads(l)['frame'] for l in open(folder / 'openxr.jsonl', encoding='utf-8') if '"native_fixture_capture_complete"' in l}) if (folder / 'openxr.jsonl').exists() else []
    for plan in plans:
        lines.append(f'look round at spot {plan["spot"]} (run frames {plan["look_from"]}..{plan["look_to"]}):')
        lines.append('   turn  pitch  picture  left  right  blocks  brighter L/R  mean|diff|  strongest')
        for shot in plan['pictures']:
            f = next((t for t in taken if shot['frame'] <= t < shot['frame'] + 40), None)
            if f is None or not (folder / f'frame-{f}-eye-1.ppm').exists():
                lines.append(f'  {shot["turn"]:5.0f} {shot["pitch"]:6.0f}  missing (asked at {shot["frame"]})'); continue
            r = compare_pair(folder, f, out)
            lines.append(f'  {shot["turn"]:5.0f} {shot["pitch"]:6.0f}  {f:7d} {r["left"]:5.1f}  {r["right"]:5.1f}  {r["blocks"]:6d}  {r["brighter_left"]:5d} / {r["brighter_right"]:<5d} {r["mean_abs"]:8.2f}  {r["strongest"]:8.1f}')
    text = '\n'.join(lines) + '\n'
    (out / 'report.txt').write_text(text, encoding='utf-8'); print(text)


if __name__ == '__main__':
    main()
