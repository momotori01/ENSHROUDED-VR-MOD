"""The eyes' fog volumes in a RenderDoc capture, compared cell by cell (2026-10-17 evening, layer/gi_reproject.hpp fog pass).
After rd_fog_composite_pair.py (its out dir: the two composites' bindings): exports the two 3D volumes (bindings 10 and
11 of the composite, R9G9B9E5 words) of each eye at its composite and compares them at the same cell, per group of depth
slices: the far slices have almost no parallax, so with the fog pass the second eye's volumes there are the first eye's
(ratio 1), without it each eye's own (the night Mock hq: 0.3 .. 1.5).

  python rd_fog_volume_pair.py <capture.rdc> <rd_fog_composite_pair out dir>
"""
import json, os, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'vrmod/build/bin/Release/evr_renderdoc.exe'
DLL = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/renderdoc.dll'


def e5(w):
    s = 2.0 ** (((w >> 27) & 31) - 24)
    return ((w & 511) * s, ((w >> 9) & 511) * s, ((w >> 18) & 511) * s)


def main():
    capture, out = Path(sys.argv[1]), Path(sys.argv[2])
    binds = sorted(d for d in out.iterdir() if d.name.startswith('bind-'))
    events = sorted(int(d.name[5:]) for d in binds)[:2]
    sel = []
    for e in events:
        rows = [json.loads(l) for l in open(out / f'bind-{e}' / 'bindings.jsonl')]
        vols = [r for r in rows if r['category'] == 'resource' and r['descriptor_type'] == 4 and r['index'] in (4, 5)]
        sel += [f"{e}:{r['texture']}" for r in sorted(vols, key=lambda r: r['index'])]
    vdir = out / 'volumes'
    if not (vdir / 'texels.jsonl').exists():
        env = dict(os.environ, VK_LOADER_LAYERS_DISABLE='~implicit~'); env.pop('VK_INSTANCE_LAYERS', None)
        r = subprocess.run([str(TOOL), str(DLL), 'texels', str(capture), str(vdir), ','.join(sel)], env=env, capture_output=True, text=True)
        if r.returncode:
            raise SystemExit(r.stdout[-400:] + r.stderr[-400:])
    recs = [json.loads(l) for l in open(vdir / 'texels.jsonl')]
    by = {}
    for r in recs:
        by.setdefault(r['event'], []).append(r)
    (la, lb), (ra, rb) = [sorted(by[e], key=lambda r: r['texture']) for e in events]
    for name, a, b in (('scattered in', la, ra), ('passing through', lb, rb)):
        W, H, D = a['width'], a['height'], a['depth']
        va = struct.unpack('<%dI' % (W * H * D), open(vdir / a['file'], 'rb').read())
        vb = struct.unpack('<%dI' % (W * H * D), open(vdir / b['file'], 'rb').read())
        rows = []
        for z0 in range(0, D, D // 8):
            sa = [0.0] * 3; sb = [0.0] * 3; same = 0; n = 0
            for z in range(z0, z0 + D // 8):
                for i in range(z * W * H + W // 8, (z + 1) * W * H - W // 8, 3):
                    x, y = e5(va[i]), e5(vb[i])
                    for k in range(3): sa[k] += x[k]; sb[k] += y[k]
                    same += va[i] == vb[i]; n += 1
            ratio = sa[0] / sb[0] if sb[0] else float('nan')
            rows.append('z%d-%d R %.3f same %.2f' % (z0, z0 + D // 8 - 1, ratio, same / n))
        print(name, '(%dx%dx%d)' % (W, H, D), ' | '.join(rows))


if __name__ == '__main__':
    main()
