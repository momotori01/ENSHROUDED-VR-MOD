"""The eyes' fog composite in a RenderDoc capture (2026-10-17 evening; the night Mock hq: the left eye's sky 7 % redder after
it, the eyes' fog volumes built apart - layer/gi_reproject.hpp fog pass). Finds the composite of each eye (the game's shader
af60aa16: the dispatch that reads the eye's two 3D fog volumes, R32_UINT the picture / 12 across) from a usage export,
exports its output colour (set 0 binding 1, RGBA16F) just after it for both eyes and compares the means of the upper 35 %
of the picture (mostly sky) and of the whole picture.

  python rd_fog_composite_pair.py <capture.rdc> <out dir>
Read-only; RenderDoc replays one at a time and never next to the game.
"""
import json, os, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
TOOL = ROOT / 'vrmod/build/bin/Release/evr_renderdoc.exe'
DLL = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/renderdoc.dll'


def run(mode, capture, out, argument=None):
    env = dict(os.environ, VK_LOADER_LAYERS_DISABLE='~implicit~')
    env.pop('VK_INSTANCE_LAYERS', None)
    args = [str(TOOL), str(DLL), mode, str(capture), str(out)] + ([argument] if argument else [])
    r = subprocess.run(args, env=env, capture_output=True, text=True)
    if r.returncode:
        raise SystemExit(f'{mode} failed {r.returncode}: {r.stdout[-400:]} {r.stderr[-400:]}')


def main():
    capture, out = Path(sys.argv[1]), Path(sys.argv[2])
    usage = out / 'usage'
    if not (usage / 'usage.jsonl').exists():
        run('usage', capture, usage)
    tex = {}
    for l in open(usage / 'textures.jsonl'):
        o = json.loads(l); tex[o['index']] = o
    actions = {}
    for l in open(usage / 'actions.jsonl'):
        o = json.loads(l); actions[o['event']] = o
    volumes = {t for t, o in tex.items() if o['name'].startswith('3D') and o['format'] == 'R32_UINT' and o['width'] == o['height'] and o['width'] < 200}
    reads = {}
    for l in open(usage / 'usage.jsonl'):
        o = json.loads(l)
        if o['texture'] in volumes and o['usage'] == 18:
            reads.setdefault(o['event'], set()).add(o['texture'])
    # the composite: a dispatch over the picture (groups of 8) reading two volumes
    composites = [e for e, ts in sorted(reads.items()) if len(ts) >= 2 and actions.get(e, {}).get('dispatch', [0])[0] > 60]
    print('composites', composites[:6])
    # each eye's first composite: the first event of each of the first two sets of volumes of the eyes' size
    wide = max(tex[t]['width'] for t in volumes)
    eyed = [e for e in composites if tex[sorted(reads[e])[0]]['width'] == wide]
    eyes = eyed[:1] + [e for e in eyed if not (reads[e] & reads[eyed[0]])][:1]
    if len(eyes) < 2:
        raise SystemExit('two eye composites not found')
    targets = []
    for e in eyes:
        d = out / f'bind-{e}'
        if not (d / 'bindings.jsonl').exists():
            run('bindings', capture, d, str(e))
        rows = [json.loads(l) for l in open(d / 'bindings.jsonl')]
        colour = [r for r in rows if r['category'] == 'resource' and r['descriptor_type'] == 7 and r.get('format') == 'R16G16B16A16_FLOAT'][0]
        targets.append(f"{e}:{colour['texture']}")
    texels = out / 'texels'
    if not (texels / 'texels.jsonl').exists():
        run('texels', capture, texels, ','.join(targets))
    recs = {json.loads(l)['event']: json.loads(l) for l in open(texels / 'texels.jsonl')}
    means = []
    for e in eyes:
        r = recs[e]; n = r['width']
        v = struct.unpack('<%de' % (n * n * 4), open(texels / r['file'], 'rb').read())
        def band(y1):
            s = [0.0, 0.0, 0.0]; c = 0
            for y in range(0, int(n * y1), 3):
                for x in range(n // 8, n - n // 8, 3):
                    i = (y * n + x) * 4; s[0] += v[i]; s[1] += v[i + 1]; s[2] += v[i + 2]; c += 1
            return [x / c for x in s]
        means.append((band(0.35), band(1.0)))
        print('eye composite', e, 'sky band', ['%.4f' % x for x in means[-1][0]], 'whole', ['%.4f' % x for x in means[-1][1]])
    for k, name in ((0, 'sky band'), (1, 'whole')):
        a, b = means[0][k], means[1][k]
        print(name, 'left / right: R %.3f G %.3f B %.3f' % tuple(x / max(y, 1e-9) for x, y in zip(a, b)))


if __name__ == '__main__':
    main()
