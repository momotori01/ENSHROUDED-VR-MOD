"""OFFLINE ONLY: local-equipment near-plane clip in captured depth/color VS.

Ablation distance is a test parameter, not a product/HMD pose deadzone. Writes
ClipDistance without changing Position or any existing varyings. Requires a
capture explicitly created with shaderClipDistance enabled. Game shader bytes
remain local analysis artifacts and are never embedded in distributable code.
"""
import argparse,hashlib,json,math,pathlib,re,struct,subprocess
from build_culling_reference_patch import TOOL
SPECS={
 'depth':dict(sha='e7ddf2d6f02a752e57e06f9a9ca780421c30c711ccfe0dcbadab6364313d8934',size=31688,cb=480,offset=336,rows=[1193,1194,1195,1196],instance=541,position=1202,entry='%2 = OpFunction %void None %475'),
 'color':dict(sha='fd8ea5dffef32fd659442ec9fce7011f4ede8703e1a34380c10eac39c361ff8a',size=55756,cb=992,offset=816,rows=[2012,2013,2014,2015],instance=898,position=2108),
}
def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('shader',type=pathlib.Path);p.add_argument('output',type=pathlib.Path)
 p.add_argument('--mode',choices=SPECS,required=True);p.add_argument('--distance',type=float,required=True)
 p.add_argument('--equipment-probe',type=pathlib.Path,required=True);p.add_argument('--parts',type=int,nargs='+',required=True)
 p.add_argument('--view-constant',type=pathlib.Path,nargs=2,required=True);a=p.parse_args();spec=SPECS[a.mode]
 if not math.isfinite(a.distance) or not .01<=a.distance<=1:raise ValueError('bounded experimental clip distance')
 raw=a.shader.read_bytes()
 if len(raw)!=spec['size'] or hashlib.sha256(raw).hexdigest()!=spec['sha']:raise ValueError('unreviewed native vertex shader')
 launch=a.equipment_probe.parent;config=json.loads((launch/'launch.json').read_text(encoding='utf-8-sig'))
 logs=[json.loads(l) for l in (launch/'vulkan.jsonl').read_text().splitlines() if l.strip()]
 features=[e for e in logs if e['event']=='body_clip_feature_result']
 if not config.get('body_clip_prepare') or len(features)!=1 or not features[0]['enabled'] or features[0]['result']!=0:raise ValueError('capture requires verified clip feature preparation')
 parts=sorted(set(a.parts))
 if not parts or any(n not in range(4,24) or n==15 for n in parts):raise ValueError('local body parts only')
 samples=[json.loads(l) for l in (a.equipment_probe/'view-resource-probe.jsonl').read_text().splitlines() if l.strip()]
 samples=[e for e in samples if e['event']=='local_equipment_observation']
 selections=[{m['handle']&0xffffff for m in e['models'] if m['part'] in parts and m['handle']} for e in samples]
 if len(samples)!=20 or len({e['entity'] for e in samples})!=1 or not selections[0] or any(s!=selections[0] for s in selections):raise ValueError('current stable local equipment required')
 ids=sorted(selections[0]);matrices=[]
 for f in a.view_constant:
  b=f.read_bytes()
  if len(b)!=spec['cb']:raise ValueError('native constant layout changed')
  matrices.append(struct.unpack_from('<16I',b,spec['offset']))
 if matrices[0]==matrices[1]:raise ValueError('distinct eye matrices required')
 a.output.mkdir(parents=True,exist_ok=False);source=a.output/'original.spvasm'
 subprocess.run([str(TOOL/'spirv-dis.exe'),str(a.shader),'-o',str(source)],check=True);text=source.read_text()
 if 'BuiltIn ClipDistance' in text or text.count(f'OpStore %gl_Position %{spec["position"]}')!=1:raise ValueError('clip output boundary differs')
 text=text.replace('OpCapability Shader','OpCapability Shader\n               OpCapability ClipDistance',1)
 text,n=re.subn(r'(?m)^(\s*OpEntryPoint Vertex[^\n]+)$',r'\1 %evr_clip',text)
 if n!=1:raise ValueError('vertex entry differs')
 text=text.replace('               OpDecorate ', '               OpDecorate %evr_clip BuiltIn ClipDistance\n               OpDecorate ',1)
 declarations=['%evr_u4 = OpTypeVector %uint 4','%evr_b4 = OpTypeVector %bool 4','%evr_clip_arr = OpTypeArray %float %uint_1','%evr_clip_ptr = OpTypePointer Output %evr_clip_arr','%evr_clip = OpVariable %evr_clip_ptr Output',f'%evr_distance = OpConstant %float {a.distance:.9g}']
 declarations += [f'%evr_instance_{n} = OpConstant %uint {v}' for n,v in enumerate(ids)]
 for eye,matrix in enumerate(matrices):
  for n,v in enumerate(matrix):declarations += [f'%evr_m_{eye}_{n} = OpConstant %uint {v}']
  for row in range(4):declarations += [f'%evr_row_{eye}_{row} = OpConstantComposite %evr_u4 '+' '.join(f'%evr_m_{eye}_{row*4+j}' for j in range(4))]
 text,n=re.subn(r'(?m)^(\s*%2 = OpFunction[^\n]+)$','\n'.join(declarations)+'\n'+r'\1',text)
 if n!=1:raise ValueError('function boundary differs')
 body=[]
 for row,native in enumerate(spec['rows']):body += [f'%evr_bits_{row} = OpBitcast %evr_u4 %{native}']
 for eye in range(2):
  for row in range(4):body += [f'%evr_same_{eye}_{row} = OpIEqual %evr_b4 %evr_bits_{row} %evr_row_{eye}_{row}',f'%evr_all_{eye}_{row} = OpAll %bool %evr_same_{eye}_{row}']
  body += [f'%evr_ab_{eye} = OpLogicalAnd %bool %evr_all_{eye}_0 %evr_all_{eye}_1',f'%evr_cd_{eye} = OpLogicalAnd %bool %evr_all_{eye}_2 %evr_all_{eye}_3',f'%evr_eye_{eye} = OpLogicalAnd %bool %evr_ab_{eye} %evr_cd_{eye}']
 body += ['%evr_scope = OpLogicalOr %bool %evr_eye_0 %evr_eye_1']
 selected='%evr_hit_0'
 for n in range(len(ids)):
  body += [f'%evr_hit_{n} = OpIEqual %bool %{spec["instance"]} %evr_instance_{n}']
  if n:body += [f'%evr_union_{n} = OpLogicalOr %bool {selected} %evr_hit_{n}'];selected=f'%evr_union_{n}'
 body += [f'%evr_selected = OpLogicalAnd %bool %evr_scope {selected}',f'%evr_w = OpCompositeExtract %float %{spec["position"]} 3','%evr_plane = OpFSub %float %evr_w %evr_distance','%evr_value = OpSelect %float %evr_selected %evr_plane %float_1','%evr_output = OpCompositeConstruct %evr_clip_arr %evr_value','OpStore %evr_clip %evr_output']
 needle=f'               OpStore %gl_Position %{spec["position"]}';text=text.replace(needle,'\n'.join(body)+'\n'+needle)
 assembly=a.output/'experiment.spvasm';binary=a.output/'experiment.spv';assembly.write_text(text)
 subprocess.run([str(TOOL/'spirv-as.exe'),'--target-env','spv1.5',str(assembly),'-o',str(binary)],check=True)
 subprocess.run(['references/vulkan-sdk/tools/Bin/spirv-val.exe','--target-env','vulkan1.2','--scalar-block-layout',str(binary)],check=True)
 recipe=dict(offline_only=True,mode=a.mode,original_sha256=spec['sha'],patched_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),parts=parts,indices=ids,distance=a.distance,eye_matrix_bits=matrices,clip_feature_verified=True,existing_outputs_unchanged=True,limitation=__doc__)
 (a.output/'recipe.json').write_text(json.dumps(recipe,indent=2));print('PASS clip experiment assembled/validated',a.mode,ids,a.distance)
if __name__=='__main__':main()
