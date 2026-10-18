"""Create an OFFLINE replay experiment from a pinned local game shader.

This script is our patch recipe; neither generated shader nor disassembly is distributable.
It changes the pass visibility boolean only for selected instance indices/subview.
It is not a live-game injection or a production first-person implementation.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import struct

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = '04ceb2a4bc7dab905ae022fcbb8464dde3cd78516ecde5978f928d0e4a7b8ff9'
TOOL = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/plugins/spirv'


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('shader', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--indices', type=int, nargs='+', required=True)
    p.add_argument('--subview', type=int, required=True)
    p.add_argument('--view-constant', type=Path, nargs='+', help='Offline only: gate replacement to one or two captured eye camera positions')
    a = p.parse_args()
    ids = sorted(set(a.indices))
    if not 1 <= len(ids) <= 5 or not all(0 <= i < 65536 for i in ids) or not 0 <= a.subview < 11:
        raise ValueError('bounded experiment: 1..5 CPU model indices, one known subview')
    raw = a.shader.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED:
        raise ValueError('unreviewed shader, refusing patch')
    a.output.mkdir(parents=True, exist_ok=True)
    cameras=[]
    if a.view_constant:
        if len(a.view_constant)>2:
            raise ValueError('at most two eye gates')
        for path in a.view_constant:
            constants=path.read_bytes()
            if len(constants)!=4144:
                raise ValueError('camera gate requires captured 4144-byte native constants')
            cameras.append(struct.unpack_from('<3I',constants,4112))
    original = a.output / 'original.spvasm'
    subprocess.run([str(TOOL/'spirv-dis.exe'), str(a.shader), '-o', str(original)], check=True)
    text = original.read_text()
    # Names are emitted by the pinned bundled disassembler, validated against SSA dataflow.
    required = ['%331 = OpLoad %uint %330', '%330 = OpAccessChain %_ptr_Input_uint %gl_GlobalInvocationID %int_0',
                '%745 = OpCompositeExtract %uint %743 1', '%780 = OpPhi %bool %753 %740 %779 %762']
    if any(text.count(s) != 1 for s in required):
        raise ValueError('expected invocation/pass dataflow changed')
    declarations = [f'%evr_subview = OpConstant %uint {a.subview}']
    declarations += [f'%evr_id_{n} = OpConstant %uint {i}' for n, i in enumerate(ids)]
    for eye,camera in enumerate(cameras):
        declarations += [f'%evr_camera_{eye}_{n} = OpConstant %uint {v}' for n,v in enumerate(camera)]
    text, count = re.subn(r'(?m)^(\s*%2 = OpFunction %void None %292)$', '\n'.join(declarations)+'\n'+r'\1', text)
    if count != 1:
        raise ValueError('function boundary mismatch')
    extra = [f'%evr_hit_{n} = OpIEqual %bool %331 %evr_id_{n}' for n in range(len(ids))]
    union = '%evr_hit_0'
    for n in range(1, len(ids)):
        extra.append(f'%evr_union_{n} = OpLogicalOr %bool {union} %evr_hit_{n}')
        union = f'%evr_union_{n}'
    if cameras:
        for n in range(3):
            extra += [f'%evr_camera_ptr_{n} = OpAccessChain %_ptr_Uniform_uint %4 %int_14 %int_0 %int_{n}',
                      f'%evr_camera_value_{n} = OpLoad %uint %evr_camera_ptr_{n}']
        for eye in range(len(cameras)):
            extra += [f'%evr_camera_matches_{eye}_{n} = OpIEqual %bool %evr_camera_value_{n} %evr_camera_{eye}_{n}' for n in range(3)]
            extra += [f'%evr_camera_xy_{eye} = OpLogicalAnd %bool %evr_camera_matches_{eye}_0 %evr_camera_matches_{eye}_1',
                      f'%evr_camera_xyz_{eye} = OpLogicalAnd %bool %evr_camera_xy_{eye} %evr_camera_matches_{eye}_2']
        camera_union='%evr_camera_xyz_0'
        if len(cameras)==2:
            extra += ['%evr_either_camera = OpLogicalOr %bool %evr_camera_xyz_0 %evr_camera_xyz_1']
            camera_union='%evr_either_camera'
        extra += [f'%evr_selected_instance = OpLogicalAnd %bool {union} {camera_union}']
        union='%evr_selected_instance'
    extra += ['%evr_in_subview = OpIEqual %bool %745 %evr_subview',
              f'%evr_excluded = OpLogicalAnd %bool {union} %evr_in_subview',
              '%evr_allowed = OpLogicalNot %bool %evr_excluded',
              '%evr_visible = OpLogicalAnd %bool %780 %evr_allowed',
              '%781 = OpSelect %uint %evr_visible %uint_1 %uint_0']
    text, count = re.subn(r'(?m)^\s*%781 = OpSelect %uint %780 %uint_1 %uint_0$', '\n'.join(extra), text)
    if count != 1:
        raise ValueError('pass mask producer mismatch')
    assembly, binary = a.output / 'experiment.spvasm', a.output / 'experiment.spv'
    assembly.write_text(text)
    subprocess.run([str(TOOL/'spirv-as.exe'), '--target-env', 'spv1.5', str(assembly), '-o', str(binary)], check=True)
    subprocess.run([str(TOOL/'spirv-cross.exe'), str(binary), '--hlsl', '--shader-model', '60', '--output', str(a.output/'experiment.hlsl')], check=True)
    # SPIRV-Cross parsing and assembly are structural checks, not Vulkan validation.
    (a.output/'recipe.json').write_text(json.dumps(dict(offline_only=True, original_sha256=EXPECTED,
        patched_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(), indices=ids, subview=a.subview,camera_position_gates=cameras,
        changes='pass visibility only; no binding/layout changes; no live game writes',
        limitation='Assembly and cross-compilation succeeded; GPU replay comparison still required.'), indent=2))
    print(f'PASS offline experiment assembled: {binary}')


if __name__ == '__main__':
    main()
