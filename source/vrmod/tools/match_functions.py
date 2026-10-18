#!/usr/bin/env python3
"""Where the hooks' addresses went in a new build of the game (read-only; 2026-10-16, the 1.0 update: every fixed RVA
of the hooks moved, and the byte patterns of find_hook_sites.py found 7 of 58).

Both executables are taken apart by their unwind tables (.pdata gives every function's range) and capstone. Each
function gets three shapes of its instruction list: strict (mnemonics, registers, displacements, small immediates;
only the rip-relative and call/jump targets masked), medium (displacements and immediates masked too: a changed
structure layout keeps the shape) and loose (mnemonics and operand kinds only). A function of the old build is looked
for in the new one by, in turn: the same strict shape (one function only), the same medium shape, the same loose shape,
the same rare strings referenced, then the call graph (the matched callers' callees and the matched callees' callers)
ranked by the similarity of the loose instruction 4-grams. A site inside a function (a caller's return address, a
hook in the middle of a function) is mapped by aligning the old and new instruction lists (difflib); a call site is
checked against the matched callee. A data address is mapped through a matched function that refers to it.

    match_functions.py --old <old.exe> --new <new.exe> --scan-hooks vrmod/hooks [--site name=0x...]... --report <json>

The report lists every site with the method and the doubts; for a matched function the displacements that changed
(structure offsets) and a byte pattern of the new function that is unique in the new build. The features of each
executable are cached beside the report (cache-<sha8>.pkl) because taking one apart takes a minute.
"""
import argparse, array, bisect, difflib, hashlib, json, pathlib, pickle, re, sys, time

sys.path.insert(0, 'vrmod/tools/python')
import pefile, capstone  # noqa: E402
from capstone import x86  # noqa: E402

TOKENS = {}   # token text -> id (shared by both images so the ids compare)


def tid(t):
    i = TOKENS.get(t)
    if i is None:
        i = TOKENS[t] = len(TOKENS)
    return i


def h64(s):
    return int.from_bytes(hashlib.blake2b(s.encode() if isinstance(s, str) else s, digest_size=8).digest(), 'little')


