"""Run allowlisted math instructions from the local PE inside Unicorn, never in the game.

No loader, imports, process access, or host syscalls are emulated. The bounded CPU
fixture is specific to the recorded build. Game bytes are not copied into source.
"""
import argparse
import hashlib
import json
import math
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).parent / 'python'))
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_64, UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_RSP, UC_X86_REG_RIP, UC_X86_REG_RCX,
                               UC_X86_REG_RDX, UC_X86_REG_R8, UC_X86_REG_XMM0,
                               UC_X86_REG_MXCSR)

# Full binary identity, independent of ASLR and any running process.
SHA256 = 'af2f5a1227911d8aa06b3908d6bd0211838211cae14ea91099cb57d0df990781'


class CameraMath:
    def __init__(self, exe):
        data = exe.read_bytes()
        if hashlib.sha256(data).hexdigest() != SHA256:
            raise ValueError('unsupported executable; revalidate function ranges first')
        pe = pefile.PE(data=data)
        self.base = pe.OPTIONAL_HEADER.ImageBase
        self.cpu = Uc(UC_ARCH_X86, UC_MODE_64)
        image = pe.get_memory_mapped_image()
        self.cpu.mem_map(self.base, (pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
        self.cpu.mem_write(self.base, image)
        self.stack = 0x6000000000
        self.data = 0x6100000000
        self.stop = 0x6200000000
        self.cpu.mem_map(self.stack, 0x10000)
        self.cpu.mem_map(self.data, 0x10000)
        self.cpu.mem_map(self.stop, 0x1000)
        self.allowed = [(0xd61400, 0xd6142a), (0x141b30, 0x141efb)]  # validated leaf routines, no .pdata entry
        for start in [0xe4a50, 0xd65b60, 0xd7a0c0, 0x1d3a0, 0xd7ce70, 0xd7cb10]:
            f = next((f for f in pe.DIRECTORY_ENTRY_EXCEPTION if f.struct.BeginAddress == start), None)
            if f is None:
                raise ValueError(f'missing validated function extent: {start:x}')
            self.allowed.append((start, f.struct.EndAddress))
        self.cpu.hook_add(UC_HOOK_CODE, self.on_instruction)
        self.calls = []

    def on_instruction(self, cpu, address, size, unused):
        rva = address - self.base
        if rva == 0x1234c90:
            # The symmetric branch calls tanf. All matrix arithmetic and the
            # asymmetric branch execute the original instructions unchanged.
            x = struct.unpack('<f', struct.pack('<I', cpu.reg_read(UC_X86_REG_XMM0)&0xffffffff))[0]
            cpu.reg_write(UC_X86_REG_XMM0, struct.unpack('<I', struct.pack('<f', math.tan(x)))[0])
            sp = cpu.reg_read(UC_X86_REG_RSP)
            cpu.reg_write(UC_X86_REG_RIP, struct.unpack('<Q', cpu.mem_read(sp, 8))[0])
            cpu.reg_write(UC_X86_REG_RSP, sp+8)
            self.calls.append('tanf substituted with rounded Python math.tan')
            return
        if not any(lo <= rva < hi for lo, hi in self.allowed):
            raise ValueError(f'execution escaped allowlisted math: {rva:x}')

    def invoke(self, rva, rcx=0, rdx=0, r8=0):
        sp = self.stack + 0xf008
        self.cpu.mem_write(sp, struct.pack('<Q', self.stop))
        for reg, value in [(UC_X86_REG_RSP, sp), (UC_X86_REG_RCX, rcx),
                           (UC_X86_REG_RDX, rdx), (UC_X86_REG_R8, r8), (UC_X86_REG_MXCSR, 0x1f80)]:
            self.cpu.reg_write(reg, value)
        self.cpu.emu_start(self.base+rva, self.stop, timeout=1_000_000, count=20000)
        if self.cpu.reg_read(UC_X86_REG_RIP) != self.stop:
            raise ValueError('bounded function invocation did not return')

    def projection(self, kind, values, reverse=True):
        descriptor = bytearray(80)
        struct.pack_into('<I', descriptor, 0, kind)
        struct.pack_into('<'+'f'*len(values), descriptor, 4, *values)
        descriptor[0x48] = int(reverse)
        return self.projection_descriptor(bytes(descriptor))

    def projection_descriptor(self, descriptor):
        if len(descriptor) != 80:
            raise ValueError('projection descriptor must contain 80 bytes')
        self.cpu.mem_write(self.data, bytes(descriptor))
        self.invoke(0xd65b60, self.data+0x100, self.data)
        return list(struct.unpack('<16f', self.cpu.mem_read(self.data+0x100, 64)))

    def view(self, position_q32, quaternion, origin_q32=(0, 0, 0)):
        source = struct.pack('<3q4f3fI', *position_q32, *quaternion, 1, 1, 1, 0)
        self.cpu.mem_write(self.data, source)
        self.cpu.mem_write(self.data+0x200, struct.pack('<3q', *origin_q32))
        self.invoke(0xd7a0c0, self.data, self.data+0x100, self.data+0x200)
        self.invoke(0xd7ce70, self.data+0x100)
        return list(struct.unpack('<16f', self.cpu.mem_read(self.data+0x100, 64)))


def project(matrix, vector):
    """Raw game matrices use row vectors; affine CPU matrices omit the last column."""
    v = [*vector, 1]
    clip = [sum(v[r]*matrix[4*r+c] for r in range(4)) for c in range(4)]
    return [x/clip[3] for x in clip[:3]]


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--exe', type=pathlib.Path, default=pathlib.Path('enshrouded.exe'))
    p.add_argument('--output', type=pathlib.Path, default=pathlib.Path('docs/analysis/emulated_camera_math.json'))
    p.add_argument('--adapter-vectors', type=pathlib.Path, help='JSON emitted by evr_game_camera_tests --vectors')
    p.add_argument('--stereo-vectors', type=pathlib.Path, help='Serialized game descriptors from evr_game_stereo_tests')
    a = p.parse_args()
    engine = CameraMath(a.exe)
    rows = []
    def check(name, actual, expected, tolerance=2e-5):
        error = max(abs(x-y) for x,y in zip(actual, expected))
        ok = len(actual)==len(expected) and all(math.isfinite(x) for x in actual) and error <= tolerance
        rows.append(dict(name=name, status='PASS' if ok else 'FAIL', actual=actual, expected=expected, max_error=error))
        print(rows[-1]['status'], name)
    matrices = {}
    for reverse in (False, True):
        for near, far in [(0.01, 10000), (0.1, 100), (0.5, 2)]:
            m = engine.projection(0, [16/9, near, far, math.radians(65)], reverse)
            key=f'symmetric_{near}_{far}_{reverse}'
            matrices[key]=m
            check(key+'_near', project(m, [0,0,near]), [0,0,1 if reverse else 0])
            check(key+'_far', project(m, [0,0,far]), [0,0,0 if reverse else 1])
            t = math.tan(math.radians(65)/2)
            check(key+'_top_right', project(m, [near*t*16/9,near*t,near])[:2], [1,1])
    # Asymmetric frustum branch runs entirely original instructions (no tanf).
    near, far = .05, 500
    l,r,b,t = -.04,.06,-.03,.05
    m = engine.projection(3, [l,r,b,t,near,far], True)
    matrices['asymmetric_reverse']=m
    check('asymmetric_bottom_left', project(m, [l,b,near]), [-1,-1,1])
    check('asymmetric_top_right', project(m, [r,t,near]), [1,1,1])
    check('asymmetric_far', project(m, [(l+r)/2*far/near,(b+t)/2*far/near,far]), [0,0,0])
    for angle in (0, math.pi/2, -math.pi/2, math.pi):
        m=engine.view([int(x*2**32) for x in (100,200,300)], [0,math.sin(angle/2),0,math.cos(angle/2)])
        # CPU Matrix43 occupies four 16-byte rows; the padding isn't homogeneous data.
        for i in (3,7,11):m[i]=0
        m[15]=1
        check(f'view_origin_yaw{angle}', project(m,[100,200,300]),[0,0,0],5e-5)
        check(f'view_forward_yaw{angle}',project(m,[100+math.sin(angle),200,300+math.cos(angle)]),[0,0,1],5e-5)
        matrices[f'view_yaw{angle}']=m
    if a.adapter_vectors:
        vectors=json.loads(a.adapter_vectors.read_text())
        if not 0 < len(vectors) <= 1024:
            raise ValueError('unexpected adapter vector count')
        for index, case in enumerate(vectors):
            left,right,top,bottom=map(math.tan,case['fov'])
            n,far=case['near'],case['far']
            actual=engine.projection(3,[left*n,right*n,bottom*n,top*n,n,far],True)
            check(f'adapter_projection_{index}',case['projection'],actual,2e-5)
            actual=engine.view(case['position_q32'],case['orientation'],case['origin_q32'])
            # CPU affine fourth-column bytes have no defined semantic value.
            indices=[i for i in range(16) if i%4!=3]
            check(f'adapter_view_{index}',[case['view'][i] for i in indices],[actual[i] for i in indices],2e-5)
    if a.stereo_vectors:
        vectors=json.loads(a.stereo_vectors.read_text())
        if not 0 < len(vectors) <= 1024:
            raise ValueError('unexpected stereo vector count')
        for index,case in enumerate(vectors):
            descriptor=bytes.fromhex(case['descriptor'])
            if len(descriptor)!=80 or struct.unpack_from('<I',descriptor)[0] not in (0,3) or descriptor[0x48]!=1 or struct.unpack_from('<I',descriptor,0x44)[0]!=0:
                raise ValueError('unexpected stereo descriptor encoding')
            if struct.unpack_from('<I',descriptor)[0]==0:
                aspect,near,far,fov=struct.unpack_from('<4f',descriptor,4)
                if not all(math.isfinite(x) for x in (aspect,near,far,fov)) or not (aspect>0 and 0<near<far and 0<fov<math.pi):
                    raise ValueError('invalid native perspective vector')
            actual=engine.projection_descriptor(descriptor)
            check(f'serialized_stereo_descriptor_{index}',case['projection'],actual,2e-5)
    report=dict(executable_sha256=SHA256,scope='isolated CPU math only; not game rendering',
                function_ranges=[[hex(l),hex(h)] for l,h in engine.allowed],
                substitutions=sorted(set(engine.calls)), matrices=matrices, checks=rows,
                passed=sum(r['status']=='PASS' for r in rows),failed=sum(r['status']=='FAIL' for r in rows))
    a.output.parent.mkdir(parents=True, exist_ok=True)
    a.output.write_text(json.dumps(report,indent=2,allow_nan=False),encoding='utf8')
    return 1 if report['failed'] else 0


if __name__=='__main__':
    sys.exit(main())
