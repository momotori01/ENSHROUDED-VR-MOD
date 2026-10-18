"""Build OFFLINE native vertex prototypes reading an owned immutable BDA payload.
No running game injection or device allocations. Native zero padding stays inactive.
"""
import argparse,hashlib,json,pathlib,re,subprocess
from build_local_body_clip_replay import SPECS,TOOL

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('shader',type=pathlib.Path);p.add_argument('output',type=pathlib.Path);p.add_argument('--mode',choices=SPECS,required=True);a=p.parse_args();spec=SPECS[a.mode]
 raw=a.shader.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=spec['sha']:raise ValueError('unreviewed vertex module')
 a.output.mkdir(parents=True,exist_ok=False);original=a.output/'original.spvasm'
 subprocess.run([str(TOOL/'spirv-dis.exe'),str(a.shader),'-o',str(original)],check=True);s=original.read_text()
 s=s.replace('OpCapability Shader','OpCapability Shader\nOpCapability ClipDistance\nOpCapability Int64\nOpCapability PhysicalStorageBufferAddresses',1)
 s=s.replace('OpMemoryModel Logical GLSL450','OpMemoryModel PhysicalStorageBuffer64 GLSL450',1)
 s,n=re.subn(r'(?m)^(\s*OpEntryPoint Vertex[^\n]+)$',r'\1 %evr_clip',s)
 if n!=1:raise ValueError('vertex interface')
 ann=['OpDecorate %evr_clip BuiltIn ClipDistance','OpDecorate %evr_words ArrayStride 4','OpMemberDecorate %evr_payload 0 Offset 0','OpDecorate %evr_payload Block']
 if a.mode=='color':
  ann.append('OpMemberDecorate %_struct_81 32 Offset 632')
  s,n=re.subn(r'(?m)^(\s*%_struct_81 = OpTypeStruct[^\n]+)$',r'\1 %v2uint',s)
  if n!=1:raise ValueError('color constant structure')
 s=s.replace('               OpDecorate ', '\n'.join(ann)+'\n               OpDecorate ',1)
 decl=['%evr_u64 = OpTypeInt 64 0','%evr_count = OpConstant %uint 28','%evr_limit = OpConstant %uint 24','%evr_tag = OpConstant %uint 1163280963','%evr_min = OpConstant %float 0.01', # 0x45564243
 '%evr_words = OpTypeArray %uint %evr_count','%evr_payload = OpTypeStruct %evr_words','%evr_p = OpTypePointer PhysicalStorageBuffer %evr_payload','%evr_word_ptr = OpTypePointer PhysicalStorageBuffer %uint','%evr_up2 = OpTypePointer Uniform %v2uint','%evr_clip_arr = OpTypeArray %float %uint_1','%evr_clip_ptr = OpTypePointer Output %evr_clip_arr','%evr_clip = OpVariable %evr_clip_ptr Output']
 decl += [f'%evr_i{n} = OpConstant %uint {n}' for n in range(33)]
 s,n=re.subn(r'(?m)^(\s*%2 = OpFunction[^\n]+)$','\n'.join(decl)+'\n'+r'\1',s)
 if n!=1:raise ValueError('main boundary')
 body=[]
 if a.mode=='color':body+=['%evr_pair_ptr = OpAccessChain %evr_up2 %24 %evr_i32','%evr_pair = OpLoad %v2uint %evr_pair_ptr','%evr_lo = OpCompositeExtract %uint %evr_pair 0','%evr_hi = OpCompositeExtract %uint %evr_pair 1']
 else:body+=['%evr_lo_ptr = OpAccessChain %_ptr_Uniform_uint %15 %evr_i22','%evr_hi_ptr = OpAccessChain %_ptr_Uniform_uint %15 %evr_i23','%evr_lo = OpLoad %uint %evr_lo_ptr','%evr_hi = OpLoad %uint %evr_hi_ptr','%evr_pair = OpCompositeConstruct %v2uint %evr_lo %evr_hi']
 body+=['%evr_lo_present = OpINotEqual %bool %evr_lo %uint_0','%evr_hi_present = OpINotEqual %bool %evr_hi %uint_0','%evr_present = OpLogicalOr %bool %evr_lo_present %evr_hi_present','OpSelectionMerge %evr_end None','OpBranchConditional %evr_present %evr_load %evr_end','%evr_load = OpLabel','%evr_address = OpBitcast %evr_u64 %evr_pair','%evr_ptr = OpConvertUToPtr %evr_p %evr_address']
 for n in range(28):body += [f'%evr_ptr{n} = OpAccessChain %evr_word_ptr %evr_ptr %evr_i0 %evr_i{n}',f'%evr_word{n} = OpLoad %uint %evr_ptr{n} Aligned 4']
 body+=['%evr_tag_ok = OpIEqual %bool %evr_word0 %evr_tag','%evr_count_min = OpUGreaterThan %bool %evr_word1 %uint_0','%evr_count_max = OpULessThanEqual %bool %evr_word1 %evr_limit','%evr_count_ok = OpLogicalAnd %bool %evr_count_min %evr_count_max','%evr_header_ok = OpLogicalAnd %bool %evr_tag_ok %evr_count_ok','%evr_distance = OpBitcast %float %evr_word2','%evr_distance_min = OpFOrdGreaterThanEqual %bool %evr_distance %evr_min','%evr_distance_max = OpFOrdLessThanEqual %bool %evr_distance %float_1','%evr_distance_ok = OpLogicalAnd %bool %evr_distance_min %evr_distance_max','%evr_enabled = OpLogicalAnd %bool %evr_header_ok %evr_distance_ok']
 selected=''
 for n in range(24):
  body += [f'%evr_hit{n} = OpIEqual %bool %{spec["instance"]} %evr_word{n+4}',f'%evr_active{n} = OpUGreaterThan %bool %evr_word1 %evr_i{n}',f'%evr_selected{n} = OpLogicalAnd %bool %evr_hit{n} %evr_active{n}']
  if n:body += [f'%evr_union{n} = OpLogicalOr %bool {selected} %evr_selected{n}'];selected=f'%evr_union{n}'
  else:selected='%evr_selected0'
 body += [f'%evr_selected = OpLogicalAnd %bool %evr_enabled {selected}',f'%evr_w = OpCompositeExtract %float %{spec["position"]} 3','%evr_plane = OpFSub %float %evr_w %evr_distance','%evr_value = OpSelect %float %evr_selected %evr_plane %float_1','OpBranch %evr_end','%evr_end = OpLabel',f'%evr_result = OpPhi %float %float_1 %{1177 if a.mode=="depth" else 2057} %evr_value %evr_load','%evr_output = OpCompositeConstruct %evr_clip_arr %evr_result','OpStore %evr_clip %evr_output']
 needle=f'               OpStore %gl_Position %{spec["position"]}'
 if s.count(needle)!=1:raise ValueError('Position output differs')
 s=s.replace(needle,'\n'.join(body)+'\n'+needle)
 assembly=a.output/'experiment.spvasm';binary=a.output/'experiment.spv';assembly.write_text(s)
 subprocess.run([str(TOOL/'spirv-as.exe'),'--target-env','spv1.5',str(assembly),'-o',str(binary)],check=True)
 subprocess.run(['references/vulkan-sdk/tools/Bin/spirv-val.exe','--target-env','vulkan1.2','--scalar-block-layout',str(binary)],check=True)
 (a.output/'recipe.json').write_text(json.dumps(dict(mode=a.mode,original_sha256=spec['sha'],patched_sha256=hashlib.sha256(binary.read_bytes()).hexdigest(),payload_bytes=112,marker=0x45564243,uniform_offset=472 if a.mode=='depth' else 632,scope=__doc__),indent=2))
 print('PASS address prototype',a.mode)
if __name__=='__main__':main()
