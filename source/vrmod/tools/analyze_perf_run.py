"""Read-only performance summary of a live stereo run (no pass/fail; measurement aid).

Per run: submitted pairs/s and interval percentiles (openxr native_game_pair_submitted),
render-thread CPU recording time of the original view and each eye (probe log: pair start ->
calibration_eye_begin(0) -> calibration_eye_recorded(0) -> calibration_eye_recorded(1)),
eye View RenderScale (native_view_resolution_initialized) and the extents of render images the
layer saw created while the eye Views were initialized (to confirm internal resolution).
"""
import argparse, collections, json, statistics
from pathlib import Path


def rows(path):
    out = []
    for line in Path(path).read_text(encoding='utf-8-sig').splitlines():
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            pass
    return out


def pct(v, q):
    v = sorted(v)
    return v[min(len(v) - 1, int(q * (len(v) - 1) + .5))] if v else 0


def summarize(launch):
    launch = Path(launch)
    probe = sorted(launch.glob('view-resource-probe-*'))[-1]
    native = rows(probe / 'view-resource-probe.jsonl')
    xr = rows(launch / 'native-xr/openxr.jsonl')
    sub = [r['us'] for r in xr if r['event'] == 'native_game_pair_submitted']
    gaps = [(b - a) / 1000 for a, b in zip(sub, sub[1:])]
    out = dict(run=launch.name, pairs=len(sub))
    if len(sub) > 1:
        out.update(pairs_per_s=round(len(sub) / ((sub[-1] - sub[0]) / 1e6), 2), median_ms=round(statistics.median(gaps), 2), p90_ms=round(pct(gaps, .9), 2))
    # CPU recording per view on the render thread, per serial.
    by = collections.defaultdict(dict)
    pair_start = None
    for r in native:
        e = r['event']
        if e == 'live_native_frame_begin':
            pair_start = r['us']
        elif e == 'calibration_eye_begin' and r.get('eye') == 0 and pair_start:
            by[r['serial']]['orig'] = (r['us'] - pair_start) / 1000
            by[r['serial']]['b0'] = r['us']
        elif e == 'calibration_eye_recorded' and r.get('eye') in (0, 1):
            by[r['serial']]['r%d' % r['eye']] = r['us']
    orig, left, right = [], [], []
    for s in by.values():
        if {'orig', 'b0', 'r0', 'r1'} <= s.keys():
            orig.append(s['orig']); left.append((s['r0'] - s['b0']) / 1000); right.append((s['r1'] - s['r0']) / 1000)
    if orig:
        out.update(cpu_original_ms=round(statistics.median(orig), 2), cpu_left_ms=round(statistics.median(left), 2), cpu_right_ms=round(statistics.median(right), 2))
    init = [r for r in native if r['event'] == 'native_view_resolution_initialized']
    out['eye_views'] = [dict(width=r['width'], height=r['height'], render_scale=r.get('render_scale')) for r in init]
    policy = [r for r in native if r['event'] == 'eye_render_scale_policy']
    out['eye_render_scale_policy'] = policy[0]['render_scale'] if policy else None
    created = [r['us'] for r in native if r['event'] == 'native_view_created']
    vk = launch / 'vulkan.jsonl'
    if created and vk.exists():
        lo, hi = min(created), max(r['us'] for r in init) if init else max(created) + 2_000_000
        ext = collections.Counter()
        for r in rows(vk):
            if r['event'] == 'render_image' and lo <= r['us'] <= hi + 500_000:
                w, h = r.get('width'), r.get('height')
                if w and h:
                    ext[(w, h)] += 1
        out['images_created_during_eye_init'] = sorted(((w, h, n) for (w, h), n in ext.items()), key=lambda t: -t[0] * t[1])[:8]
    return out


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('launch', nargs='+', type=Path)
    a = p.parse_args()
    for l in a.launch:
        print(json.dumps(summarize(l), ensure_ascii=False))


if __name__ == '__main__':
    raise SystemExit(main())
