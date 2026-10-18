#!/usr/bin/env python3
"""Carry the hooks' addresses from one game build to a build that differs a little (a hotfix), from the cached features
of both builds (match_functions.py, cache-<sha8>.pkl): the old executable itself is not needed (2026-10-16: 1.0 ->
Hotfix 1.0.0.1; Steam replaced the 1.0 executable before a copy of it was kept, its features were in the cache).

    shift_rematch.py --old-cache <cache-old.pkl> --new enshrouded.exe --cache-dir <dir> [--write] [--report out.json]

Functions are paired where their strict shape hash is unique in both builds and the same; then, between two such pairs
(in address order, both sides increasing), the functions in the gap when there are as many on both sides and each pair
has the same instruction count. An address is carried by its function: to the same instruction index, and the same
distance into that instruction (a call's rel32). A data address goes with the instruction that reads it (rip-relative).
The literals looked at are the code's hex numbers of 0x10000 and more, except a number after a name and a plus (an
offset into a structure, unless the name is baseAddress / base / image / module) and the layout constants (size_t).
Without --write nothing is changed; the list says what would be, and what needs a look: read it before --write (a
number that only looks like an address - a size, an offset - is moved too unless a rule above leaves it).
"""
import argparse, bisect, json, pathlib, pickle, re, sys

sys.path.insert(0, 'vrmod/tools')
from match_functions import Image  # noqa: E402

LITERAL = re.compile(r'0x([0-9A-Fa-f]{5,7})\b')
BASES = ('baseAddress', 'base', 'image', 'module', 'imageBase')
NOT_ADDRESS = re.compile(r'(offset|size|bytes|stride|capacity|count)\w*\s*=\s*$|buffer\([^()]*$', re.I)


class Side:
    def __init__(self, functions, features):
        self.functions = sorted(functions)
        self.starts = [lo for lo, _ in self.functions]
        self.features = features
        self.data_refs = {}
        for lo, fe in features.items():
            for k, target in fe.get('rips', []):
                self.data_refs.setdefault(target, []).append((lo, k))

    def fn_of(self, v):
        i = bisect.bisect_right(self.starts, v) - 1
        if i >= 0 and self.functions[i][0] <= v < self.functions[i][1]:
            return self.functions[i]
        return None


def pair_functions(old, new):
    def by_hash(side):
        out = {}
        for lo, fe in side.features.items():
            out.setdefault(fe['h'][0], []).append(lo)
        return out
    oh, nh = by_hash(old), by_hash(new)
    anchors = sorted((los[0], nh[h][0]) for h, los in oh.items() if len(los) == 1 and len(nh.get(h, [])) == 1)
    # keep the anchors increasing on both sides (longest increasing run by the new address, greedy)
    kept, last = [], -1
    for o, n in anchors:
        if n > last:
            kept.append((o, n)); last = n
    pairs = dict(kept)
    # the gaps between anchors
    for (o1, n1), (o2, n2) in zip(kept, kept[1:]):
        i1, i2 = bisect.bisect_right(old.starts, o1), bisect.bisect_left(old.starts, o2)
        j1, j2 = bisect.bisect_right(new.starts, n1), bisect.bisect_left(new.starts, n2)
        olds, news = old.starts[i1:i2], new.starts[j1:j2]
        if len(olds) == len(news) and all(old.features[a]['n'] == new.features[b]['n'] for a, b in zip(olds, news)):
            pairs.update(zip(olds, news))
    return pairs, len(kept)


def region_delta(v, old, pairs, paired_starts):
    # the shift of the nearest paired function at or before v, when the next paired one shifts by the same amount
    i = bisect.bisect_right(paired_starts, v) - 1
    if i < 0 or i + 1 >= len(paired_starts):
        return None
    before, after = paired_starts[i], paired_starts[i + 1]
    d1, d2 = pairs[before] - before, pairs[after] - after
    return d1 if d1 == d2 else None


