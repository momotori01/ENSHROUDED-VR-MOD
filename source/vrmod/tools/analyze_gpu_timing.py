"""Summarise `gpu_timing` events (launch_diagnostics -GpuTimingEvery N, 2026-10-06).

Each sampled pair holds the primary command buffers with timestamp segments:
  ["S"|"B"|"E"|"D"|..., start_ms, duration_ms, width, height, colors, draws, dispatches, pipeline, gx, gy, gz, sets]
"B" segments are render passes (render area, color attachment count); "E" segments are the work between a pass
end and the next mark; "D" segments start at a dispatch (bound compute pipeline, direct group counts, hash of the
storage resources in its descriptor sets). Older logs without dispatch marks still give pass/block totals.

Views are split at the first of every two screen-sized visibility passes (2 colour attachments): desktop, left, right.
Prints per-view totals, the largest compute blocks, the heaviest pipelines and how much dispatch work used identical
group counts / identical storage resources in all three Views. This attributes GPU time approximately (timestamps are
BOTTOM_OF_PIPE marks between commands); it is a profiling aid, not an exact per-shader cost.
"""
import argparse
import collections
import json
import statistics
from pathlib import Path

SCREEN_DEFAULT = {(742, 835), (1114, 626), (733, 395), (1920, 1080), (1264, 681)}


def load(launch):
    ev = []
    for line in (launch / 'native-xr/openxr.jsonl').read_text(encoding='utf-8', errors='replace').splitlines():
        if '"gpu_timing"' in line:
            try:
                ev.append(json.loads(line))
            except ValueError:
                pass
    # Module handles are reused after destruction: resolve each pipeline's shader hash at pipeline creation time.
    pipe2mod, mod2hash, live = {}, {}, {}
    vk = launch / 'vulkan.jsonl'
    if vk.exists():
        for line in vk.read_text(encoding='utf-8', errors='replace').splitlines():
            if '"compute_pipeline"' in line:
                r = json.loads(line); pipe2mod[int(r['pipeline'], 16)] = r['module'] + '@' + str(r['us'])
                mod2hash[pipe2mod[int(r['pipeline'], 16)]] = live.get(r['module'])
            elif '"event":"shader"' in line:
                r = json.loads(line); live[r['module']] = r['hash']
    return ev, pipe2mod, mod2hash


def split_views(segs, screen):
    view, c2 = -1, 0
    for s in segs:
        if s[0] == 'B' and (s[3], s[4]) in screen and s[5] == 2:
            if c2 % 2 == 0:
                view += 1
            c2 += 1
        yield view, s


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('launch', type=Path)
    p.add_argument('--screen', nargs='*', default=[], help='extra screen sizes WxH used to split Views')
    p.add_argument('--views', type=int, choices=(2, 3), default=3, help='Views per frame: 3 = desktop+eyes, 2 = eyes only (desktop skip, default since 2026-10-06)')
    a = p.parse_args()
    screen = set(SCREEN_DEFAULT) | {tuple(int(x) for x in s.split('x')) for s in a.screen}
    ev, pipe2mod, mod2hash = load(a.launch)
    names = ['desktop', 'left', 'right'] if a.views == 3 else ['left', 'right']
    spans, view_ms, blocks, pipes = [], collections.defaultdict(list), collections.defaultdict(list), collections.defaultdict(list)
    same_dims, same_res = collections.defaultdict(list), collections.defaultdict(list)
    for e in ev:
        segs = [s for c in e['cmds'] for s in c['segs']]
        if not segs:
            continue
        spans.append(max(s[1] + s[2] for s in segs) - min(s[1] for s in segs))
        per_view = collections.defaultdict(float)
        dims = collections.defaultdict(lambda: collections.defaultdict(float))
        res = collections.defaultdict(lambda: collections.defaultdict(float))
        frame_pipes = collections.defaultdict(float)
        frame_blocks = collections.defaultdict(float)
        for v, s in split_views(segs, screen):
            per_view[v] += s[2]
            if s[0] == 'E':
                frame_blocks[(v, s[7])] += s[2]
            if s[0] == 'D' and len(s) >= 9:
                frame_pipes[s[8]] += s[2]
                if len(s) >= 12 and s[9] > 0:
                    dims[v][(s[8], s[9], s[10], s[11])] += s[2]
                if len(s) >= 13 and s[12]:
                    res[v][(s[8], s[12])] += s[2]
        for v, ms in per_view.items():
            view_ms[v].append(ms)
        for k, ms in frame_blocks.items():
            blocks[k].append(ms)
        for k, ms in frame_pipes.items():
            pipes[k].append(ms)
        if max(per_view) >= a.views - 1:
            for table, out in ((dims, same_dims), (res, same_res)):
                common = set.intersection(*(set(table[v]) for v in range(a.views)))
                for v in range(a.views):
                    out[v].append(sum(ms for k, ms in table[v].items() if k in common))
    if not spans:
        print('no gpu_timing samples'); return 1
    print('samples %d, frame span median %.2f ms' % (len(spans), statistics.median(spans)))
    for v in sorted(view_ms):
        label = names[v] if 0 <= v < len(names) else ('before-first-view' if v < 0 else 'view%d' % v)
        extra = ''
        if v in same_dims:
            extra = ', identical group counts in all Views %.2f ms' % statistics.median(same_dims[v])
        if v in same_res and any(same_res[v]):
            extra += ', identical storage resources %.2f ms' % statistics.median(same_res[v])
        print('%-18s %.2f ms%s' % (label, statistics.median(view_ms[v]), extra))
    print('--- largest blocks between passes (view, dispatch count): median ms')
    for m, k in sorted(((statistics.median(v), k) for k, v in blocks.items()), reverse=True)[:10]:
        print('  %6.2f  %-8s x%d' % (m, names[k[0]] if 0 <= k[0] < len(names) else k[0], k[1]))
    if pipes:
        print('--- heaviest compute pipelines (all Views): median ms per sampled pair')
        for m, k in sorted(((statistics.median(v), k) for k, v in pipes.items()), reverse=True)[:12]:
            print('  %6.2f  pipeline %x shader %s' % (m, k, mod2hash.get(pipe2mod.get(k))))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
