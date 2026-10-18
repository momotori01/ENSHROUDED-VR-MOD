"""Prepare a hash-locked local GI probe experiment; game shader remains under output."""
import argparse, hashlib, json, re, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SPIRV = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/plugins/spirv'
SOURCE_SHA = '5d79570aa82b8a4247a11985893317fe908d1105e32a63244880e984957fd01c'


def once(source, old, new):
    if source.count(old) != 1:
        raise ValueError('Unexpected reviewed shader shape: ' + old[:70])
    return source.replace(old, new, 1)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('source', type=Path)
    p.add_argument('output', type=Path)
    p.add_argument('--control', action='store_true')
    a = p.parse_args()
    original = a.source.read_bytes()
    if hashlib.sha256(original).hexdigest() != SOURCE_SHA:
        raise ValueError('Not the reviewed native GI sampling shader')
    a.output.mkdir(parents=True, exist_ok=False)
    source = subprocess.check_output([str(SPIRV / 'spirv-cross.exe'), str(a.source), '--version', '460',
                                     '--vulkan-semantics', '--glsl-emit-ubo-as-plain-uniforms'], text=True)
    # Native uvec2 begins at byte 4. Two scalar uints preserve its 16-byte struct stride.
    source = once(source, '    uvec2 _m1;', '    uint evrTileX;\n    uint evrTileY;')
    pattern = r'_5\._m1\[([^]]+)\]\._m1'
    source, count = re.subn(pattern, lambda m: 'uvec2(_5._m1['+m[1]+'].evrTileX,_5._m1['+m[1]+'].evrTileY)', source)
    if count != 4:
        raise ValueError('Native atlas tile references changed')
    source = once(source, 'layout(set = 1) uniform _57 _5;',
                  'layout(set = 1, binding = 0, std140) uniform EvrGiConstants { _57 parameters; } evrGi;')
    source = source.replace('_5.', 'evrGi.parameters.')
    if not a.control:
        source = once(source, '        float _724;\n        uint _727;',
            '        uint evrRejectedIds[8];\n        float evrRejectedWeights[8];\n'
            '        uint evrRejectedCount=0u;\n        float evrRejectedMax=0.0;\n'
            '        float _724;\n        uint _727;')
        source = once(source, '            if (_858)\n            {',
            '            if (_858)\n            {\n'
            '                if (_163 > 0.0 && !isnan(_163) && !isinf(_163)) {\n'
            '                    evrRejectedIds[evrRejectedCount]=_753;\n'
            '                    evrRejectedWeights[evrRejectedCount++]=_163;\n'
            '                    evrRejectedMax=max(evrRejectedMax,_163);\n'
            '                }')
        marker = '        _26[gl_LocalInvocationIndex]._m1 = _727 | (packHalf2x16(vec2(_724, 0.0)) << 16u);'
        helper = (ROOT / 'vrmod/shaders/gi_probe_fallback.glsl').read_text(encoding='utf-8')
        source = once(source, marker, marker+'\n'+helper)
    comp = a.output / 'gi_probe.comp'
    spv = a.output / 'gi_probe.spv'
    comp.write_text(source, encoding='utf-8')
    subprocess.run([str(SPIRV / 'glslangValidator.exe'), '-V', '--target-env', 'vulkan1.2', '-S', 'comp',
                    '--source-entrypoint', 'main', '-e', 'cs_main', str(comp), '-o', str(spv)], check=True)
    subprocess.run([str(SPIRV / 'spirv-cross.exe'), str(spv), '--reflect', '--output', str(a.output/'reflect.json')], check=True)
    reflection = json.loads((a.output/'reflect.json').read_text(encoding='utf-8'))
    if [(u['set'],u['binding'],u['block_size']) for u in reflection['ubos']] != [(1,0,768)]:
        raise ValueError('GI uniform descriptor/extent changed')
    report = dict(source_sha256=SOURCE_SHA, shader_sha256=hashlib.sha256(spv.read_bytes()).hexdigest(),
                  control=a.control, history_reset=False, fallback_only=True, shader=str(spv.resolve()))
    (a.output/'manifest.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(report))


if __name__ == '__main__':
    main()