class Image:
    def __init__(self, path):
        self.path = pathlib.Path(path)
        self.raw = self.path.read_bytes()
        self.sha8 = hashlib.sha256(self.raw).hexdigest()[:8]
        self.pe = pefile.PE(data=self.raw, fast_load=True)
        self.pe.parse_data_directories(directories=[pefile.DIRECTORY_ENTRY['IMAGE_DIRECTORY_ENTRY_EXCEPTION']])
        self.sections = [(s.VirtualAddress, s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData), s.Name.decode().strip('\x00'), s.PointerToRawData) for s in self.pe.sections]
        self.size = self.pe.OPTIONAL_HEADER.SizeOfImage
        self.functions = []       # (lo, hi)
        self.starts = []
        self.features = {}        # lo -> dict
        self.by_h = ({}, {}, {})  # hash -> [lo]
        self.callers = {}         # callee lo -> set(caller lo)
        self.data_refs = {}       # data rva -> [(fn lo, insn index)]
        self.string_fns = {}      # string hash -> set(fn lo)

    def section_of(self, rva):
        for lo, hi, name, _ in self.sections:
            if lo <= rva < hi:
                return name
        return None

    def read(self, rva, n):
        return self.pe.get_data(rva, n)

    def string_at(self, rva):
        """A printable string (ASCII or UTF-16LE, 5+ characters) at rva, or None."""
        name = self.section_of(rva)
        if name not in ('.rdata', '.data', '_RDATA'):
            return None
        b = self.read(rva, 200)
        n = 0
        while n < len(b) and 32 <= b[n] < 127:
            n += 1
        if n >= 5 and (n == len(b) or b[n] == 0):
            return b[:n].decode('ascii')
        n = 0
        while n + 1 < len(b) and 32 <= b[n] < 127 and b[n + 1] == 0:
            n += 2
        if n >= 10 and (n + 1 >= len(b) or (b[n] == 0 and b[n + 1] == 0)):
            return 'L' + b[:n].decode('utf-16-le')
        return None

    def load_functions(self):
        entries = sorted(self.pe.DIRECTORY_ENTRY_EXCEPTION, key=lambda f: f.struct.BeginAddress)
        merged = []
        for e in entries:
            lo, hi = e.struct.BeginAddress, e.struct.EndAddress
            chained = False
            ui = getattr(e, 'unwindinfo', None)
            if ui is not None:
                chained = bool(getattr(ui, 'Flags', 0) & 4) if hasattr(ui, 'Flags') else False
            else:
                try:
                    chained = bool((self.read(e.struct.UnwindData, 1)[0] >> 3) & 4)
                except Exception:
                    chained = False
            if chained and merged and merged[-1][1] == lo:
                merged[-1] = (merged[-1][0], hi)
            else:
                merged.append((lo, hi))
        self.functions = merged
        self.starts = [lo for lo, _ in merged]

    def fn_of(self, rva):
        i = bisect.bisect_right(self.starts, rva) - 1
        if i >= 0 and self.functions[i][0] <= rva < self.functions[i][1]:
            return self.functions[i]
        return None

    def ensure(self, lo):
        """Features for a function the unwind table does not list (a leaf function): from lo to the next listed
        function, cut at the first int3 padding."""
        if lo in self.features or self.section_of(lo) != '.text':
            return lo in self.features
        i = bisect.bisect_right(self.starts, lo)
        hi = min(self.starts[i] if i < len(self.starts) else lo + 2048, lo + 2048)
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        md.detail = True
        code = self.read(lo, hi - lo)
        end = hi
        for insn in md.disasm(code, lo):
            if insn.mnemonic == 'int3':
                end = insn.address
                break
        if end <= lo:
            return False
        fe = self.features_of(md, lo, end)
        fe['leaf'] = True
        self.features[lo] = fe
        for i in range(3):
            self.by_h[i].setdefault(fe['h'][i], []).append(lo)
        for _, t in fe['calls']:
            self.callers.setdefault(t, set()).add(lo)
        for k, t in fe['rips']:
            self.data_refs.setdefault(t, []).append((lo, k))
        for sh in fe['strings']:
            self.string_fns.setdefault(sh, set()).add(lo)
        return True

    def analyse(self, cache_dir):
        cache = cache_dir / f'cache-{self.sha8}.pkl'
        if cache.exists():
            with open(cache, 'rb') as f:
                saved = pickle.load(f)
            global TOKENS
            # merge the token tables: ids of this cache are rewritten into the shared table
            remap = {i: tid(t) for t, i in saved['tokens'].items()}
            self.functions, self.starts = saved['functions'], [lo for lo, _ in saved['functions']]
            for lo, fe in saved['features'].items():
                fe['loose'] = array.array('I', [remap[i] for i in fe['loose']])
            self.features = saved['features']
            self.index()
            return
        self.load_functions()
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        md.detail = True
        t0 = time.time()
        for n, (lo, hi) in enumerate(self.functions):
            self.features[lo] = self.features_of(md, lo, hi)
            if n % 10000 == 0:
                print(f'  {self.path.name}: {n}/{len(self.functions)} functions, {time.time() - t0:.0f} s', file=sys.stderr)
        with open(cache, 'wb') as f:
            pickle.dump(dict(tokens=dict(TOKENS), functions=self.functions, features={lo: dict(fe, loose=list(fe['loose'])) for lo, fe in self.features.items()}), f)
        self.index()

    def features_of(self, md, lo, hi):
        code = self.read(lo, hi - lo)
        strict, medium, loose = [], [], []
        addrs = array.array('I')
        sizes = array.array('B')
        calls = []        # (insn index, target rva)
        rips = []         # (insn index, target rva)
        strings = []
        imms = []
        for insn in md.disasm(code, lo):
            addrs.append(insn.address)
            sizes.append(insn.size)
            k = len(addrs) - 1
            is_branch = insn.group(capstone.CS_GRP_CALL) or insn.group(capstone.CS_GRP_JUMP)
            s, m, l = [insn.mnemonic], [insn.mnemonic], [insn.mnemonic]
            for op in insn.operands:
                if op.type == x86.X86_OP_REG:
                    r = insn.reg_name(op.reg)
                    s.append('r' + r); m.append('r' + r); l.append('R')
                elif op.type == x86.X86_OP_IMM:
                    v = op.imm
                    if is_branch:
                        s.append('rel'); m.append('rel'); l.append('rel')
                        if insn.group(capstone.CS_GRP_CALL) and 0 <= v < self.size:
                            calls.append((k, v))
                    else:
                        s.append('i%x' % (v & 0xffffffffffffffff))
                        m.append('i%x' % v if -256 <= v < 256 else 'I')
                        l.append('I')
                        if v > 0xffff and not (0 <= v < self.size):
                            imms.append(v & 0xffffffffffffffff)
                elif op.type == x86.X86_OP_MEM:
                    base = insn.reg_name(op.mem.base) if op.mem.base else ''
                    index = insn.reg_name(op.mem.index) if op.mem.index else ''
                    if op.mem.base == x86.X86_REG_RIP:
                        target = (insn.address + insn.size + op.mem.disp) & 0xffffffff
                        rips.append((k, target))
                        s.append('rip'); m.append('rip'); l.append('rip')
                        st = self.string_at(target)
                        if st:
                            strings.append(h64(st))
                    else:
                        s.append(f'm{base}+{index}*{op.mem.scale}+{op.mem.disp:x}')
                        m.append(f'm{base}+{index}*{op.mem.scale}')
                        l.append('M')
                else:
                    s.append('?'); m.append('?'); l.append('?')
            strict.append(' '.join(s)); medium.append(' '.join(m)); loose.append(tid(' '.join(l)))
        return dict(hi=hi, n=len(addrs), addrs=addrs, sizes=sizes, h=(h64('|'.join(strict)), h64('|'.join(medium)), h64('|'.join(str(x) for x in loose))),
                    loose=array.array('I', loose), calls=calls, rips=rips, strings=strings, imms=imms, strict=None if len(strict) > 4000 else strict)

    def index(self):
        for lo, fe in self.features.items():
            for i in range(3):
                self.by_h[i].setdefault(fe['h'][i], []).append(lo)
            for _, t in fe['calls']:
                self.callers.setdefault(t, set()).add(lo)
            for k, t in fe['rips']:
                self.data_refs.setdefault(t, []).append((lo, k))
            for sh in fe['strings']:
                self.string_fns.setdefault(sh, set()).add(lo)

    def ngrams(self, lo):
        fe = self.features[lo]
        g = fe.get('ng')
        if g is None:
            a = fe['loose']
            g = fe['ng'] = frozenset(hash(tuple(a[i:i + 4])) for i in range(max(0, len(a) - 3)))
        return g

    def pattern(self, lo, length=28):
        """The first `length` bytes of the function with the rip-relative and branch displacements masked."""
        masked = bytearray(self.read(lo, length))
        mask = [True] * length
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        md.detail = True
        for insn in md.disasm(bytes(masked), lo):
            off = insn.address - lo
            if off >= length:
                break
            if insn.group(capstone.CS_GRP_CALL) or insn.group(capstone.CS_GRP_JUMP):
                for i in range(max(0, insn.size - 4), insn.size):
                    if off + i < length:
                        mask[off + i] = False
            for op in insn.operands:
                if op.type == x86.X86_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                    for i in range(insn.disp_offset, insn.disp_offset + 4):
                        if off + i < length:
                            mask[off + i] = False
                if op.type == x86.X86_OP_IMM and insn.imm_size == 8:
                    for i in range(insn.imm_offset, insn.imm_offset + 8):
                        if off + i < length:
                            mask[off + i] = False
        text = ' '.join('%02X' % masked[i] if mask[i] else '?' for i in range(length))
        return text, self.count_pattern(masked, mask)

    def count_pattern(self, pattern, mask):
        first = next((i for i, m in enumerate(mask) if m), 0)
        anchor = bytes(pattern[first:first + 1])
        hits = 0
        for lo, hi, name, raw_at in self.sections:
            if name != '.text':
                continue
            start, end = raw_at, raw_at + (hi - lo)
            pos = self.raw.find(anchor, start)
            while pos != -1 and pos < end:
                p = pos - first
                if p >= 0 and p + len(pattern) <= len(self.raw) and all((not m) or self.raw[p + i] == pattern[i] for i, m in enumerate(mask)):
                    hits += 1
                    if hits > 5:
                        return hits
                pos = self.raw.find(anchor, pos + 1)
        return hits


