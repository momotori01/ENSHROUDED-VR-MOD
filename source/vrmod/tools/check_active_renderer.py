"""Audit every native renderer settings record; repeated unchanged records are valid."""
import argparse
import json
import re
from pathlib import Path


def setting_checks(actual, expected):
    # A settings rebuild logs the whole config again. Require one distinct mode,
    # not one log line; empty logs and mid-run changes must still fail.
    return dict(single_native_setting=bool(actual) and len(set(actual)) == 1,
                requested_mode_matches=bool(actual) and
                (expected is None or all(mode == expected for mode in actual)))


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('launch', type=Path, nargs='?')
    p.add_argument('--report', type=Path)
    p.add_argument('--self-test', action='store_true')
    a = p.parse_args()
    if a.self_test:
        for actual, expected, passed in [
            (['Dlss'], 'Dlss', True),
            (['Dlss', 'Dlss'], 'Dlss', True),
            ([], 'Dlss', False), ([], None, False),
            (['Dlss', 'Fsr'], 'Dlss', False),
            (['Dlss', 'Fsr'], None, False),
            (['Fsr'], 'Dlss', False),
            (['Dlaa', 'Dlaa'], None, True),
        ]:
            assert all(setting_checks(actual, expected).values()) == passed
        print('8 renderer-setting cases PASS')
        return 0
    if a.launch is None:
        p.error('launch is required')
    root = a.launch
    launch = json.loads((root / 'launch.json').read_text(encoding='utf-8-sig'))
    log = (root / 'completed-game.log').read_text(encoding='utf-8', errors='replace')
    actual = re.findall(r'"antiAliasing":\s*"([^"\r\n]+)"', log)
    record = root / 'vr-render-settings.json'
    expected = None
    if record.exists():
        expected = json.loads(record.read_text(encoding='utf-8-sig'))['applied']['antiAliasing']
    checks = setting_checks(actual, expected)
    report = dict(status='PASS' if all(checks.values()) else 'MISMATCH',
                  checks=checks, requested=expected, actual=actual,
                  settings_records=len(actual), renderdoc=launch.get('renderdoc', False),
                  ngx_requirements_succeeded='DLSS GetFeatureRequirements succeeded' in log,
                  ngx_requirements_errors=re.findall(r'GetFeatureRequirements error: ([^\r\n]+)', log),
                  scope='Native logged settings only. Per-eye API/dimensions and image quality need separate checks.')
    (a.report or root / 'active-renderer-check.json').write_text(
        json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))
    return int(report['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