def carry(v, old, new, pairs, paired_starts):
    f = old.fn_of(v)
    if f and f[0] in pairs:
        lo = f[0]
        nlo = pairs[lo]
        oa, na = old.features[lo]['addrs'], new.features[nlo]['addrs']
        k = bisect.bisect_right(oa, v) - 1
        if k < 0 or k >= len(na):
            return None, 'instruction outside the paired function'
        same = old.features[lo]['n'] == new.features[nlo]['n']
        return na[k] + (v - oa[k]), 'function' + ('' if same else ' (instruction counts differ)')
    if f is None or f[0] not in pairs:
        d = region_delta(v, old, pairs, paired_starts)
        if d is not None and (f is None or old.data_refs.get(v) is None):
            return v + d, 'region shift' + (' (its function unpaired)' if f else ' (no function record)')
    refs = old.data_refs.get(v)
    if refs:
        for lo, k in refs:
            if lo in pairs:
                nr = [t for kk, t in new.features[pairs[lo]].get('rips', []) if kk == k]
                if nr:
                    return nr[0], 'data via its reader'
        return None, 'data reader unpaired'
    return None, 'not in a function and no reader'


def literals(path):
    text = path.read_text(encoding='utf-8', errors='replace')
    for n, line in enumerate(text.splitlines(), 1):
        code = line.split('//')[0] if not line.lstrip().startswith('//') else ''
        if not code or 'constexpr size_t' in code:
            continue
        for m in LITERAL.finditer(code):
            v = int(m.group(1), 16)
            if v < 0x10000:
                continue
            before = code[:m.start()].rstrip()
            # 2026-10-16: a structure offset, a size or a subtracted offset is not an address (senderContextOffset=0x47d4e0,
            # buffer(pair,own,0x38,0x60000), block-0x50980 were moved by 0x60 in the first write and put back by hand)
            if NOT_ADDRESS.search(before[-32:]) or before.endswith('-'):
                continue
            if before.endswith('+'):
                name = re.search(r'([A-Za-z_][A-Za-z0-9_:]*)\s*\+$', before)
                if not name or name.group(1).split('::')[-1] not in BASES:
                    continue
            yield n, v, line.strip()


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--old-cache', required=True, type=pathlib.Path)
    p.add_argument('--new', required=True, type=pathlib.Path)
    p.add_argument('--cache-dir', required=True, type=pathlib.Path)
    p.add_argument('--folders', nargs='+', default=['vrmod/hooks', 'vrmod/layer', 'vrmod/include/evr', 'vrmod/tests'])
    p.add_argument('--report', type=pathlib.Path)
    p.add_argument('--write', action='store_true')
    p.add_argument('--keep', nargs='*', default=[], help='values that are not addresses (masks, limits, offsets), as hex')
    p.add_argument('--skip-files', nargs='*', default=[], help='file names whose numbers are not addresses')
    a = p.parse_args()
    saved = pickle.load(open(a.old_cache, 'rb'))
    old = Side(saved['functions'], saved['features'])
    img = Image(a.new); img.analyse(a.cache_dir)
    new = Side(img.functions, img.features)
    pairs, anchors = pair_functions(old, new)
    paired_starts = sorted(pairs)
    keep = {int(x, 16) for x in a.keep}
    print(f'functions: old {len(old.functions)}, new {len(new.functions)}, anchors {anchors}, paired {len(pairs)}', file=sys.stderr)
    rows, edits = [], {}
    for folder in a.folders:
        for f in sorted(pathlib.Path(folder).glob('*.[hc]pp')):
            if f.name in a.skip_files:
                continue
            for n, v, line in literals(f):
                if v in keep:
                    continue
                nv, how = carry(v, old, new, pairs, paired_starts)
                rows.append(dict(file=str(f), line=n, old=hex(v), new=hex(nv) if nv is not None else None, how=how, text=line[:160]))
                if nv is not None and nv != v:
                    edits.setdefault(str(f), {}).setdefault(n, {})[v] = nv
    for r in rows:
        print(f"{r['file']}:{r['line']} {r['old']} -> {r['new']} ({r['how']})")
    if a.report:
        a.report.write_text(json.dumps(rows, indent=1), encoding='utf-8')
    if a.write:
        for name, lines in edits.items():
            path = pathlib.Path(name); raw = path.read_bytes(); bom = raw.startswith(b'\xef\xbb\xbf')
            text = raw[3:].decode('utf-8') if bom else raw.decode('utf-8')
            out = text.split('\n')
            for n, values in lines.items():
                code, sep, comment = out[n - 1].partition('//')
                for v, nv in values.items():
                    code = re.sub(r'0x0*%x\b' % v, '0x%x' % nv, code, flags=re.I)
                out[n - 1] = code + sep + comment
            data = '\n'.join(out).encode('utf-8')
            path.write_bytes((b'\xef\xbb\xbf' if bom else b'') + data)
        print(f'written: {sum(len(v) for v in edits.values())} lines in {len(edits)} files', file=sys.stderr)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
