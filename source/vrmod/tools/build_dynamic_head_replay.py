"""Offline only: feed captured eye gates into the LIVE algorithm's three payload loads.

The live C++ transformer contains no fixed equipment/camera values. This wrapper
supplies a reproducible test payload for a saved capture without editing the RDC.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from build_culling_reference_patch import TOOL, EXPECTED


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('dynamic_shader',type=Path)
    p.add_argument('output',type=Path)
    p.add_argument('--indices',type=int,nargs='+',required=True)
    p.add_argument('--view-constant',type=Path,nargs='+',required=True)
    a=p.parse_args();ids=sorted(set(a.indices))
    if not 1<=len(ids)<=5 or not all(0<=i<65536 for i in ids) or not 1<=len(a.view_constant)<=2:
        raise ValueError('bounded offline head experiment')
    a.output.mkdir(parents=True,exist_ok=True)
    source=a.output/'dynamic.spvasm'
    subprocess.run([str(TOOL/'spirv-dis.exe'),str(a.dynamic_shader),'-o',str(source)],check=True)
    text=source.read_text()
    payload=[0,0,0]
    for n,i in enumerate(ids):payload[n//2]|=i<<(16*(n%2))
    payload[2]|=(0xa5d0|len(ids))<<16
    cameras=[]
    for path in a.view_constant:
        data=path.read_bytes()
        if len(data)!=4144 or data[4132:]!=bytes(12):raise ValueError('native padding is not zero')
        cameras.append(struct.unpack_from('<3I',data,4112))
    declarations=[f'%evr_payload_{n} = OpConstant %uint {v}' for n,v in enumerate(payload)]
    declarations += [f'%evr_camera_{eye}_{n} = OpConstant %uint {v}' for eye,c in enumerate(cameras) for n,v in enumerate(c)]
    text,n=re.subn(r'(?m)^(\s*%2 = OpFunction %void None %292)$','\n'.join(declarations)+'\n'+r'\1',text)
    if n!=1:raise ValueError('entry changed')
    gate=[]
    for n in range(3):
        gate += [f'%evr_camera_ptr_{n} = OpAccessChain %_ptr_Uniform_uint %4 %int_14 %int_0 %int_{n}',
                 f'%evr_camera_value_{n} = OpLoad %uint %evr_camera_ptr_{n}']
    for eye in range(len(cameras)):
        gate += [f'%evr_cam_match_{eye}_{n} = OpIEqual %bool %evr_camera_value_{n} %evr_camera_{eye}_{n}' for n in range(3)]
        gate += [f'%evr_xy_{eye} = OpLogicalAnd %bool %evr_cam_match_{eye}_0 %evr_cam_match_{eye}_1',
                 f'%evr_xyz_{eye} = OpLogicalAnd %bool %evr_xy_{eye} %evr_cam_match_{eye}_2']
    selected='%evr_xyz_0'
    if len(cameras)==2:
        gate += ['%evr_either = OpLogicalOr %bool %evr_xyz_0 %evr_xyz_1'];selected='%evr_either'
    for n,(value,ptr) in enumerate([(919,918),(922,921),(925,924)]):
        replacement=(gate if n==0 else [])+[f'%evr_native_{n} = OpLoad %uint %{ptr}',
             f'%{value} = OpSelect %uint {selected} %evr_payload_{n} %evr_native_{n}']
        text,count=re.subn(rf'(?m)^\s*%{value} = OpLoad %uint %{ptr}$','\n'.join(replacement),text)
        if count!=1:raise ValueError('live payload load differs')
    assembly=a.output/'experiment.spvasm';binary=a.output/'experiment.spv';assembly.write_text(text)
    subprocess.run([str(TOOL/'spirv-as.exe'),'--target-env','spv1.5',str(assembly),'-o',str(binary)],check=True)
    (a.output/'recipe.json').write_text(json.dumps(dict(offline_only=True,original_sha256=EXPECTED,
        dynamic_sha256=hashlib.sha256(a.dynamic_shader.read_bytes()).hexdigest(),patched_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),
        indices=ids,subview=0,payload=payload,camera_position_gates=cameras,
        changes='Only supplies payload test values; uses unchanged live dynamic exclusion instructions'),indent=2))
    print('PASS offline input wrapper for dynamic shader',binary)


if __name__=='__main__':main()
