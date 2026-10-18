# read-only: for old in-function sites (return addresses or instruction addresses), resolve the function in the new
# build with the matcher (all methods, deep), map the instruction by alignment and show the instructions around it in
# both builds, with call targets; also the new functions that reference the given strings (--string "...").
import sys, pathlib, bisect, argparse
sys.path.insert(0, 'vrmod/tools')
from match_functions import Image, Matcher
p = argparse.ArgumentParser(); p.add_argument('sites', nargs='*'); p.add_argument('--string', action='append', default=[]); p.add_argument('--fn', action='append', default=[], help='old=new: force a function match')
a = p.parse_args()
old = Image('vrmod/output/exe-backups/af2f5a12-20260630/enshrouded.exe'); new = Image('enshrouded.exe')
cache = pathlib.Path('vrmod/output/hook-rematch-20261016'); old.analyse(cache); new.analyse(cache)
m = Matcher(old, new)
for f in a.fn:
    o, n = (int(x, 16) for x in f.split('=')); m.take(o, n, 'forced', '')
for s in a.string:
    for img, tag in ((old, 'old'), (new, 'new')):
        pos = img.raw.find(s.encode()); hits = []
        while pos != -1:
            for lo, hi, name, raw_at in img.sections:
                if raw_at <= pos < raw_at + (hi - lo):
                    rva = lo + (pos - raw_at); hits.append((rva, [hex(f) for f, _ in img.data_refs.get(rva, [])]))
            pos = img.raw.find(s.encode(), pos + 1)
        print(f'{tag} string {s!r}: ' + '; '.join(f'{r:x} referenced by {fs}' for r, fs in hits))
for site in a.sites:
    rva = int(site, 16); f = old.fn_of(rva)
    if not f:
        print(f'{rva:x}: not in a function'); continue
    lo = f[0]; m.resolve_neighbourhood(lo, 2); mt = m.resolve(lo, deep=True)
    print(f'== site {rva:x} in old fn {lo:x} ({old.features[lo]["n"]} insns) -> {mt}')
    if not mt: continue
    nlo = mt[0]; fo, fn = old.features[lo], new.features[nlo]
    k = bisect.bisect_right(fo['addrs'], rva - 1) - 1 if old.read(rva - 5, 1) == b'\xe8' else bisect.bisect_right(fo['addrs'], rva) - 1
    j, verdict = m.map_index(lo, nlo, k); print(f'   old insn #{k} -> new insn #{j} ({verdict}); new fn {nlo:x} ({fn["n"]} insns)')
    oc, nc = dict(fo['calls']), dict(fn['calls'])
    for tag, fe, idx, calls in (('old', fo, k, oc), ('new', fn, j, nc)):
        if idx is None: continue
        for i in range(max(0, idx - 4), min(fe['n'], idx + 5)):
            t = calls.get(i); tm = ''
            if t is not None and tag == 'old':
                r = m.match.get(t); tm = f'  (-> matched {r[0]:x} {r[1]})' if r else '  (callee unmatched)'
            s = fe['strict'][i] if fe.get('strict') else '?'
            print(f'   {tag} {"*" if i == idx else " "} {fe["addrs"][i]:x}: {s}' + (f'   call {t:x}{tm}' if t is not None else ''))
