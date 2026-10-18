"""Isolate the original scene-time arithmetic and compare saved scene observations.

Executes only e2074..e20e5 in Unicorn. It does not run scene updates or game code
in the host process, and does not interpret raw time units as an OpenXR clock.
"""
import argparse
import json
import pathlib
import struct
from emulate_camera_math import CameraMath, SHA256
from analyze_render_observer import analyze
from unicorn.x86_const import UC_X86_REG_RDI, UC_X86_REG_R14, UC_X86_REG_RIP


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--observation', type=pathlib.Path, required=True)
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('docs/analysis/emulated_scene_time.json'))
    args = p.parse_args()
    engine = CameraMath(pathlib.Path('enshrouded.exe'))
    engine.allowed.append((0xe2074, 0xe20e5))
    scene, source = engine.data+0x1000, engine.data+0x3000
    rows = []
    def check(name, actual, expected):
        rows.append(dict(name=name, status='PASS' if actual == expected else 'FAIL', actual=actual, expected=expected))
        print(rows[-1]['status'], name)
    def update(epoch, current, timestamp):
        engine.cpu.mem_write(scene+0x8e0, struct.pack('<3q', epoch, current, -123))
        engine.cpu.mem_write(source, struct.pack('<q', timestamp))
        engine.cpu.reg_write(UC_X86_REG_RDI, scene)
        engine.cpu.reg_write(UC_X86_REG_R14, source)
        engine.cpu.emu_start(engine.base+0xe2074, engine.base+0xe20e5, timeout=100000, count=100)
        if engine.cpu.reg_read(UC_X86_REG_RIP) != engine.base+0xe20e5:
            raise ValueError('scene time arithmetic did not reach expected boundary')
        return list(struct.unpack('<2q', engine.cpu.mem_read(scene+0x8e8, 16)))
    first = update(1000000000, 20000000, 1023000000)
    check('single_scene_update', first, [23000000, 20000000])
    second = update(1000000000, first[0], 1023000000)
    check('repeated_same_time_changes_previous', second, [23000000, 23000000])
    check('repeated_update_collapses_delta', second[0]-second[1], 0)
    maximum, minimum = 2**63-1, -2**63
    for name, epoch, timestamp, expected in [
            ('maximum_timestamp', 7, maximum, maximum), ('minimum_timestamp', 7, minimum, minimum),
            ('maximum_epoch', maximum, 7, minimum), ('minimum_epoch', minimum, 7, maximum),
            ('same_maximum', maximum, maximum, 0), ('same_minimum', minimum, minimum, 0)]:
        check(name, update(epoch, 11, timestamp), [expected, 11])
    observed = analyze([json.loads(line) for line in args.observation.read_text().splitlines()])
    previous_by_scene = {}
    comparisons = 0
    for sample in observed['samples']:
        timing = sample.get('scene_time')
        if not timing: continue
        previous = previous_by_scene.get(sample['scene_address'])
        if previous and timing['epoch_raw'] == previous['epoch_raw']:
            check(f'observed_history_sample{sample["sample"]}', timing['previous_relative_raw'], previous['current_relative_raw'])
            comparisons += 1
        previous_by_scene[sample['scene_address']] = timing
    if not comparisons: raise ValueError('no comparable observed scene time transitions')
    report = dict(executable_sha256=SHA256, scope=__doc__, observation=str(args.observation.resolve()),
                  checks=rows, passed=sum(r['status'] == 'PASS' for r in rows), failed=sum(r['status'] == 'FAIL' for r in rows))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2), encoding='utf8')
    print('RESULT', report['passed'], 'PASS', report['failed'], 'FAIL')
    return int(report['failed'] != 0)


if __name__ == '__main__':
    raise SystemExit(main())
