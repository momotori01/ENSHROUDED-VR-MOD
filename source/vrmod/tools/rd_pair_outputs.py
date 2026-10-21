"""Whole-image mean of every storage image written by given dispatches in both eyes (the second eye's event = the
first's + offset): for the stages after the last 73x84 dispatch (DLSS, the exposure pyramid), where the eyes see the
same room and a few % of mean is the question. usage: rd_pair_outputs.py <capture.rdc> <out dir> <offset> <event>..."""
import json, os, pathlib, subprocess, sys
import numpy as np
ROOT = pathlib.Path(r'S:/SteamLibrary/steamapps/common/Enshrouded')
TOOL = ROOT / 'vrmod/build/bin/Release/evr_renderdoc.exe'; DLL = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/renderdoc.dll'
def run(mode, capture, out, arg):
    env = dict(os.environ, VK_LOADER_LAYERS_DISABLE='~implicit~'); env.pop('VK_INSTANCE_LAYERS', None)
    return subprocess.run([str(TOOL), str(DLL), mode, str(capture), str(out), arg], env=env, capture_output=True, text=True).returncode
cap, out, off = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), int(sys.argv[3])
def half(raw, n): return np.frombuffer(raw, '<f2')[:n].astype(np.float64)
def mean_of(r, raw):
    f, w, h = r['format'], r['width'], r['height']; n = w * h
    if f.startswith('R16G16B16A16_FLOAT'): v = half(raw, n * 4).reshape(-1, 4); return (v[:, 0] * .2126 + v[:, 1] * .7152 + v[:, 2] * .0722).mean(), f'{w}x{h}'
    if f.startswith('R32G32B32A32_FLOAT'): v = np.frombuffer(raw, '<f4')[:n * 4].reshape(-1, 4).astype(np.float64); return (v[:, 0] * .2126 + v[:, 1] * .7152 + v[:, 2] * .0722).mean(), f'{w}x{h}'
    if f.startswith('R16_FLOAT'): return half(raw, n).mean(), f'{w}x{h}'
    if f.startswith('R32_FLOAT'): return np.frombuffer(raw, '<f4')[:n].astype(np.float64).mean(), f'{w}x{h}'
    if f.startswith('R16G16_FLOAT'): v = half(raw, n * 2).reshape(-1, 2); return v[:, 0].mean(), f'{w}x{h} (r)'
    if f.startswith('R11G11B10'):
        u = np.frombuffer(raw, '<u4')[:n].astype(np.uint64)
        def f11(x, mb):
            e = (x >> mb) & 31; m = x & ((1 << mb) - 1); return np.where(e == 0, m / (1 << mb) * 2.0 ** -14, (1 + m / (1 << mb)) * 2.0 ** (e.astype(np.float64) - 15))
        return (f11(u & 2047, 6) * .2126 + f11((u >> 11) & 2047, 6) * .7152 + f11((u >> 22) & 1023, 5) * .0722).mean(), f'{w}x{h}'
    if f.startswith('R8G8B8A8_UNORM') or f.startswith('B8G8R8A8_UNORM') or f.startswith('R8G8B8A8_SRGB'):
        v = np.frombuffer(raw, np.uint8)[:n * 4].reshape(-1, 4).astype(np.float64) / 255; return v[:, :3].mean(), f'{w}x{h}'
    return None, f
for a in map(int, sys.argv[4:]):
    b = a + off; rows = {}
    for e in (a, b):
        bd = out / f'bind-{e}'
        if not (bd / 'bindings.jsonl').exists(): run('bindings', cap, bd, str(e))
        rows[e] = [json.loads(l) for l in (bd / 'bindings.jsonl').read_text().splitlines()] if (bd / 'bindings.jsonl').exists() else []
    oa = [r for r in rows[a] if r['category'] == 'resource' and r.get('descriptor_type') == 7 and r.get('texture', -1) >= 0]
    ob = [r for r in rows[b] if r['category'] == 'resource' and r.get('descriptor_type') == 7 and r.get('texture', -1) >= 0]
    if not oa or len(oa) != len(ob): print(a, b, 'outputs', len(oa), len(ob)); continue
    tx = out / f'tex-{a}'
    if not (tx / 'texels.jsonl').exists(): run('texels', cap, tx, ','.join([f"{a}:{r['texture']}" for r in oa] + [f"{b}:{r['texture']}" for r in ob]))
    if not (tx / 'texels.jsonl').exists(): print(a, b, 'export failed'); continue
    ex = [json.loads(l) for l in (tx / 'texels.jsonl').read_text().splitlines()]
    for ra, rb in zip([x for x in ex if x['event'] == a], [x for x in ex if x['event'] == b]):
        ma, s = mean_of(ra, (tx / ra['file']).read_bytes()); mb, _ = mean_of(rb, (tx / rb['file']).read_bytes())
        if ma is None: print(a, b, ra['format'], 'not decoded'); continue
        print(f'{a}/{b} {ra["format"]:<22} {s:<12} first {ma:.6g} second {mb:.6g} ratio {ma / mb if mb else float("nan"):.4f}')
