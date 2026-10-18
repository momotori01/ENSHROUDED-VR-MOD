#!/usr/bin/env python3
"""Where the hooks' byte patterns sit in an executable (read-only): for a game update (2026-10-16, user: "an update comes
tomorrow - keep it in mind"), the patterns the hooks declare (constexpr const char* pattern="..." beside a constexpr
uintptr_t rva=0x...) are searched in the given enshrouded.exe, and every hook is listed with the RVA it declares, the
RVAs the pattern is found at in that executable, and a verdict: same (found once, at the declared RVA), moved (found
once, elsewhere), ambiguous (found more than once) or missing. The other fixed numbers (callers, offsets into the
game's structures) are not checked here: those need the new executable's disassembly.

   find_hook_sites.py [enshrouded.exe] [--hooks vrmod/hooks] [--report <json>]
"""
import argparse, json, pathlib, re, sys


def parse_pattern(text):
    bytes_, mask = [], []
    for tok in text.split():
        if tok in ('?', '??'):
            bytes_.append(0); mask.append(False)
        else:
            bytes_.append(int(tok, 16)); mask.append(True)
    return bytes(bytes_), mask


def find_all(image, pattern, mask, sections):
    first = next((i for i, m in enumerate(mask) if m), 0)
    anchor = pattern[first:first + 1]
    hits = []
    for start, end in sections:
        pos = image.find(anchor, start)
        while pos != -1 and pos < end:
            p = pos - first
            if p >= 0 and p + len(pattern) <= len(image) and all((not m) or image[p + i] == pattern[i] for i, m in enumerate(mask)):
                hits.append(p)
            pos = image.find(anchor, pos + 1)
    return hits


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('exe', nargs='?', type=pathlib.Path, default=pathlib.Path('enshrouded.exe'))
    p.add_argument('--hooks', type=pathlib.Path, default=pathlib.Path('vrmod/hooks'))
    p.add_argument('--report', type=pathlib.Path)
    a = p.parse_args()
    sys.path.insert(0, 'vrmod/tools/python')
    import pefile
    raw = a.exe.read_bytes(); pe = pefile.PE(data=raw, fast_load=True)
    # the file offsets of the code sections and their RVA bases
    sections = [(s.PointerToRawData, s.PointerToRawData + s.SizeOfRawData, s.VirtualAddress) for s in pe.sections if s.Characteristics & 0x20000000]
    rows = []
    for f in sorted(list(a.hooks.glob('*.hpp')) + list(a.hooks.glob('*.cpp'))):
        text = f.read_text(encoding='utf-8', errors='replace')
        patterns = [(m.start(), m.group(1)) for m in re.finditer(r'constexpr\s+const\s+char\*\s+\w*[pP]attern\w*\s*=\s*"([0-9A-Fa-f? ]+)"', text)]
        for at, pat in patterns:
            before = text[max(0, at - 1200):at]
            rva_m = list(re.finditer(r'\b(?:rva|siteRva|Rva)\s*=\s*0x([0-9A-Fa-f]+)', before))
            declared = int(rva_m[-1].group(1), 16) if rva_m else None
            pattern, mask = parse_pattern(pat)
            hits = find_all(raw, pattern, mask, [(s, e) for s, e, _ in sections])
            rvas = []
            for h in hits:
                for s, e, base in sections:
                    if s <= h < e:
                        rvas.append(base + (h - s))
            verdict = 'missing' if not rvas else 'ambiguous' if len(rvas) > 1 else 'same' if declared == rvas[0] else 'moved' if declared is not None else 'found'
            rows.append(dict(file=f.name, declared=hex(declared) if declared is not None else None, found=[hex(r) for r in rvas], verdict=verdict))
    counts = {}
    for r in rows:
        counts[r['verdict']] = counts.get(r['verdict'], 0) + 1
    for r in rows:
        if r['verdict'] != 'same':
            print(r['verdict'], r['file'], 'declared', r['declared'], 'found', r['found'])
    print('patterns', len(rows), counts)
    if a.report:
        a.report.write_text(json.dumps(dict(exe=str(a.exe), rows=rows, counts=counts), indent=1), encoding='utf-8')
    return 0 if counts.get('missing', 0) == 0 and counts.get('ambiguous', 0) == 0 else 1


if __name__ == '__main__':
    raise SystemExit(main())
