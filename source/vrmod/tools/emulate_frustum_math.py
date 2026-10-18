"""Execute the game's allowlisted CPU frustum builder on asymmetric eye inputs.

No game process, hooks, allocations in the game, imports, or OS calls are used.
This checks CPU frustum geometry, not GPU culling or complete native stereo.
"""
import argparse
import json
import math
import pathlib
import struct
from emulate_camera_math import CameraMath, SHA256
from unicorn.x86_const import UC_X86_REG_RAX


class FrustumMath(CameraMath):
    def __init__(self, exe):
        super().__init__(exe)
        # Validated local disassembly, including chained-unwind fragments.
        self.allowed += [(0xd647e0, 0xd64880), (0xd64880, 0xd6500b),
                         (0xd7cf00, 0xd7d598), (0xd640c0, 0xd64205),
                         (0xd62f10, 0xd630aa), (0x2d840, 0x2db33)]

    def frustum(self, camera_position, yaw, bounds, near, far, reverse):
        matrix = self.view([round(p * 2**32) for p in camera_position],
                           [0, math.sin(yaw/2), 0, math.cos(yaw/2)])
        descriptor = bytearray(80)
        struct.pack_into('<I6f', descriptor, 0, 3, *bounds, near, far)
        descriptor[0x48] = int(reverse)
        self.cpu.mem_write(self.data + 0x400, struct.pack('<16f', *matrix))
        self.cpu.mem_write(self.data + 0x500, bytes(descriptor))
        self.cpu.mem_write(self.data + 0x600, b'\xcd' * 128)
        self.invoke(0xd647e0, self.data + 0x600, self.data + 0x400, self.data + 0x500)
        success = (self.cpu.reg_read(UC_X86_REG_RAX) & 0xff) != 0
        planes = [struct.unpack('<4f', self.cpu.mem_read(self.data + 0x600 + i*16, 16)) for i in range(6)]
        return success, planes


def transform(position, yaw, point):
    x, y, z = point
    c, s = math.cos(yaw), math.sin(yaw)
    return [position[0] + c*x + s*z, position[1] + y, position[2] - s*x + c*z]


def evaluate(planes, point):
    return [sum(a*b for a, b in zip(plane[:3], point)) + plane[3] for plane in planes]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=pathlib.Path, default=pathlib.Path('docs/analysis/emulated_frustum_math.json'))
    args = parser.parse_args()
    engine = FrustumMath(pathlib.Path('enshrouded.exe'))
    rows = []
    def check(name, okay, **details):
        rows.append(dict(name=name, status='PASS' if okay else 'FAIL', **details))
        print(rows[-1]['status'], name)
    for reverse in (False, True):
        for angle in (0, math.pi/2, -math.pi/2, math.pi):
            for eye in (0, 1):
                name = f'reverse{int(reverse)}_yaw{angle:.3f}_eye{eye}'
                position = [-.032 if eye == 0 else .032, 1.7, 2]
                near, far = .05, 100
                bounds = (-.06, .04, -.035, .045) if eye == 0 else (-.04, .06, -.035, .045)
                okay, planes = engine.frustum(position, angle, bounds, near, far, reverse)
                check(name + '_build', okay and all(math.isfinite(v) for p in planes for v in p), planes=planes)
                center = [(bounds[0]+bounds[1])*10, (bounds[2]+bounds[3])*10, 1]
                inside = evaluate(planes, transform(position, angle, center))
                # Confirmed from the original cross-product order: normals point outward.
                check(name + '_interior', max(inside) < 0, distances=inside)
                normalized = [math.sqrt(sum(v*v for v in plane[:3])) for plane in planes]
                check(name + '_unit_normals', max(abs(v-1) for v in normalized) < 2e-5)
                for boundary, x, y, z in [('left', bounds[0]*20, center[1], 1),
                                          ('right', bounds[1]*20, center[1], 1),
                                          ('bottom', center[0], bounds[2]*20, 1),
                                          ('top', center[0], bounds[3]*20, 1),
                                          ('near', center[0]*near, center[1]*near, near),
                                          ('far', center[0]*far, center[1]*far, far)]:
                    distances = evaluate(planes, transform(position, angle, [x, y, z]))
                    # The game's general float32 inverse/unprojection loses precision
                    # at far/near=2000 for both depth conventions. Keep the far error
                    # explicit (5cm at 100m); side/near tests remain 20 microunits.
                    tolerance = .05 if boundary == 'far' else 2e-5
                    check(name + '_boundary_' + boundary, abs(max(distances)) < tolerance, distances=distances, tolerance=tolerance)
                outside = [[bounds[0]*24, center[1], 1], [bounds[1]*24, center[1], 1],
                           [center[0], bounds[2]*24, 1], [center[0], bounds[3]*24, 1],
                           [center[0]*near/2, center[1]*near/2, near/2],
                           [center[0]*far*1.2, center[1]*far*1.2, far*1.2]]
                check(name + '_outside_rejected', all(max(evaluate(planes, transform(position, angle, p))) > .001 for p in outside))
    report = dict(executable_sha256=SHA256, scope=__doc__, checks=rows,
                  passed=sum(row['status'] == 'PASS' for row in rows), failed=sum(row['status'] == 'FAIL' for row in rows),
                  substitutions=sorted(set(engine.calls)), function_ranges=[[hex(a), hex(b)] for a, b in engine.allowed])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, allow_nan=False), encoding='utf8')
    print('RESULT', report['passed'], 'PASS', report['failed'], 'FAIL')
    return int(report['failed'] != 0)


if __name__ == '__main__':
    raise SystemExit(main())
