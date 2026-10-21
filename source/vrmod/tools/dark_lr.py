"""Left/right brightness per block for dark scenes (eye_shade_survey's method in numpy, with a lower darkness limit):
half size, 24 x 24 blocks of the left eye searched along the same rows of the right (shifts -28..+4), blocks with a mean
of at least 2 (0..255) and some texture, the outer 12 % left out. Per burst: blocks, share over 10 / 20 %, mean signed
(left - right) / mean, and a picture of the blocks (red: left brighter, blue: right) for the first pair.
usage: dark_lr.py <game folder> [sheet.png]"""
import pathlib, re, sys
import numpy as np
def load(p):
    b = p.read_bytes(); parts = b.split(b'\n', 3); w, h = map(int, parts[1].split())
    a = np.frombuffer(parts[3][:w * h * 3], np.uint8).reshape(h, w, 3).astype(np.float32).mean(axis=2)
    a = (a[0::2, 0::2] + a[1::2, 0::2] + a[0::2, 1::2] + a[1::2, 1::2]) / 4
    return a
B, SH = 24, range(-28, 5)
def blocks(l, r):
    h, w = l.shape; m = int(w * .12); out = []
    for y in range(m, h - m - B + 1, B):
        for x in range(m, w - m - B + 1, B):
            bl = l[y:y + B, x:x + B]
            if bl.std() < 1.5 or bl.mean() < 2: continue
            best = None
            for s in SH:
                if not 0 <= x + s <= w - B: continue
                d = np.abs(bl - r[y:y + B, x + s:x + s + B]).mean()
                if best is None or d < best[0]: best = (d, s)
            if best is None or best[1] < -8: continue
            la, lb = bl.mean(), r[y:y + B, x + best[1]:x + best[1] + B].mean()
            out.append((x, y, (la - lb) / ((la + lb) / 2)))
    return out
g = pathlib.Path(sys.argv[1]) / 'native-xr'
frames = sorted({int(m.group(1)) for p in g.glob('frame-*-eye-0.ppm') for m in [re.match(r'frame-(\d+)-eye-0', p.name)] if m})
groups = []
for f in frames:
    if groups and f - groups[-1][-1] <= 5: groups[-1].append(f)
    else: groups.append([f])
first = None
for gr in groups:
    allv = []
    for f in gr[::4]:
        found = blocks(load(g / f'frame-{f}-eye-0.ppm'), load(g / f'frame-{f}-eye-1.ppm'))
        if first is None and found: first = (f, found)
        allv += [v for _, _, v in found]
    if allv:
        a = np.abs(allv); print(f'frames {gr[0]}..{gr[-1]}: blocks {len(allv)} over10 {(a > .1).mean():.3f} over20 {(a > .2).mean():.3f} mean signed {np.mean(allv):+.3f}')
if first and len(sys.argv) > 2:
    f, found = first; l = load(g / f'frame-{f}-eye-0.ppm'); h, w = l.shape
    img = np.zeros((h, w, 3), np.uint8); img[..., :] = np.clip(l, 0, 255)[..., None].astype(np.uint8)
    for x, y, v in found:
        c = np.array([255, 0, 0] if v > 0 else [0, 80, 255], np.float32); k = min(abs(v) / .3, 1)
        img[y:y + B, x:x + B] = (img[y:y + B, x:x + B] * (1 - k) + c * k).astype(np.uint8)
    hdr = f'P6\n{w} {h}\n255\n'.encode(); pathlib.Path(sys.argv[2]).write_bytes(hdr + img.tobytes()); print('sheet', sys.argv[2], 'frame', f)
