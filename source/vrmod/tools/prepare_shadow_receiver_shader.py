"""Prepare a local hash-locked shadow experiment from the user's captured shader.
The game shader remains under output; only receiver-plane helpers are distributed.
No download, process access, game asset modification or persistent setting.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess

SOURCE_SHA = 'e0d6de57a4db980fcdcfD7ba61640f0db21be193ef57ffde91b43dd6efca2427'.lower()
# 1.0 (2026-10-16, captured in game-20261016-061640 as shader_5df36f1ee4186499.spv): the same algorithm with other names,
# an early exit for the sky, the normal no longer in its own variable, and the shadow constants reordered (the helpers'
# 1.0 copy). Each profile: the helper file, the line the receiver normal goes before (and with it), the six taps.
PROFILES = {
    SOURCE_SHA: dict(helpers='vrmod/shaders/shadow_receiver_helpers.glsl',
                     normal=('vec3 _417 = normalize(_413);', 'vec3 _417 = normalize(_413);\n        evrReceiverNormal=evrNormal(_75,_417);'),
                     gather=[('_158', '_142', '_513'), ('_191', '_175', '_103'), ('_125', '_109', '_513')],
                     pcf=[('_170', '_142', '_513'), ('_203', '_175', '_103'), ('_137', '_109', '_513')]),
    'bb450d00434dca964a841dcc9995582282ce69bf49260e6d5e153ba87483177b': dict(
                     helpers='vrmod/shaders/shadow_receiver_helpers_1_0.glsl',
                     normal=('precise float _55 = dot(normalize(_412),', 'evrReceiverNormal=evrNormal(_74,normalize(_412));\n        precise float _55 = dot(normalize(_412),'),
                     gather=[('_157', '_141', '_460'), ('_190', '_174', '_102'), ('_124', '_108', '_460')],
                     pcf=[('_169', '_141', '_460'), ('_202', '_174', '_102'), ('_136', '_108', '_460')]),
}
ROOT = Path(__file__).resolve().parents[2]
SPIRV = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/plugins/spirv'

def replace_once(source, old, new):
    if source.count(old) != 1:
        raise ValueError(f'Unexpected shader source shape: {old[:80]}')
    return source.replace(old, new, 1)

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--reset-history', action='store_true', help='Offline diagnostic only')
    parser.add_argument('--control', action='store_true', help='Recompile without receiver-plane changes')
    args = parser.parse_args()
    code = args.source.read_bytes()
    source_sha = hashlib.sha256(code).hexdigest()
    if source_sha not in PROFILES:
        raise ValueError('Source shader is not a reviewed game shader')
    profile = PROFILES[source_sha]
    args.output.mkdir(parents=True, exist_ok=True)
    source = subprocess.check_output([str(SPIRV / 'spirv-cross.exe'), str(args.source), '--version', '460', '--vulkan-semantics', '--glsl-emit-ubo-as-plain-uniforms'], text=True)
    # Preserve native offsets with std140; no scalarBlockLayout feature needed.
    source = replace_once(source, '    vec3 _m8;', '    float evrLightX;\n    float evrLightY;\n    float evrLightZ;')
    source = source.replace('_4._m8', 'vec3(evrUniform.parameters.evrLightX,evrUniform.parameters.evrLightY,evrUniform.parameters.evrLightZ)')
    source = replace_once(source, 'layout(set = 0) uniform _34 _4;',
                          'layout(set = 0, binding = 0, std140) uniform EvrShadowConstants { _34 parameters; } evrUniform;')
    source = source.replace('_4.', 'evrUniform.parameters.')
    if args.reset_history:
        source = replace_once(source, 'if (evrUniform.parameters._m9 != 0u)', 'if (true)')
    if not args.control:
        helper = (ROOT / profile['helpers']).read_text(encoding='utf-8')
        source = replace_once(source, '#version 460', '#version 460\n#define EVR_PLANE 1')
        source = replace_once(source, '\nvoid main()', '\n' + helper + '\nvoid main()')
        source = replace_once(source, *profile['normal'])
        for tap, center, cascade in profile['gather']:
            source = replace_once(source, f'textureGatherOffset(sampler2DArray(_13, _15), vec3({tap}, float({cascade})), ivec2(0))', f'evrGather({tap},{center},{cascade})')
        for tap, center, cascade in profile['pcf']:
            source = replace_once(source, f'textureGrad(sampler2DArrayShadow(_13, _14), vec4(vec3({tap}, float({cascade})), {center}.z), vec2(0.0), vec2(0.0))', f'evrPcf({tap},{center},{cascade})')
    comp = args.output / 'shadow_receiver.comp'
    spv = args.output / 'shadow_receiver.spv'
    comp.write_text(source, encoding='utf-8')
    subprocess.run([str(SPIRV / 'glslangValidator.exe'), '-V', '--target-env', 'vulkan1.2', '-S', 'comp', '--source-entrypoint', 'main', '-e', 'cs_main', str(comp), '-o', str(spv)], check=True)
    subprocess.run([str(SPIRV / 'spirv-dis.exe'), str(spv), '-o', str(args.output / 'shadow_receiver.spvasm')], check=True)
    report = {'source_sha256': source_sha, 'shader_sha256': hashlib.sha256(spv.read_bytes()).hexdigest(),
              'history_reset': args.reset_history, 'control': args.control, 'shader': str(spv.resolve())}
    (args.output / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report))

if __name__ == '__main__':
    main()
