"""OFFLINE local-body ablation. Preserves the captured live head exclusion.

Adds a visibility exclusion only for validated equipment instance IDs in the two
captured eye cameras and main subview. Never changes the RDC or running game.
Generated game shader bytes are local evidence, not redistributable source.
"""
import argparse,hashlib,json,pathlib,re,struct,subprocess
from build_culling_reference_patch import TOOL
EXPECTED='8cfcb566b2c9afdc4f0bca1e026530b0fb70abf93fa0f8719d64464c48028c9b'
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('shader',type=pathlib.Path);p.add_argument('output',type=pathlib.Path)
 p.add_argument('--equipment-probe',type=pathlib.Path,required=True);p.add_argument('--parts',nargs='+',type=int,required=True)
 p.add_argument('--view-constant',type=pathlib.Path,nargs=2,required=True);a=p.parse_args()
 raw=a.shader.read_bytes()
 if len(raw)!=25796 or hashlib.sha256(raw).hexdigest()!=EXPECTED:raise ValueError('unreviewed live head shader')
 parts=sorted(set(a.parts))
 if not parts or any(n not in range(4,24) or n==15 for n in parts):raise ValueError('body slots 4..14,16..23 only; head policy is retained')
 samples=[json.loads(l) for l in (a.equipment_probe/'view-resource-probe.jsonl').read_text().splitlines() if l.strip()]
 samples=[e for e in samples if e['event']=='local_equipment_observation']
 if len(samples)!=20 or len({e['entity'] for e in samples})!=1:raise ValueError('bounded local equipment observations required')
 selections=[{m['handle']&0xffffff for m in e['models'] if m['part'] in parts and m['handle']} for e in samples]
 ids=sorted(selections[0])
 if not ids or any(set(ids)!=s for s in selections) or max(ids)>=65536:raise ValueError('equipment changed/unavailable')
 cameras=[]
 for f in a.view_constant:
  b=f.read_bytes()
  if len(b)!=4144 or not any(b[4132:]):raise ValueError('captured head-enabled eye constants required')
  cameras.append(struct.unpack_from('<3I',b,4112))
 if cameras[0]==cameras[1]:raise ValueError('two distinct eye gates required')
 a.output.mkdir(parents=True,exist_ok=False)
 original=a.output/'original.spvasm';subprocess.run([str(TOOL/'spirv-dis.exe'),str(a.shader),'-o',str(original)],check=True)
 text=original.read_text();match=re.search(r'(?m)^\s*%781 = OpSelect %uint (%[0-9]+) %uint_1 %uint_0$',text)
 if not match or text.count('%331 = OpLoad %uint %330')!=1 or text.count('%745 = OpCompositeExtract %uint %743 1')!=1:raise ValueError('SSA boundary differs')
 declarations=[f'%body_id_{n} = OpConstant %uint {v}' for n,v in enumerate(ids)]
 declarations += [f'%body_camera_{e}_{n} = OpConstant %uint {v}' for e,c in enumerate(cameras) for n,v in enumerate(c)]
 text,n=re.subn(r'(?m)^(\s*%2 = OpFunction %void None %292)$','\n'.join(declarations)+'\n'+r'\1',text)
 if n!=1:raise ValueError('entry boundary differs')
 body=[]
 for n in range(3):body += [f'%body_ptr_{n} = OpAccessChain %_ptr_Uniform_uint %4 %int_14 %int_0 %int_{n}',f'%body_cam_{n} = OpLoad %uint %body_ptr_{n}']
 for e in range(2):
  body += [f'%body_cam_match_{e}_{n} = OpIEqual %bool %body_cam_{n} %body_camera_{e}_{n}' for n in range(3)]
  body += [f'%body_xy_{e} = OpLogicalAnd %bool %body_cam_match_{e}_0 %body_cam_match_{e}_1',f'%body_xyz_{e} = OpLogicalAnd %bool %body_xy_{e} %body_cam_match_{e}_2']
 body += ['%body_eye = OpLogicalOr %bool %body_xyz_0 %body_xyz_1','%body_main = OpIEqual %bool %745 %uint_0','%body_scope = OpLogicalAnd %bool %body_eye %body_main']
 selected='%body_hit_0'
 for n in range(len(ids)):
  body += [f'%body_hit_{n} = OpIEqual %bool %331 %body_id_{n}']
  if n:body += [f'%body_union_{n} = OpLogicalOr %bool {selected} %body_hit_{n}'];selected=f'%body_union_{n}'
 body += [f'%body_hide = OpLogicalAnd %bool %body_scope {selected}','%body_keep = OpLogicalNot %bool %body_hide',f'%body_visible = OpLogicalAnd %bool {match[1]} %body_keep','%781 = OpSelect %uint %body_visible %uint_1 %uint_0']
 text,n=re.subn(r'(?m)^\s*%781 = OpSelect %uint %[0-9]+ %uint_1 %uint_0$','\n'.join(body),text)
 if n!=1:raise ValueError('visibility boundary differs')
 assembly=a.output/'experiment.spvasm';binary=a.output/'experiment.spv';assembly.write_text(text)
 subprocess.run([str(TOOL/'spirv-as.exe'),'--target-env','spv1.5',str(assembly),'-o',str(binary)],check=True)
 validator=pathlib.Path('references/vulkan-sdk/tools/Bin/spirv-val.exe')
 subprocess.run([str(validator),'--target-env','vulkan1.2','--scalar-block-layout',str(binary)],check=True)
 (a.output/'recipe.json').write_text(json.dumps(dict(offline_only=True,original_sha256=EXPECTED,patched_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),indices=ids,parts=parts,subview=0,camera_position_gates=cameras,equipment_probe=str(a.equipment_probe),head_policy_unchanged=True,limitation=__doc__),indent=2))
 print('PASS offline body recipe',parts,ids,binary)
if __name__=='__main__':main()
