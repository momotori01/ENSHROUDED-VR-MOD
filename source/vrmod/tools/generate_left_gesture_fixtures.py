"""Derive the 2026-10-05 18:50 user-spec fixtures from approved base fixtures (deterministic).

  left-menu:     world-head-gesture-hands.csv with the LEFT hand at the left temple instead of
                 the right hand at the right temple (same frames 430..619, same stick/click columns).
  equip-overlap: world-grip-router-equip-hands.csv with the two shoulder presses made while the
                 pressing hand rests at its own temple (gesture already active):
                 left hand at (-0.18,1.65,0.02) for 520..579 (press 550..559),
                 right hand at (0.18,1.65,0.02) for 620..679 (press 650..659).
                 0.02 m behind the HMD keeps the point inside the head-relative shoulder reach.
Only positions change; grips, sticks, clicks, focus and profile columns are copied.
"""
import argparse
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LEFT, RIGHT = (1, 2, 3), (14, 15, 16)
REST_RIGHT = ('0.5', '1.65', '0')


def read(path):
    lines = path.read_text(encoding='utf-8').splitlines()
    return [l for l in lines if l.startswith('#')], [l.split(',') for l in lines if l and not l.startswith('#')]


def text(v):
    s = f'{v:.6f}'.rstrip('0').rstrip('.')
    return '0' if s in ('-0', '') else s


def left_menu():
    _, rows = read(ROOT / 'test_env/world-head-gesture-hands.csv')
    moved = 0
    for r in rows:
        if tuple(r[i] for i in RIGHT) != REST_RIGHT:
            if tuple(r[i] for i in RIGHT) != ('0.18', '1.65', '0'):
                raise ValueError('unexpected right-hand path in base gesture fixture')
            for i, v in zip(LEFT, ('-0.18', '1.65', '0')):
                r[i] = v
            for i, v in zip(RIGHT, REST_RIGHT):
                r[i] = v
            moved += 1
    frames = [int(r[0]) for r in rows if r[LEFT[0]] == '-0.18']
    if moved != 190 or frames != list(range(430, 620)):
        raise ValueError('left menu derivation changed')
    header = '# Menu-only, LEFT temple: enter430, short Start450/580, DpadRight500..502, leave620; right hand at rest; no confirm/movement/attack.'
    return header, rows


def equip_overlap():
    _, rows = read(ROOT / 'test_env/world-grip-router-equip-hands.csv')
    for first, last, cols, value in ((520, 579, LEFT, (-0.18, 1.65, 0.02)), (620, 679, RIGHT, (0.18, 1.65, 0.02))):
        for f in range(first, last + 1):
            for i, v in zip(cols, value):
                rows[f - 1][i] = text(v)
    # Presses must be unchanged and inside the resting window (>= 8 frames of dwell first).
    if [int(r[0]) for r in rows if r[11] == '1'] != list(range(550, 560)) or [int(r[0]) for r in rows if r[24] == '1'] != list(range(460, 470)) + list(range(650, 660)):
        raise ValueError('equip base presses changed')
    header = '# Finite GripRouter overlap equip: right-hip press 460..469; left hand rests at its temple 520..579 (press 550..559); right hand rests at its temple 620..679 (press 650..659); shoulder reach v2, gesture never blocks grips.'
    return header, rows


MODES = {'left-menu': (left_menu, 'test_env/world-head-gesture-left-hands.csv'),
         'equip-overlap': (equip_overlap, 'test_env/world-grip-router-equip-overlap-hands.csv')}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('mode', choices=sorted(MODES))
    p.add_argument('--verify', action='store_true', help='compare with the committed fixture instead of writing')
    a = p.parse_args()
    build, rel = MODES[a.mode]
    header, rows = build()
    data = header + '\n' + '\n'.join(','.join(r) for r in rows) + '\n'
    out = ROOT / rel
    if a.verify:
        ok = out.read_text(encoding='utf-8') == data
        print('PASS' if ok else 'FAIL', rel)
        return int(not ok)
    out.write_text(data, encoding='utf-8')
    print('wrote', rel, len(rows), 'rows')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