class Matcher:
    def __init__(self, old, new):
        self.old, self.new = old, new
        self.match = {}      # old fn lo -> (new lo, method, note)
        self.used = {}       # new lo -> old lo

    def take(self, lo, new_lo, method, note=''):
        if lo in self.match:
            return
        self.match[lo] = (new_lo, method, note)
        self.used[new_lo] = lo

    def rare_strings(self, lo):
        return [sh for sh in set(self.old.features[lo]['strings']) if len(self.old.string_fns.get(sh, ())) <= 3 and sh in self.new.string_fns]

    def by_shape(self, lo):
        fe = self.old.features[lo]
        for i, method in ((0, 'strict'), (1, 'medium'), (2, 'loose')):
            cands = [c for c in self.new.by_h[i].get(fe['h'][i], []) if c not in self.used]
            if len(cands) == 1:
                return cands[0], method, ''
            if len(cands) > 1 and i < 2:
                # several identical functions: a string or a caller tells them apart later
                return None
        return None

    def by_strings(self, lo):
        rare = self.rare_strings(lo)
        if not rare:
            return None
        cands = None
        for sh in rare:
            s = set(self.new.string_fns.get(sh, ())) - set(self.used)
            cands = s if cands is None else cands & s
        if cands and len(cands) == 1:
            return next(iter(cands)), 'strings', f'{len(rare)} rare strings'
        if cands and len(cands) > 1:
            best = self.rank(lo, cands)
            if best:
                return best[0], 'strings+ngram', best[1]
        return None

    def similarity(self, lo, new_lo):
        a, b = self.old.ngrams(lo), self.new.ngrams(new_lo)
        if not a or not b:
            return 0.0
        return len(a & b) / max(len(a), len(b))

    def rank(self, lo, cands, min_score=0.35, margin=1.4):
        fe = self.old.features[lo]
        scored = []
        for c in cands:
            n = self.new.features[c]['n']
            if n < fe['n'] * 0.5 - 8 or n > fe['n'] * 2 + 8:
                continue
            scored.append((self.similarity(lo, c), c))
        scored.sort(reverse=True)
        if not scored or scored[0][0] < min_score:
            return None
        if len(scored) > 1 and scored[1][0] * margin > scored[0][0] and scored[1][0] > 0.2:
            return None
        return scored[0][1], f'ngram {scored[0][0]:.2f}' + (f' (next {scored[1][0]:.2f})' if len(scored) > 1 else '')

    def by_graph(self, lo):
        """Candidates from the matched callers (their new callees) and the matched callees (their new callers)."""
        votes = {}
        fe = self.old.features[lo]
        for caller in self.old.callers.get(lo, ()):
            m = self.match.get(caller)
            if not m:
                continue
            for _, t in self.new.features[m[0]]['calls']:
                if t in self.new.features:
                    votes[t] = votes.get(t, 0) + 1
        for _, t in fe['calls']:
            m = self.match.get(t)
            if not m:
                continue
            for c in self.new.callers.get(m[0], ()):
                votes[c] = votes.get(c, 0) + 1
        cands = [c for c in votes if c not in self.used]
        if not cands:
            return None
        top = max(votes[c] for c in cands)
        cands = [c for c in cands if votes[c] >= max(1, top - 1)]
        best = self.rank(lo, cands, min_score=0.3)
        if best:
            return best[0], 'graph', f'{votes[best[0]]} edges, {best[1]}'
        return None

    def by_callers(self, lo):
        """The matched callers' aligned call instructions name the new function (also for a leaf function that has no
        unwind entry and so is in no index)."""
        votes, total = {}, 0
        for caller in self.old.callers.get(lo, ()):
            m = self.match.get(caller)
            if not m:
                continue
            fo, fn = self.old.features[caller], self.new.features[m[0]]
            calls = dict(fn['calls'])
            for k, t in fo['calls']:
                if t != lo:
                    continue
                j, verdict = self.map_index(caller, m[0], k)
                if j is None:
                    continue
                total += 1
                for jj in (j, j + 1, j - 1, j + 2, j - 2):
                    if jj in calls:
                        w = 2 if (jj == j and verdict.startswith('aligned')) else 1
                        votes[calls[jj]] = votes.get(calls[jj], 0) + w
                        break
        if not votes:
            return None
        best = max(votes, key=votes.get)
        if best in self.used or votes[best] * 10 < sum(votes.values()) * 6:
            return None
        self.new.ensure(best)
        sim = self.similarity(lo, best) if best in self.new.features and lo in self.old.features else -1
        if sim >= 0 and sim < 0.15 and self.old.features[lo]['n'] > 20:
            return None
        return best, 'callers', f'{votes[best]} of {sum(votes.values())} caller votes, ngram {sim:.2f}'

    def rare_index(self):
        """4-grams of the loose instruction list that occur in at most 40 functions of the new image -> the functions."""
        if getattr(self, '_rare', None) is None:
            counts = {}
            for c in self.new.features:
                for g in self.new.ngrams(c):
                    counts[g] = counts.get(g, 0) + 1
            rare = {}
            for c in self.new.features:
                for g in self.new.ngrams(c):
                    if counts[g] <= 40:
                        rare.setdefault(g, []).append(c)
            self._rare = rare
        return self._rare

    def by_scan(self, lo):
        """Last: the functions sharing the most rare 4-grams with it, ranked by similarity."""
        fe = self.old.features[lo]
        if fe['n'] < 12:
            return None
        rare = self.rare_index()
        votes = {}
        for g in self.old.ngrams(lo):
            for c in rare.get(g, ()):
                votes[c] = votes.get(c, 0) + 1
        cands = [c for c, _ in sorted(votes.items(), key=lambda kv: -kv[1])[:40] if c not in self.used]
        best = self.rank(lo, cands, min_score=0.45, margin=1.3)
        if best:
            return best[0], 'scan', best[1]
        return None

    def resolve(self, lo, deep=False):
        if lo in self.match:
            return self.match[lo]
        for method in (self.by_shape, self.by_strings, self.by_callers, self.by_graph) + ((self.by_scan,) if deep else ()):
            r = method(lo)
            if r:
                self.take(lo, *r)
                return self.match[lo]
        return None

    def resolve_neighbourhood(self, lo, depth=2):
        """Match the callers and callees around lo first (the call graph then has edges to vote with)."""
        seen = {lo}
        frontier = [lo]
        for _ in range(depth):
            nxt = []
            for f in frontier:
                fe = self.old.features.get(f)
                if not fe:
                    continue
                around = [t for _, t in fe['calls'] if t in self.old.features] + list(self.old.callers.get(f, ()))
                for g in around:
                    if g not in seen:
                        seen.add(g)
                        nxt.append(g)
                        self.resolve(g)
            frontier = nxt

    def align(self, lo, new_lo):
        cache = getattr(self, '_align', None)
        if cache is None:
            cache = self._align = {}
        ops = cache.get((lo, new_lo))
        if ops is None:
            a, b = self.old.features[lo]['loose'], self.new.features[new_lo]['loose']
            # the long functions (the render function: 4000 instructions) take seconds each: once per pair
            sm = difflib.SequenceMatcher(None, a.tolist(), b.tolist(), autojunk=len(a) > 3000 and len(b) > 3000)
            ops = cache[(lo, new_lo)] = sm.get_opcodes()
        return ops

    def map_index(self, lo, new_lo, k):
        """The new instruction index for old instruction index k, with a verdict."""
        for tag, i1, i2, j1, j2 in self.align(lo, new_lo):
            if i1 <= k < i2:
                if tag == 'equal':
                    return j1 + (k - i1), 'aligned'
                if tag == 'replace' and (i2 - i1) == (j2 - j1):
                    return j1 + (k - i1), 'replaced (same length)'
                if tag == 'replace':
                    return min(j1 + (k - i1), j2 - 1), 'replaced (different length)'
                return j1, 'deleted'
        return None, 'outside'

    def disp_changes(self, lo, new_lo, limit=40):
        """Displacements that differ between aligned, otherwise equal instructions (structure offsets that moved)."""
        fo, fn = self.old.features[lo], self.new.features[new_lo]
        if fo.get('strict') is None or fn.get('strict') is None:
            return []
        out = []
        pat = re.compile(r'm([a-z0-9]*)\+([a-z0-9]*)\*(\d)\+(-?[0-9a-f]+)')
        for tag, i1, i2, j1, j2 in self.align(lo, new_lo):
            if tag not in ('equal', 'replace') or (i2 - i1) != (j2 - j1):
                continue
            for i, j in zip(range(i1, i2), range(j1, j2)):
                so, sn = fo['strict'][i], fn['strict'][j]
                if so == sn:
                    continue
                mo, mn = pat.findall(so), pat.findall(sn)
                if mo and mn and len(mo) == len(mn) and all(a[:3] == b[:3] for a, b in zip(mo, mn)):
                    for a, b in zip(mo, mn):
                        if a[3] != b[3]:
                            out.append(dict(old_insn=hex(fo['addrs'][i]), new_insn=hex(fn['addrs'][j]), base=a[0], old_disp=a[3], new_disp=b[3], old=so, new=sn))
                if len(out) >= limit:
                    return out
        return out


