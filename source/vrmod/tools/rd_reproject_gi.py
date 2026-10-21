"""Exact left/right comparison of the lighting inputs of a RenderDoc pair (texels and bindings exported by
rd_lighting_inputs.py): every pixel of the first eye goes to the world with its depth and that eye's clip-to-camera
matrix (lighting constants c11..c14, camera c15), into the second eye with its world-to-clip (c7..c10), and is kept when
the second eye's depth there agrees within 1 %. GI (R9G9B9E5 words) and the sun's shadow are compared per distance.
usage: rd_reproject_gi.py <rd dir> [y flip 0/1]"""
import json, pathlib, sys
import numpy as np
d = pathlib.Path(sys.argv[1]); t = d / 'texels'; recs = [json.loads(l) for l in (t / 'texels.jsonl').read_text().splitlines()]
WIDTH = max(r['width'] for r in recs if r['format'] == 'D32')
ev = sorted({r['event'] for r in recs}); A, B = ev[0], ev[1]
def get(event, fmt):
    r = next(r for r in recs if r['event'] == event and r['format'] == fmt and r['width'] == WIDTH)
    raw = (t / r['file']).read_bytes(); h, w = r['height'], r['width']
    if fmt == 'D32': return np.frombuffer(raw, '<f4').reshape(h, w).astype(np.float64)
    if fmt == 'R8_UNORM': return np.frombuffer(raw, np.uint8).reshape(h, w) / 255.0
    words = np.frombuffer(raw, '<u4').reshape(h, w).astype(np.uint64)
    e = ((words >> 27) & 31).astype(np.float64); s = 2.0 ** (e - 24)
    return ((words & 511) + ((words >> 9) & 511) + ((words >> 18) & 511)) * s / 3
def consts(event):
    return np.fromfile(d / f'bind-{event}' / f'event_{event}_stage_5_constant_6.bin', '<f4').reshape(-1, 4).astype(np.float64)
cA, cB = consts(A), consts(B)
dA, dB = get(A, 'D32'), get(B, 'D32'); gA, gB = get(A, 'R32_UINT'), get(B, 'R32_UINT'); sA, sB = get(A, 'R8_UNORM'), get(B, 'R8_UNORM')
h, w = dA.shape; flip = len(sys.argv) > 2 and sys.argv[2] == '1'
ys, xs = np.mgrid[0:h, 0:w]; nx = (xs + .5) / w * 2 - 1; ny = (ys + .5) / h * 2 - 1
if flip: ny = -ny
ok = dA > 0
P = np.stack([nx[ok], ny[ok], dA[ok], np.ones(ok.sum())])
rel = cA[11:15] @ P; rel = rel[:3] / rel[3]
world = rel + cA[15, :3, None]
# self check: back into the first eye
for name, pos in (('world', world), ('relative', rel)):
    q = cA[7:11] @ np.vstack([pos, np.ones(pos.shape[1])]); px = (q[0] / q[3] + 1) / 2 * w - .5
    print('self check', name, 'mean |dx| px', np.abs(px - xs[ok]).mean())
q = cB[7:11] @ np.vstack([world, np.ones(world.shape[1])])
bx = (q[0] / q[3] + 1) / 2 * w - .5; byn = q[1] / q[3]; by = ((-byn if flip else byn) + 1) / 2 * h - .5; bz = q[2] / q[3]
ix, iy = np.round(bx).astype(int), np.round(by).astype(int)
inside = (ix >= 0) & (ix < w) & (iy >= 0) & (iy < h)
dist = np.linalg.norm(rel, axis=0)
sel = np.where(inside)[0]; depthB = dB[iy[sel], ix[sel]]
agree = np.abs(depthB - bz[sel]) <= .01 * np.abs(bz[sel])
print('events', A, B, 'pixels', ok.sum(), 'inside', inside.sum(), 'depth agrees', agree.sum())
sel = sel[agree]
ga = gA[ok][sel]; gb = gB[iy[sel], ix[sel]]; sa = sA[ok][sel]; sb = sB[iy[sel], ix[sel]]; dd = dist[sel]
for lo, hi in ((0, 1), (1, 2), (2, 4), (4, 8), (8, 20), (20, 1e9)):
    m = (dd >= lo) & (dd < hi)
    if m.sum() < 50: continue
    a, b = ga[m], gb[m]; rel_ = (a - b) / np.maximum((a + b) / 2, 1e-6); sh = sa[m] - sb[m]
    print(f'{lo:>4}-{hi:<5} m px {m.sum():6d} GI first {a.mean():.4f} second {b.mean():.4f} rel {rel_.mean():+.3f} '
          f'|rel|>10% {(np.abs(rel_) > .1).mean():.2f} exact {(a == b).mean():.2f}  sun shadow diff {sh.mean():+.3f} |>0.1| {(np.abs(sh) > .1).mean():.2f}')
