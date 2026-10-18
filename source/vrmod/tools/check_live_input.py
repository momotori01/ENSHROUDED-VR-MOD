"""Check a Mock live input run against test_env/world-live-input-hands.csv (2026-10-09, real-device phase 1).

Usage: check_live_input.py <live probe dir> [--report PATH]

Reads controller_live_sample (logged on change and about every 90 hand frames) and the native frontend samples
(input_ui_native_sample: hud_screen / auxiliary_state) from the probe log. Each scripted span of the CSV must show
the expected synthetic pad, and the native screen must follow the menu and the quick wheel. Hand frame numbers are
the XR frames of the CSV rows; a sample may lag its row by a few frames, so spans are widened by `--slack`.
This proves the mapping and the native routing on the Mock runtime only, not the feel of the real controllers.
"""
import argparse
import json
import pathlib
import sys

A, B, X, Y, LB, RB, START = 0x1000, 0x2000, 0x4000, 0x8000, 0x100, 0x200, 0x10


def load(probe):
    out = []
    for line in (probe / 'view-resource-probe.jsonl').read_text(encoding='utf-8', errors='replace').splitlines():
        try:
            out.append(json.loads(line))
        except ValueError:
            pass
    return out


def check(events, slack=12):
    live = [e for e in events if e.get('event') == 'controller_live_sample' and e.get('fresh')]
    ui = [e for e in events if e.get('event') == 'input_ui_native_sample']

    def span(lo, hi):
        return [e for e in live if lo <= e['frame'] <= hi + slack]

    def any_in(lo, hi, pred):
        return any(pred(e) for e in span(lo, hi))
    checks = {
        'enabled': any(e.get('event') == 'controller_live_enabled' for e in events),
        'connected': any(e.get('event') == 'controller_fixture_connected' for e in events),
        'native_enumerated': any(e.get('native_connected') == 1 for e in live),
        'walk_left_stick': any_in(900, 1019, lambda e: e['left'] == [0, 32767] and e['live_screen'] == 1),
        'turn_right_stick_x': any_in(1080, 1139, lambda e: e['right'][0] == 26214 and e['buttons'] == 0),
        'jump_right_a': any_in(1200, 1205, lambda e: e['buttons'] & A and e['live_screen'] == 1),
        'dodge_right_stick_down_b': any_in(1300, 1306, lambda e: e['buttons'] & B and e['right'][1] == 0 and e['live_screen'] == 1),
        'gesture_start': any_in(1460, 1465, lambda e: e['buttons'] & START and e['gesture']),
        'menu_opened_ui_mode': any_in(1460, 1545, lambda e: e['live_screen'] == 3),
        'menu_b_back': any_in(1540, 1545, lambda e: e['buttons'] & B and e['raw_faces'][1] == 1),
        'right_b_never_y': not any(e['buttons'] & Y for e in live),
        'left_x_nothing_in_play': all(e['buttons'] == 0 for e in span(1700, 1705) if e['raw_faces'][2] == 1) and
            any(e['raw_faces'][2] == 1 for e in span(1700, 1705)),
        'wheel_grip_rb': any_in(1800, 1919, lambda e: e['buttons'] & RB),
        'wheel_opened_overlay': any_in(1800, 1919, lambda e: e['live_screen'] == 2),
        'neutral_after_script': bool(live) and live[-1]['buttons'] == 0 and live[-1]['left'] == [0, 0] and live[-1]['right'] == [0, 0],
        'native_menu_screen': any(e.get('hud_screen') == 4 for e in ui),
        'native_wheel_state': any(e.get('auxiliary_state') == 1 for e in ui),
        'no_controller_failure': not any(e.get('event') == 'controller_fixture_failure' for e in events),
        'release_confirmed': any(e.get('event') == 'controller_live_release_confirmed' for e in events),
        'restored': any(e.get('event') == 'controller_fixture_restored' and e.get('pointers_restored') for e in events),
    }
    metrics = dict(fresh_samples=len(live), stale_samples=sum(1 for e in events if e.get('event') == 'controller_live_sample' and not e.get('fresh')),
                   screens=sorted({e.get('hud_screen') for e in ui if e.get('hud_screen') is not None}),
                   failure=next((e.get('reason') for e in events if e.get('event') == 'controller_fixture_failure'), None))
    return checks, metrics


def main():
    p = argparse.ArgumentParser()
    p.add_argument('probe', type=pathlib.Path)
    p.add_argument('--report', type=pathlib.Path)
    p.add_argument('--slack', type=int, default=12)
    a = p.parse_args()
    checks, metrics = check(load(a.probe), a.slack)
    for k, v in checks.items():
        print('PASS' if v else 'FAIL', k)
    ok = all(checks.values())
    print('PASS' if ok else 'FAIL', sum(checks.values()), 'of', len(checks), json.dumps(metrics))
    if a.report:
        a.report.write_text(json.dumps(dict(checks={k: 'PASS' if v else 'FAIL' for k, v in checks.items()}, metrics=metrics,
                                            status='PASS' if ok else 'FAIL', limit='Mock runtime; mapping and native routing only'), indent=2), encoding='utf-8')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