def scan_hooks(folder, old):
    """Every address of the old image written in the hooks: name=0x..., baseAddress+0x..., base+0x..., {0x..., "pattern"}
    and "0x..." inside a string (a log's text: mapped too, so the records name the new build's addresses). A number
    after another name and a plus is an offset into a structure, not an address, and is left out."""
    sites = []
    rx = re.compile(r'(?P<name>[A-Za-z_][A-Za-z0-9_]*)?\s*(?P<how>=|\+|\{|,|")\s*0x(?P<hex>[0-9A-Fa-f]{4,7})\b')
    pattern_line = re.compile(r'"(?:[0-9A-Fa-f]{2}|\?\??)(?: (?:[0-9A-Fa-f]{2}|\?\??)){5,}"')
    for f in sorted(list(folder.glob('*.hpp')) + list(folder.glob('*.cpp'))):
        for n, line in enumerate(f.read_text(encoding='utf-8', errors='replace').splitlines(), 1):
            if line.lstrip().startswith('//'):
                continue
            has_pattern = bool(pattern_line.search(line))
            for m in rx.finditer(line):
                how, name = m.group('how'), m.group('name') or ''
                if how == '+' and name not in ('baseAddress', 'base', 'image', 'module', 'imageBase'):
                    continue
                # {0x..., "pattern"} and {0x..., 0x..., "pattern"}: the addresses of a table row that holds a pattern
                if how in ('{', ',') and not has_pattern:
                    continue
                v = int(m.group('hex'), 16)
                if old.section_of(v) in ('.text', '.rdata', '.data', '_RDATA') and v >= 0x1000:
                    kind_name = name if how == '=' else {'+': 'base+', '{': 'pair', ',': 'pair', '"': 'text'}[how]
                    sites.append(dict(file=f.name, line=n, name=kind_name, old=v))
    return sites


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--old', required=True, type=pathlib.Path)
    p.add_argument('--new', required=True, type=pathlib.Path)
    p.add_argument('--scan-hooks', type=pathlib.Path)
    p.add_argument('--site', action='append', default=[], help='name=0x... (an address of the old build)')
    p.add_argument('--report', type=pathlib.Path, required=True)
    p.add_argument('--deep', action='store_true', help='also the slow whole-image scan for the functions nothing else finds')
    p.add_argument('--disp', action='store_true', help='list the displacements that changed in each matched function')
    a = p.parse_args()
    a.report.parent.mkdir(parents=True, exist_ok=True)
    old, new = Image(a.old), Image(a.new)
    print(f'old {old.path} sha {old.sha8}, new {new.path} sha {new.sha8}', file=sys.stderr)
    for img in (old, new):
        img.analyse(a.report.parent)
        print(f'  {img.path.name}: {len(img.functions)} functions', file=sys.stderr)
    sites = scan_hooks(a.scan_hooks, old) if a.scan_hooks else []
    for s in a.site:
        name, _, v = s.partition('=')
        sites.append(dict(file='(argument)', line=0, name=name, old=int(v, 16)))
    # the same address once (the first mention names it)
    seen = {}
    for s in sites:
        seen.setdefault(s['old'], s)
    sites = sorted(seen.values(), key=lambda s: s['old'])
    m = Matcher(old, new)
    t_start = time.time()
    # pass 1: the functions themselves (sites that are function starts or inside functions), with their neighbourhoods
    fn_sites = []
    for s in sites:
        f = old.fn_of(s['old'])
        if f:
            s['fn'] = f[0]
            s['kind'] = 'fn' if f[0] == s['old'] else ('call-site' if old.read(s['old'] - 5, 1) == b'\xe8' else 'in-fn')
            fn_sites.append(s)
        elif old.section_of(s['old']) == '.text' and old.ensure(s['old']):
            s['fn'] = s['old']
            s['kind'] = 'leaf'
            fn_sites.append(s)
        else:
            s['kind'] = 'data' if old.section_of(s['old']) in ('.data', '.rdata', '_RDATA') else 'unknown'
    for s in fn_sites:
        m.resolve(s['fn'])
    print(f'  pass 0: {sum(1 for s in fn_sites if s["fn"] in m.match)}/{len(fn_sites)} function sites matched, {time.time() - t_start:.0f} s', file=sys.stderr)
    for rnd in range(4):
        unresolved = [s['fn'] for s in fn_sites if s['fn'] not in m.match]
        if not unresolved:
            break
        for lo in unresolved:
            m.resolve_neighbourhood(lo, depth=2 if rnd == 0 else 1)
            m.resolve(lo)
        if a.deep and rnd >= 1:
            for lo in unresolved:
                m.resolve(lo, deep=True)
                m.resolve_neighbourhood(lo, 1)
        print(f'  pass {rnd + 1}: {sum(1 for s in fn_sites if s["fn"] in m.match)}/{len(fn_sites)} function sites matched, {time.time() - t_start:.0f} s', file=sys.stderr)
    for s in fn_sites:
        if s['fn'] not in m.match:
            m.resolve_neighbourhood(s['fn'], 1)
            m.resolve(s['fn'], deep=a.deep)
    # call sites: the callee too (for the check)
    for s in fn_sites:
        if s['kind'] == 'call-site':
            t = old.read(s['old'] - 4, 4)
            target = (s['old'] + int.from_bytes(t, 'little', signed=True)) & 0xffffffff
            s['callee'] = target
            if target in old.features:
                m.resolve(target)
                if target not in m.match:
                    m.resolve_neighbourhood(target, 1)
                    m.resolve(target)
    # results
    rows = []
    for s in sites:
        r = dict(name=s['name'], file=s['file'], line=s['line'], old=hex(s['old']), kind=s['kind'])
        if s['kind'] in ('fn', 'call-site', 'in-fn', 'leaf'):
            mt = m.match.get(s['fn'])
            r['old_fn'] = hex(s['fn'])
            if not mt:
                r['new'] = None
                r['method'] = 'unmatched'
            else:
                new_fn, method, note = mt
                r['new_fn'] = hex(new_fn)
                r['method'] = method
                r['note'] = note
                r['old_fn_insns'] = old.features[s['fn']]['n']
                r['new_fn_insns'] = new.features[new_fn]['n'] if new.ensure(new_fn) else None
                if s['kind'] in ('fn', 'leaf'):
                    r['new'] = hex(new_fn)
                    pat, hits = new.pattern(new_fn)
                    r['pattern'] = pat
                    r['pattern_hits'] = hits
                else:
                    fo, fn = old.features[s['fn']], new.features[new_fn]
                    k = bisect.bisect_right(fo['addrs'], s['old'] - 1) - 1 if s['kind'] == 'call-site' else bisect.bisect_right(fo['addrs'], s['old']) - 1
                    # a call site names the return address: the instruction before it is the call
                    j, verdict = m.map_index(s['fn'], new_fn, k)
                    r['align'] = verdict
                    if j is not None:
                        if s['kind'] == 'call-site':
                            r['new'] = hex(fn['addrs'][j] + fn['sizes'][j])
                            calls = dict(fn['calls'])
                            new_target = calls.get(j)
                            r['new_call_target'] = hex(new_target) if new_target is not None else None
                            cm = m.match.get(s['callee'])
                            r['callee_old'] = hex(s['callee'])
                            r['callee_new'] = hex(cm[0]) if cm else None
                            if cm and new_target != cm[0]:
                                # the aligned instruction is not the call to the matched callee: the k-th call to it instead
                                old_calls = [i for i, t in fo['calls'] if t == s['callee']]
                                ordinal = old_calls.index(k) if k in old_calls else -1
                                new_calls = [i for i, t in fn['calls'] if t == cm[0]]
                                if 0 <= ordinal < len(new_calls):
                                    jj = new_calls[ordinal]
                                    r['new'] = hex(fn['addrs'][jj] + fn['sizes'][jj])
                                    r['align'] = f'{verdict}; taken: call {ordinal + 1} of {len(new_calls)} to the matched callee'
                                else:
                                    r['align'] = f'{verdict}; DOUBT: the aligned instruction calls {r["new_call_target"]}, the callee matched {r["callee_new"]} ({len(new_calls)} calls to it)'
                            elif not cm:
                                r['align'] = verdict + '; the callee is unmatched (the call target above is what the aligned call reaches)'
                        else:
                            r['new'] = hex(fn['addrs'][j])
                            r['new_insn'] = new.features[new_fn]['strict'][j] if new.features[new_fn].get('strict') else None
                            r['old_insn'] = fo['strict'][k] if fo.get('strict') else None
                    else:
                        r['new'] = None
                if a.disp:
                    r['disp_changes'] = m.disp_changes(s['fn'], new_fn)
        elif s['kind'] == 'data':
            refs = old.data_refs.get(s['old'], [])
            r['new'] = None
            r['method'] = 'data: no reference' if not refs else 'data: referrers unmatched'
            for lo, k in refs[:40]:
                mt = m.resolve(lo) or (m.resolve_neighbourhood(lo, 1), m.resolve(lo, deep=a.deep))[1]
                if not mt:
                    continue
                j, verdict = m.map_index(lo, mt[0], k)
                if j is None:
                    continue
                rips = dict(new.features[mt[0]]['rips'])
                t = rips.get(j)
                if t is not None:
                    r['new'] = hex(t)
                    r['method'] = f'data via {hex(lo)}->{hex(mt[0])} ({mt[1]}, {verdict})'
                    r['new_section'] = new.section_of(t)
                    break
        rows.append(r)
    ok = sum(1 for r in rows if r.get('new'))
    summary = dict(old=str(old.path), old_sha=old.sha8, new=str(new.path), new_sha=new.sha8, sites=len(rows), mapped=ok,
                   methods={k: sum(1 for r in rows if r.get('method') == k) for k in set(r.get('method') for r in rows)})
    a.report.write_text(json.dumps(dict(summary=summary, rows=rows), indent=1), encoding='utf-8')
    for r in rows:
        callee = f" callee {r['callee_old']}->{r.get('callee_new')} (call reaches {r.get('new_call_target')})" if 'callee_old' in r else ''
        print(f"{r['old']:>9} {r['kind']:9} {r['name']:28} -> {str(r.get('new')):>9}  {r.get('method', '')} {r.get('note', '')} {r.get('align', '')}{callee}"[:230])
    print('sites', len(rows), 'mapped', ok, summary['methods'])
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
