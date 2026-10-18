"""Left/right comparison of what the lighting compute of each eye reads (2026-10-17, headset reports 3/5/16/20:
"the darkness of the shadows differs between the eyes, or one eye has no shadow").

1.0's deferred lighting (one compute a View, 131x131 groups at render scale 0.8; RenderDoc eo 6510 / 10485, HLSL in
DeleteMe/20261017/light6510.hlsl) multiplies the sun's light by two shadow terms and adds the indirect light:
  binding 12  R8_UNORM, screen space   the sun's shadow (from the cascades and the contact shadows)
  binding 7   R16_UNORM 3D, world      a second shadow term, sampled at (position + camera) * scale + offset
  binding 15  R9G9B9E5, screen space   the indirect (GI) irradiance
  binding 0   RGBA16F 3072x2048        sampled once (_9)
Each eye has its own of these four. This script finds the two lighting dispatches (the 5th 131x131 dispatch of each
eye's 19), exports those inputs at each dispatch and compares the eyes. Screen-space terms are compared on pixels whose
depth says far away (parallax below a pixel), the world-space volume as a whole.

  python rd_lighting_inputs.py <capture.rdc> <out dir> [index of the lighting among an eye's 131x131 dispatches, 4]
Read-only; RenderDoc replays one at a time and never next to the game.
"""
import json, math, os, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'vrmod/build/bin/Release/evr_renderdoc.exe'
DLL = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/renderdoc.dll'


def run(mode, capture, out, argument):
    env = dict(os.environ, VK_LOADER_LAYERS_DISABLE='~implicit~')
    env.pop('VK_INSTANCE_LAYERS', None)
    r = subprocess.run([str(TOOL), str(DLL), mode, str(capture), str(out), argument], env=env, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f'{mode} failed {r.returncode}: {r.stdout[-400:]} {r.stderr[-400:]}')
    return r.stdout


def lighting_events(actions, index=4):
    # index: the lighting's place among each eye's 131x131 dispatches (4; 5 with layer/gi_reproject.hpp, whose own pass
    # of the same size comes just before it)
    groups = [a['event'] for a in actions if a['dispatch'] == [131, 131, 1]]
    if len(groups) % 2:
        raise SystemExit(f'odd number of 131x131 dispatches: {len(groups)}')
    half = len(groups) // 2
    return groups[index], groups[half + index], groups


def bindings(capture, out, event):
    d = out / f'bind-{event}'
    if not (d / 'bindings.jsonl').exists():
        run('bindings', capture, d, str(event))
    rows = [json.loads(l) for l in (d / 'bindings.jsonl').read_text().splitlines()]
    return {(r['category'], r.get('descriptor_type'), r['index']): r for r in rows}


def e9(word):
    exp = (word >> 27) & 31
    scale = 2.0 ** (exp - 15 - 9)
    return ((word & 511) * scale, ((word >> 9) & 511) * scale, ((word >> 18) & 511) * scale)


def load(record, directory, shared_exponent=False):
    data = (directory / record['file']).read_bytes()
    w, h, d = record['width'], record['height'], record['depth']
    fmt = record['format']
    # the GI image is made R32_UINT and read through an R9G9B9E5 view: RenderDoc exports its words
    if (record.get('packed_32') and 'E5' in fmt) or (shared_exponent and fmt == 'R32_UINT'):
        words = struct.unpack(f'<{w*h*d}I', data)
        return w, h, d, [sum(e9(x)) / 3 for x in words]
    if fmt.startswith('R8_') or fmt == 'R8_UNORM':
        return w, h, d, [b / 255 for b in data]
    if fmt.startswith('R16_'):
        return w, h, d, [v / 65535 for v in struct.unpack(f'<{w*h*d}H', data)]
    if fmt == 'D32':
        return w, h, d, list(struct.unpack(f'<{w*h*d}f', data))
    if fmt.startswith('R16G16B16A16_FLOAT'):
        v = struct.unpack(f'<{w*h*d*4}e', data)
        return w, h, d, [(v[i] + v[i + 1] + v[i + 2]) / 3 for i in range(0, len(v), 4)]
    raise SystemExit('no decoder for ' + fmt)


def main():
    capture, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    probe = out / 'actions'
    if not (probe / 'actions.jsonl').exists():
        try:
            run('bindings', capture, probe, '1')
        except SystemExit:
            pass
    actions = [json.loads(l) for l in (probe / 'actions.jsonl').read_text().splitlines()]
    left, right, groups = lighting_events(actions, int(sys.argv[3]) if len(sys.argv) > 3 else 4)
    print('lighting dispatches', left, right, 'of', len(groups))
    bl, br = bindings(capture, out, left), bindings(capture, out, right)
    picks = {'sun_shadow': 12, 'shadow_volume': 7, 'gi': 15, 'depth': 16, 'atlas': 0}
    selection, names = [], []
    for name, index in picks.items():
        a, b = bl[('resource', 4, index)], br[('resource', 4, index)]
        print(name, a['texture'], a['format'], a.get('width'), a.get('height'), '|', b['texture'], 'same' if a['texture'] == b['texture'] else 'own')
        selection += [f"{left}:{a['texture']}", f"{right}:{b['texture']}"]
        names += [(name, 'L'), (name, 'R')]
    tex = out / 'texels'
    if not (tex / 'texels.jsonl').exists():
        run('texels', capture, tex, ','.join(selection))
    records = [json.loads(l) for l in (tex / 'texels.jsonl').read_text().splitlines()]
    images = {}
    for (name, eye), rec in zip(names, records):
        images[(name, eye)] = load(rec, tex, name == 'gi')
    # far pixels: reversed depth (0 far); parallax below a pixel beyond ~60 m at this field of view - take depth < 0.0015
    w, h, _, dl = images[('depth', 'L')]
    _, _, _, dr = images[('depth', 'R')]
    far = [i for i in range(w * h) if 0 < dl[i] < 0.0015 and 0 < dr[i] < 0.0015]
    print('far pixels', len(far), 'of', w * h)
    report = {'left_event': left, 'right_event': right, 'far_pixels': len(far)}
    for name in ('sun_shadow', 'gi'):
        _, _, _, a = images[(name, 'L')]
        _, _, _, b = images[(name, 'R')]
        if len(a) != w * h:
            print(name, 'size differs from depth', len(a))
            continue
        diffs = sorted(abs(a[i] - b[i]) for i in far)
        mean_l = sum(a[i] for i in far) / max(1, len(far))
        mean_r = sum(b[i] for i in far) / max(1, len(far))
        big = sum(1 for x in diffs if x > 0.1 * max(1e-6, (mean_l + mean_r) / 2))
        report[name] = dict(mean_left=mean_l, mean_right=mean_r, median_abs=diffs[len(diffs) // 2] if diffs else None,
                            p90_abs=diffs[int(len(diffs) * .9)] if diffs else None, share_over_10pct=big / max(1, len(diffs)))
        print(name, report[name])
    for name in ('shadow_volume', 'atlas'):
        wl, hl, dl_, a = images[(name, 'L')]
        _, _, _, b = images[(name, 'R')]
        n = min(len(a), len(b))
        diffs = sorted(abs(a[i] - b[i]) for i in range(n))
        report[name] = dict(size=[wl, hl, dl_], mean_left=sum(a) / len(a), mean_right=sum(b) / len(b), median_abs=diffs[n // 2],
                            p90_abs=diffs[int(n * .9)], share_differs=sum(1 for x in diffs if x > 1e-3) / n)
        print(name, report[name])
    (out / 'report.json').write_text(json.dumps(report, indent=1))


if __name__ == '__main__':
    main()
