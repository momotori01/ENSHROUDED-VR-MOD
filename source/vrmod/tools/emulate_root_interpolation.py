"""Execute local game Root interpolation instructions in bounded Unicorn memory.

Task-context getters run unchanged against synthetic context memory. Only scalar
CRT sin/acos are substituted.
No game process access, host calls, game task execution or save writes.
"""
import argparse
import json
import math
import struct
from pathlib import Path

from emulate_camera_math import CameraMath
from unicorn.x86_const import UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RAX, UC_X86_REG_R9, UC_X86_REG_XMM0


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


def transform(position, quaternion=(0, 0, 0, 1), scale=(1, 1, 1)):
    return struct.pack('<3q4f3fI', *(round(x * 2**32) for x in position), *quaternion, *scale, 0)


def decode(data):
    values = struct.unpack('<3q4f3fI', data)
    return {'position': [x / 2**32 for x in values[:3]], 'quaternion': values[3:7], 'scale': values[7:10]}


class RootMath(CameraMath):
    def __init__(self, exe):
        super().__init__(exe)
        self.allowed = [(0x293210, 0x2934d3), (0x29b0b0, 0x29b262),
                        (0x8d7a90, 0x8d7a9c), (0x8d32c0, 0x8d32d3), (0x8e2390, 0x8e239d),
                        (0x8d3490, 0x8d34ba), (0x336d50, 0x336d57)]
        self.factor, self.tick, self.update = 0.0, 0, False
        self.substitutions = set()

    def on_instruction(self, cpu, address, size, unused):
        rva = address - self.base
        if rva in (0x1237608, 0x1235810):
            value = struct.unpack('<f', struct.pack('<I', cpu.reg_read(UC_X86_REG_XMM0) & 0xffffffff))[0]
            name = 'acosf' if rva == 0x1237608 else 'sinf'
            value = math.acos(value) if name == 'acosf' else math.sin(value)
            floating = True
        else:
            if not any(lo <= rva < hi for lo, hi in self.allowed):
                raise ValueError(f'execution escaped validated interpolation: {rva:x}')
            return
        self.substitutions.add(name)
        cpu.reg_write(UC_X86_REG_XMM0 if floating else UC_X86_REG_RAX,
                      struct.unpack('<I', struct.pack('<f', value))[0] if floating else value)
        sp = cpu.reg_read(UC_X86_REG_RSP)
        cpu.reg_write(UC_X86_REG_RIP, struct.unpack('<Q', cpu.mem_read(sp, 8))[0])
        cpu.reg_write(UC_X86_REG_RSP, sp + 8)

    def interpolate(self, current, source, target, source_tick, newest_tick,
                    tick, fraction, update=False, inplace=False, rendered=None):
        self.factor, self.tick, self.update = fraction, tick, update
        render = (rendered or current) + source + target + struct.pack('<2I', source_tick, newest_tick)
        self.cpu.mem_write(self.data, render)
        self.cpu.mem_write(self.data + 0x1000, current)
        self.cpu.mem_write(self.data + 0x2000, b'\xcd' * 56)
        # Validated getters: context[0] -> world/task state; context+8 is a
        # one-based tick-table index. This fixture owns every pointed-to byte.
        self.cpu.mem_write(self.data + 0x3000, struct.pack('<QI', self.data + 0x6000, 1))
        self.cpu.mem_write(self.data + 0x606c, struct.pack('<2I', 100, 101 if update else 100))
        self.cpu.mem_write(self.data + 0x6088, struct.pack('<f', fraction))
        self.cpu.mem_write(self.data + 0x67d0, struct.pack('<Q', self.data + 0x7000))
        self.cpu.mem_write(self.data + 0x7000, struct.pack('<I', tick))
        self.cpu.reg_write(UC_X86_REG_R9, self.data + 0x1000)
        destination = self.data if inplace else self.data + 0x2000
        self.invoke(0x293210, destination, self.data, self.data + 0x3000)
        result = bytes(self.cpu.mem_read(destination, 56))
        history = bytes(self.cpu.mem_read(self.data, 176))
        if bytes(self.cpu.mem_read(self.data + 0x1000, 56)) != current:
            raise ValueError('native function unexpectedly changed CurrentTransform')
        return result, history


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=Path('docs/analysis/emulated_root_interpolation.json'))
    parser.add_argument('--world-dump', type=Path,
                        help='Optional saved 0x266380 world snapshot; never access a running game')
    parser.add_argument('--observations', type=Path, nargs='*', default=[],
                        help='Saved camera-observer.jsonl files with task context and raw transforms')
    args = parser.parse_args()
    native = RootMath(Path('enshrouded.exe'))
    checks = []

    def check(name, passed, details=None):
        checks.append({'name': name, 'status': 'PASS' if passed else 'FAIL', 'details': details})

    # Millimetre offsets on large coordinates: absolute world magnitude must not
    # erase the delta. No camera/headbone/HMD input is supplied to this routine.
    for start in [(0, 0, 0), (1_000_000, -2_000_000, 3_000_000)]:
        end = [p + d for p, d in zip(start, (.007, -.013, .019))]
        a, b = transform(start), transform(end)
        for gap in (1, 2, 5):
            for fraction in (0, .25, .5, 1):
                out, history = native.interpolate(b, a, b, 100, 100 + gap, 100 + gap, fraction)
                actual = decode(out)
                t = (gap - 1 + fraction) / gap
                expected = [x + (y - x) * t for x, y in zip(start, end)]
                error = max(abs(x - y) for x, y in zip(actual['position'], expected))
                check(f'large_world_{start[0]}_gap_{gap}_fraction_{fraction}', error < 1e-7, {'max_error_m': error})
                check(f'history_unchanged_{start[0]}_{gap}_{fraction}', history[56:] == a + b + struct.pack('<2I', 100, 100 + gap))
    a, b, current = transform((1, 2, 3)), transform((4, 8, 12)), transform((10, 20, 30))
    out, history = native.interpolate(current, a, b, 95, 100, 102, .5, update=True)
    check('new_tick_moves_old_target_to_source', history[56:112] == b)
    check('new_tick_copies_current_to_target', history[112:168] == current)
    check('new_tick_preserves_source_and_newest_ticks', struct.unpack_from('<2I', history, 168) == (100, 102))
    check('new_tick_interpolates_new_pair', decode(out)['position'] == [8.5, 17, 25.5])
    separate, _ = native.interpolate(current, a, b, 100, 101, 101, .5)
    inplace, _ = native.interpolate(current, a, b, 100, 101, 101, .5, inplace=True)
    check('output_aliasing_matches_separate_output', separate == inplace)
    shaken = transform((400, -500, 600), (0, 1, 0, 0))
    independent, _ = native.interpolate(current, a, b, 100, 101, 101, .5, rendered=shaken)
    check('prior_rendered_animation_offset_does_not_enter_root', independent == separate)
    for tick in (99, 100):
        out, _ = native.interpolate(current, a, b, 100, 100, tick, .5)
        check(f'non_increasing_tick_uses_one_interval_{tick}', decode(out)['position'] == [2.5, 5, 7.5])
    for angle in (0, .01, math.pi / 2, math.pi):
        q = (0, math.sin(angle / 2), 0, math.cos(angle / 2))
        for sign in (1, -1):
            a = transform((0, 0, 0), scale=(1, 2, 3))
            b = transform((0, 0, 0), tuple(sign * x for x in q), (3, 4, 5))
            out, _ = native.interpolate(b, a, b, 100, 101, 101, .5)
            result = decode(out)
            expected = (0, math.sin(angle / 4), 0, math.cos(angle / 4))
            error = min(max(abs(x - s * y) for x, y in zip(result['quaternion'], expected)) for s in (1, -1))
            check(f'shortest_arc_angle_{angle}_sign_{sign}', error < 2e-5, {'quaternion_error': error})
            check(f'scale_interpolated_{angle}_{sign}', result['scale'] == (2, 3, 4))
    # Original entity getter has a distinct zero-index branch, then a 16-byte
    # stride. The local-camera predicate compares this ID against aggregate+4.
    native.cpu.mem_write(native.data + 0x67e0, struct.pack('<Q', native.data + 0x7800))
    native.cpu.mem_write(native.data + 0x7800, struct.pack('<4I', 17, 91, 92, 93) +
                         struct.pack('<4I', 29, 81, 82, 83) + struct.pack('<4I', 43, 71, 72, 73))
    for index, entity in ((0, 0), (1, 17), (2, 29), (3, 43)):
        native.cpu.mem_write(native.data + 0x3008, struct.pack('<I', index))
        native.invoke(0x8d3490, native.data + 0x3000, native.data + 0x4000)
        actual = struct.unpack('<I', native.cpu.mem_read(native.data + 0x4000, 4))[0]
        check(f'entity_getter_index_{index}', actual == entity)
        for owner in (entity, entity + 1):
            native.cpu.mem_write(native.data + 0x4104, struct.pack('<I', owner))
            native.invoke(0x336d50, actual, native.data + 0x4100)
            check(f'local_entity_predicate_{index}_{owner}',
                  (native.cpu.reg_read(UC_X86_REG_RAX) & 255) == int(entity == owner))
    observed = None
    if args.world_dump:
        from analyze_camera_dump import Dump
        dump = Dump(args.world_dump)
        event = dump.streams[6][1]
        context_size, context_offset = dump.unpack('<II', event + 160)
        if context_size < 256:
            raise ValueError('missing x64 context')
        rip = dump.unpack('<Q', context_offset + 248)[0]
        game = next(m for m in dump.modules if m['base'] <= rip < m['base'] + m['size'])
        if game['timestamp'] != 1782724296 or rip - game['base'] != 0x266380:
            raise ValueError('dump is not the validated camera query site/build')
        rsp, task = (dump.unpack('<Q', context_offset + x)[0] for x in (152, 176))

        def read(address, fmt):
            data = dump.read(address, struct.calcsize(fmt))
            if data is None:
                raise ValueError(f'required snapshot memory absent: {address:x}')
            return struct.unpack(fmt, data)

        pointers = read(rsp + 0x50, '<10Q')
        state, index = read(task, '<QI')
        if not index:
            raise ValueError('world query index is zero')
        tick_table, = read(state + 0x7d0, '<Q')
        entity_table, = read(state + 0x7e0, '<Q')
        tick, = read(tick_table + (index - 1) * 4, '<I')
        entity, = read(entity_table + (index - 1) * 16, '<I')
        local_owner, = read(pointers[3] + 4, '<I')
        fraction, = read(state + 0x88, '<f')
        render = dump.read(pointers[2], 176)
        if render is None:
            raise ValueError('RenderTransform absent from snapshot')
        source_tick, newest_tick = struct.unpack_from('<2I', render, 168)
        # At the later camera task, interpolation has already updated history.
        # Reconstruct without consuming that update a second time. This changes
        # only fixture context memory, never the saved/live game's task state.
        output, _ = native.interpolate(render[112:168], render[56:112], render[112:168],
                                        source_tick, newest_tick, tick, fraction, update=False)
        actual, expected = decode(output), decode(render[:56])
        position_error = max(abs(x - y) for x, y in zip(actual['position'], expected['position']))
        rotation_error = max(abs(x - y) for x, y in zip(actual['quaternion'], expected['quaternion']))
        observed = {'dump': str(args.world_dump), 'entity': entity, 'local_owner': local_owner,
                    'tick': tick, 'source_tick': source_tick, 'newest_tick': newest_tick,
                    'fraction': fraction, 'reconstructed': actual, 'observed_render': expected,
                    'position_error_m': position_error, 'quaternion_error': rotation_error}
        check('saved_world_query_context_matches_slot_zero', task == pointers[0])
        check('saved_world_entity_matches_local_camera_owner', entity != 0 and entity == local_owner)
        check('saved_world_source_target_reconstructs_render_position', position_error < 1e-6)
        check('saved_world_source_target_reconstructs_render_rotation', rotation_error < 1e-5)
    observed_series = []
    for path in args.observations:
        import hashlib
        data = path.read_bytes()
        events = [json.loads(line) for line in data.decode('utf8').splitlines()]
        samples = [e for e in events if e['event'] == 'camera_query_sample']
        if not samples or not any(e['event'] == 'camera_hook_complete' and e['original_bytes_restored'] for e in events):
            raise ValueError('incomplete recorded camera observer')
        errors = []
        for sample in samples:
            context = sample['task_context']
            if not context['valid'] or not sample['query_read'] or not sample['components'][2]['readable']:
                raise ValueError('invalid saved query context')
            raw = bytes.fromhex(sample['components'][2]['bytes'])
            source_tick, target_tick = struct.unpack_from('<2I', raw, 168)
            out, history = native.interpolate(raw[112:168], raw[56:112], raw[112:168],
                source_tick, target_tick, context['tick'], context['fraction'], update=False)
            expected, actual = decode(raw[:56]), decode(out)
            errors.append(dict(sample=sample['sample'],
                position_error_m=max(abs(x-y) for x,y in zip(expected['position'], actual['position'])),
                rotation_error=max(abs(x-y) for x,y in zip(expected['quaternion'], actual['quaternion'])),
                history_preserved=history[56:]==raw[56:176],
                local=context['local_entity'] and context['entity']==context['local_owner'] and context['entity']!=0))
        prefix = str(path.parent.name)
        check(prefix+':all_local_entity_samples', all(e['local'] for e in errors))
        check(prefix+':all_root_positions_reconstructed', all(e['position_error_m'] < 1e-6 for e in errors))
        check(prefix+':all_root_rotations_reconstructed', all(e['rotation_error'] < 1e-5 for e in errors))
        check(prefix+':source_target_history_preserved', all(e['history_preserved'] for e in errors))
        observed_series.append(dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(), samples=errors))
    report = {'checks': checks, 'pass': sum(c['status'] == 'PASS' for c in checks),
              'fail': sum(c['status'] == 'FAIL' for c in checks), 'substitutions': sorted(native.substitutions),
              'saved_world_snapshot': observed,
              'saved_observer_series': observed_series,
              'limitations': 'Synthetic context tests plus optional saved world snapshot; task ordering across animation/teleport/scene transitions remains unverified.'}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps({k: v for k, v in report.items() if k not in ('checks','saved_observer_series')}))
    print('observer series', len(observed_series), 'samples', sum(len(s['samples']) for s in observed_series))
    for c in checks:
        if c['status'] == 'FAIL':
            print(json.dumps(c))
    raise SystemExit(bool(report['fail']))


if __name__ == '__main__':
    main()
