#!/usr/bin/env python3
"""Every address-with-pattern of the hooks checked against an executable (read-only; 2026-10-16, after the 1.0
re-matching: a table row's pattern was once overwritten with its neighbour's). A row {0x..., "pattern"} or
{0x..., 0x..., "pattern"} is checked at its first address; a declaration rva=0x... (or siteRva, targetRva, ...) with a
pattern="..." within six lines is checked at that address; a call site named with -5 (callerRva etc.) is not checked here
(the hooks do that at install). Prints the mismatches and exits 1 when there are any.

    check_hook_patterns.py [enshrouded.exe] [--hooks vrmod/hooks] [--hooks vrmod/layer]
"""
import argparse, pathlib, re, sys

sys.path.insert(0, 'vrmod/tools')
from match_functions import Image  # noqa: E402

PAT = re.compile(r'"((?:[0-9A-Fa-f]{2}|\?\??)(?: (?:[0-9A-Fa-f]{2}|\?\??)){5,})"')
ROW = re.compile(r'\{\s*0x([0-9A-Fa-f]{5,7})\s*,')
DECL = re.compile(r'\b(?:rva|siteRva|targetRva|renderRva|lookupRva|buildRva|getRva|encodeRva|predictRva|serverRva|clientRva|abilityStartRva|secondCheckRva|findTargetRva|blurRva|freezingRva|orbitRva|updateRva|submitRva|dispatchRva|shadingRva|noContentRva|decalDescriptorRva|dynamicLo)\s*=\s*0x([0-9A-Fa-f]{5,7})\b')


def parse(text):
    out, mask = [], []
    for tok in text.split():
        if tok in ('?', '??'):
            out.append(0); mask.append(False)
        else:
            out.append(int(tok, 16)); mask.append(True)
    return bytes(out), mask


def matches(img, rva, text):
    b, mask = parse(text)
    data = img.read(rva, len(b))
    return len(data) == len(b) and all((not m) or data[i] == b[i] for i, m in enumerate(mask))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('exe', nargs='?', default='enshrouded.exe')
    p.add_argument('--hooks', action='append', default=[])
    a = p.parse_args()
    img = Image(a.exe)
    folders = [pathlib.Path(h) for h in a.hooks] or [pathlib.Path('vrmod/hooks'), pathlib.Path('vrmod/layer')]
    checked, bad = 0, []
    for folder in folders:
        for f in sorted(list(folder.glob('*.hpp')) + list(folder.glob('*.cpp'))):
            lines = f.read_text(encoding='utf-8', errors='replace').splitlines()
            for n, line in enumerate(lines, 1):
                code = line.split('//')[0] if '//' in line and '"' not in line.split('//')[0] else line
                if code.lstrip().startswith('//'):
                    continue
                pm = PAT.search(code)
                rm = ROW.search(code)
                if pm and rm:
                    rva = int(rm.group(1), 16)
                    checked += 1
                    if not matches(img, rva, pm.group(1)):
                        bad.append(f'{f.name}:{n} row 0x{rva:x} pattern "{pm.group(1)[:40]}..." does not match (bytes {img.read(rva, 12).hex(" ").upper()})')
                    continue
                dm = DECL.search(code)
                if dm:
                    rva = int(dm.group(1), 16)
                    for k in range(max(0, n - 7), min(len(lines), n + 6)):
                        pk = PAT.search(lines[k].split('//')[0] if '//' in lines[k] and '"' not in lines[k].split('//')[0] else lines[k])
                        if pk and ('attern' in lines[k] or 'signature' in lines[k]):
                            checked += 1
                            if not matches(img, rva, pk.group(1)):
                                bad.append(f'{f.name}:{n} {dm.group(0)} with pattern at line {k + 1} "{pk.group(1)[:40]}..." does not match (bytes {img.read(rva, 12).hex(" ").upper()})')
                            break
    print(f'{checked} pattern sites checked, {len(bad)} mismatches')
    for b in bad:
        print('  ' + b)
    return 1 if bad else 0


if __name__ == '__main__':
    raise SystemExit(main())
