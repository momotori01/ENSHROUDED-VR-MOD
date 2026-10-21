"""Same point of the world in both eyes for display-sized images (DLSS output and after): every render pixel of the
first eye (depth and matrices of the lighting pass in <rd dir>, see rd_reproject_gi.py) is taken to the second eye,
and both display images are read at those places (scaled by display/render). Pairs: <first event>:<texture>,<second
event>:<texture> exported in <tex dir> (rd_pair_outputs.py). usage: rd_reproject_display.py <rd dir> <tex dir>"""
import json, pathlib, sys
import numpy as np
d = pathlib.Path(sys.argv[1]); t = d / 'texels'; o = pathlib.Path(sys.argv[2])
recs = [json.loads(l) for l in (t / 'texels.jsonl').read_text().splitlines()]
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
fx = (q[0] / q[3] + 1) / 2; fy = (-q[1] / q[3] + 1) / 2; bz = q[2] / q[3]
ix = np.round(fx * w - .5).astype(int); iy = np.round(fy * h - .5).astype(int)
sel = np.where((ix >= 0) & (ix < w) & (iy >= 0) & (iy < h))[0]
sel = sel[np.abs(dB[iy[sel], ix[sel]] - bz[sel]) <= .01 * np.abs(bz[sel])]
dist = np.linalg.norm(rel, axis=0)[sel]; ax = ((xs[ok][sel] + .5) / w); ay = ((ys[ok][sel] + .5) / h); bxf, byf = fx[sel], fy[sel]
ex = [json.loads(l) for l in (o / 'texels.jsonl').read_text().splitlines()]
first_event = min(x['event'] for x in ex); firsts = [x for x in ex if x['event'] == first_event]; seconds = [x for x in ex if x['event'] != first_event]
def lum(r):
    raw = (o / r['file']).read_bytes(); W, H, f = r['width'], r['height'], r['format']
    if f.startswith('R16G16B16A16_FLOAT'): v = np.frombuffer(raw, '<f2')[:W * H * 4].reshape(H, W, 4).astype(np.float64)
    elif f.startswith('R8G8B8A8'): v = np.frombuffer(raw, np.uint8)[:W * H * 4].reshape(H, W, 4).astype(np.float64) / 255
    else: return None
    return v[..., 0] * .2126 + v[..., 1] * .7152 + v[..., 2] * .0722
for ra, rb in zip(firsts, seconds):
    la, lb = lum(ra), lum(rb)
    if la is None: continue
    H, W = la.shape
    a = la[np.clip((ay * H).astype(int), 0, H - 1), np.clip((ax * W).astype(int), 0, W - 1)]
    b = lb[np.clip((byf * H).astype(int), 0, H - 1), np.clip((bxf * W).astype(int), 0, W - 1)]
    m = (dist >= 2) & (dist < 20) & np.isfinite(a) & np.isfinite(b); a, b = a[m], b[m]; r = (a - b) / np.maximum((a + b) / 2, 1e-9)
    print(f'{ra["event"]}/{rb["event"]} {ra["format"]:<20} {W}x{H} px {m.sum()} first {a.mean():.5f} second {b.mean():.5f} ratio {a.mean() / b.mean():.4f} |rel|>10% {(np.abs(r) > .1).mean():.2f}')
