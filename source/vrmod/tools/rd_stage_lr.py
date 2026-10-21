"""Which stage after the deferred lighting makes the eyes differ: for each of an eye's 73x84 dispatches from the
lighting on (positions <from>..<to> among the eye's 20), the screen-sized storage images it writes (RGBA16F,
R11G11B10, R16F) are exported at that dispatch in both eyes and compared at the same point of the world (depth and
matrices of the lighting pass in <rd dir>, see rd_reproject_gi.py): mean luminance of each eye, mean signed relative
difference and the share of pixels more than 10 % apart, per distance band 2..20 m.
usage: rd_stage_lr.py <capture.rdc> <rd dir> <from> <to>      (RenderDoc replays never next to the game)"""
import json, os, pathlib, subprocess, sys
import numpy as np
ROOT = pathlib.Path(r'S:/SteamLibrary/steamapps/common/Enshrouded')
TOOL = ROOT / 'vrmod/build/bin/Release/evr_renderdoc.exe'; DLL = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/renderdoc.dll'
def run(mode, capture, out, arg):
    env = dict(os.environ, VK_LOADER_LAYERS_DISABLE='~implicit~'); env.pop('VK_INSTANCE_LAYERS', None)
    r = subprocess.run([str(TOOL), str(DLL), mode, str(capture), str(out), arg], env=env, capture_output=True, text=True)
    return r.returncode, r.stdout[-300:]
cap, d, lo, hi = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4])
acts = [json.loads(l) for l in (d / 'actions' / 'actions.jsonl').read_text().splitlines()]
import collections
DIMS = list(collections.Counter(tuple(a['dispatch']) for a in acts if a['dispatch'][0] >= 40 and a['dispatch'][1] >= 40 and a['dispatch'][2] == 1).most_common(1)[0][0])
groups = [a['event'] for a in acts if a['dispatch'] == DIMS]; half = len(groups) // 2
t = d / 'texels'; recs = [json.loads(l) for l in (t / 'texels.jsonl').read_text().splitlines()]
WIDTH = max(r['width'] for r in recs if r['format'] == 'D32')
ev = sorted({r['event'] for r in recs}); A, B = ev[0], ev[1]
def depth(event):
    r = next(r for r in recs if r['event'] == event and r['format'] == 'D32' and r['width'] == WIDTH)
    return np.frombuffer((t / r['file']).read_bytes(), '<f4').reshape(r['height'], r['width']).astype(np.float64)
cA, cB = [np.fromfile(d / f'bind-{e}' / f'event_{e}_stage_5_constant_6.bin', '<f4').reshape(-1, 4).astype(np.float64) for e in (A, B)]
dA, dB = depth(A), depth(B); h, w = dA.shape
ys, xs = np.mgrid[0:h, 0:w]; nx = (xs + .5) / w * 2 - 1; ny = -((ys + .5) / h * 2 - 1); ok = dA > 0
P = np.stack([nx[ok], ny[ok], dA[ok], np.ones(ok.sum())]); rel = cA[11:15] @ P; rel = rel[:3] / rel[3]; world = rel + cA[15, :3, None]
q = cB[7:11] @ np.vstack([world, np.ones(world.shape[1])])
ix = np.round((q[0] / q[3] + 1) / 2 * w - .5).astype(int); iy = np.round((-q[1] / q[3] + 1) / 2 * h - .5).astype(int); bz = q[2] / q[3]
sel = np.where((ix >= 0) & (ix < w) & (iy >= 0) & (iy < h))[0]
sel = sel[np.abs(dB[iy[sel], ix[sel]] - bz[sel]) <= .01 * np.abs(bz[sel])]
dist = np.linalg.norm(rel, axis=0)[sel]; band = (dist >= 2) & (dist < 20)
def decode(r, raw):
    f = r['format']; n = r['width'] * r['height']
    if f.startswith('R16G16B16A16_FLOAT'):
        v = np.frombuffer(raw, '<f2').reshape(r['height'], r['width'], 4).astype(np.float64); return v[..., 0] * .2126 + v[..., 1] * .7152 + v[..., 2] * .0722
    if f.startswith('R16_FLOAT'): return np.frombuffer(raw, '<f2')[:n].reshape(r['height'], r['width']).astype(np.float64)
    if f.startswith('R11G11B10'):
        u = np.frombuffer(raw, '<u4')[:n].reshape(r['height'], r['width']).astype(np.uint64)
        def f11(x, mb):
            e = (x >> mb) & 31; m = x & ((1 << mb) - 1); return np.where(e == 0, m / (1 << mb) * 2.0 ** -14, (1 + m / (1 << mb)) * 2.0 ** (e.astype(np.float64) - 15))
        r_, g_, b_ = f11(u & 2047, 6), f11((u >> 11) & 2047, 6), f11((u >> 22) & 1023, 5)
        return r_ * .2126 + g_ * .7152 + b_ * .0722
    return None
for k in range(lo, hi + 1):
    ea, eb = groups[k], groups[half + k]; out = d / f'stage-{k}'
    rows = {}
    for e in (ea, eb):
        bd = out / f'bind-{e}'
        if not (bd / 'bindings.jsonl').exists(): run('bindings', cap, bd, str(e))
        rows[e] = [json.loads(l) for l in (bd / 'bindings.jsonl').read_text().splitlines()]
    outs = lambda e: [r for r in rows[e] if r['category'] == 'resource' and r.get('descriptor_type') == 7 and r.get('width') == w and r.get('height') == h]
    oa, ob = outs(ea), outs(eb)
    if not oa or len(oa) != len(ob): print(k, ea, eb, 'outputs', len(oa), len(ob)); continue
    tx = out / 'texels'
    if not (tx / 'texels.jsonl').exists():
        run('texels', cap, tx, ','.join([f"{ea}:{r['texture']}" for r in oa] + [f"{eb}:{r['texture']}" for r in ob]))
    if not (tx / 'texels.jsonl').exists(): print(k, 'export failed'); continue
    ex = [json.loads(l) for l in (tx / 'texels.jsonl').read_text().splitlines()]
    for ra, rb in zip([x for x in ex if x['event'] == ea], [x for x in ex if x['event'] == eb]):
        ia, ib = decode(ra, (tx / ra['file']).read_bytes()), decode(rb, (tx / rb['file']).read_bytes())
        if ia is None: print(k, ea, eb, ra['format'], 'not decoded'); continue
        a = ia[ok][sel][band]; b = ib[iy[sel], ix[sel]][band]; m = np.isfinite(a) & np.isfinite(b); a, b = a[m], b[m]
        r = (a - b) / np.maximum(np.abs(a + b) / 2, 1e-6)
        print(f'stage {k} events {ea}/{eb} {ra["format"]:<22} first {a.mean():.5f} second {b.mean():.5f} ratio {a.mean() / max(b.mean(), 1e-9):.3f} |rel|>10% {(np.abs(r) > .1).mean():.2f}')
