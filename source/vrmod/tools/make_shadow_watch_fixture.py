#!/usr/bin/env python3
"""Mock scripts (hands and head) for watching sunlit shadows without anybody at the keyboard, and variants of the sun
shadow shader for telling why its edges flicker.

User, 2026-10-13: "shadows that flicker fast, mostly at their edges, on the grass and on the hands - can that be fixed?"
The save starts in rain; the sun is out some four minutes later. So: walk out of the house as the scenery survey does
(frames 600..837 of home-decal-outdoor-20261012.csv), then stand still and look at the ground under the trees until
frame 20000 (the head: --yaw, --pitch in degrees, pitch below zero looks down). A burst of consecutive pictures late in
the run (tools: burst_at.py of the session, or xr-burst.request) then has still ground under a moving shadow.

--variants <directory>: also writes three variants of the replaced sun shadow shader, from the patched source of
output/shadow-receiver-live-20261009 (the game's shader with the receiver-plane helpers; its history handling is the
game's), each with a manifest for -ShadowReceiverPlane:
  accept  the history is taken wherever the reprojected place is on screen (the depth comparison is left out)
  wide    the range the history is clipped to (from the spread of the neighbours) is four times as wide
  both    both
  split   the three side by side in one picture: left third as it is, middle third accept, right third wide
Diagnostics for one question - which of the two throws the history away - not candidates for play as they are.
"""
import argparse, hashlib, json, pathlib, subprocess

ROOT = pathlib.Path(__file__).resolve().parents[2]
LEFT_STICK_X, LEFT_STICK_Y = 29, 30
SPIRV = ROOT / 'references/renderdoc/1.46/RenderDoc_1.46_64/plugins/spirv'
LIVE = ROOT / 'vrmod/output/shadow-receiver-live-20261009'
DEPTH_TEST = 'if (_776 > 0.89999997615814208984375)'
CLIP = 'precise vec2 _72 = _228 * evrUniform.parameters._m6;'
MAIN = 'void main()\n{'
SLOPE = 'vec2 evrSlope(uint cascade) {\n#if EVR_PLANE'
SPLIT = ('\n    float evrPlace = float(gl_GlobalInvocationID.x) / float(evrUniform.parameters._m2.x);'
         '\n    bool evrAccept = evrPlace > 0.3333 && evrPlace < 0.6667;'
         '\n    float evrWide = evrPlace >= 0.6667 ? 4.0 : 1.0;')


def scripts(a):
    walk = [line.split(',') for line in a.walk.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    head = [line.split(',') for line in a.head.read_text(encoding='utf-8').splitlines() if line and not line.startswith('#')]
    if any(len(r) != 39 for r in walk) or walk[757][LEFT_STICK_Y] != '1.0' or head[0][4] != '-90':
        raise SystemExit('unexpected source scripts')
    rest = walk[0]; out = []
    for frame in range(1, a.frames + 1):
        r = list(rest); r[0] = str(frame)
        if frame < 900: r[LEFT_STICK_X:LEFT_STICK_Y + 1] = walk[frame - 1][LEFT_STICK_X:LEFT_STICK_Y + 1]
        out.append(','.join(r))
    a.output.write_text('\n'.join(['# Watching sunlit shadows: walk 600..837, then still until %d (tools/make_shadow_watch_fixture.py).' % a.frames] + out) + '\n', encoding='utf-8', newline='\n')
    level = head[0]; rows = []
    for frame in range(1, a.frames + 1):
        r = list(level); r[0] = str(frame)
        if frame >= 900: r[4] = '%g' % a.yaw; r[5] = '%g' % a.pitch
        rows.append(','.join(r))
    target = a.output.with_name(a.output.stem + '-head.csv')
    target.write_text('\n'.join(['# Head script for watching shadows (tools/make_shadow_watch_fixture.py): yaw %g, pitch %g from frame 900.' % (a.yaw, a.pitch)] + rows) + '\n', encoding='utf-8', newline='\n')
    print(a.output, target)


def variants(directory):
    source = (LIVE / 'shadow_receiver.comp').read_text(encoding='utf-8'); manifest = json.loads((LIVE / 'manifest.json').read_text(encoding='utf-8'))
    if source.count(DEPTH_TEST) != 1 or source.count(CLIP) != 1: raise SystemExit('unexpected shader source shape')
    for name, accept, wide in (('accept', True, False), ('wide', False, True), ('both', True, True), ('split', None, None), ('plane', None, None), ('widths', None, None)):
        text = source
        if name == 'split':
            # One picture, three treatments side by side: the left third as the game does it, the middle third takes
            # the history whatever the depth says, the right third clips it four times as wide. Whatever the scene and
            # the time of day, the same frame then shows which of the two throws the history away.
            if text.count(MAIN) != 1: raise SystemExit('unexpected shader source shape')
            text = text.replace(MAIN, MAIN + SPLIT)
            text = text.replace(DEPTH_TEST, 'if (_776 > 0.89999997615814208984375 || evrAccept)')
            text = text.replace(CLIP, 'precise vec2 _72 = _228 * (evrUniform.parameters._m6 * evrWide);')
        if name == 'widths':
            # One picture, three widths of the range the history is clipped to: the game's, twice, four times.
            if text.count(MAIN) != 1: raise SystemExit('unexpected shader source shape')
            text = text.replace(MAIN, MAIN + '\n    float evrPlace = float(gl_GlobalInvocationID.x) / float(evrUniform.parameters._m2.x);\n    float evrWide = evrPlace < 0.3333 ? 1.0 : (evrPlace < 0.6667 ? 2.0 : 4.0);')
            text = text.replace(CLIP, 'precise vec2 _72 = _228 * (evrUniform.parameters._m6 * evrWide);')
        if name == 'plane':
            # One picture, two halves: the left half without the receiver-plane correction (the game's own sampling,
            # as the control variant of prepare_shadow_receiver_shader.py), the right half with it, as play has it.
            # For one question: are the dots on sunlit leaves the correction's doing?
            if text.count(MAIN) != 1 or text.count(SLOPE) != 1: raise SystemExit('unexpected shader source shape')
            text = text.replace(MAIN, MAIN + '\n    evrPlaneOff = float(gl_GlobalInvocationID.x) / float(evrUniform.parameters._m2.x) < 0.5;')
            text = text.replace(SLOPE, 'bool evrPlaneOff;\n' + SLOPE + '\n    if(evrPlaneOff) return vec2(0.0);')
        if accept: text = text.replace(DEPTH_TEST, 'if (true)')
        if wide: text = text.replace(CLIP, 'precise vec2 _72 = _228 * (evrUniform.parameters._m6 * 4.0);')
        out = directory / name; out.mkdir(parents=True, exist_ok=True); comp = out / 'shadow_receiver.comp'; spv = out / 'shadow_receiver.spv'
        comp.write_text(text, encoding='utf-8')
        subprocess.run([str(SPIRV / 'glslangValidator.exe'), '-V', '--target-env', 'vulkan1.2', '-S', 'comp', '--source-entrypoint', 'main', '-e', 'cs_main', str(comp), '-o', str(spv)], check=True, stdout=subprocess.DEVNULL)
        report = dict(manifest); report.update(shader_sha256=hashlib.sha256(spv.read_bytes()).hexdigest(), shader=str(spv.resolve()), diagnostic_variant=name)
        (out / 'manifest.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8'); print(out, report['shader_sha256'][:16])


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--walk', type=pathlib.Path, default=ROOT / 'vrmod/test_env/home-decal-outdoor-20261012.csv')
    p.add_argument('--head', type=pathlib.Path, default=ROOT / 'vrmod/test_env/home-decal-outdoor-20261012-head.csv')
    p.add_argument('--output', type=pathlib.Path, default=ROOT / 'vrmod/test_env/home-shadow-watch-20261013.csv')
    p.add_argument('--frames', type=int, default=20000); p.add_argument('--yaw', type=float, default=180.); p.add_argument('--pitch', type=float, default=-25.)
    p.add_argument('--variants', type=pathlib.Path)
    a = p.parse_args()
    scripts(a)
    if a.variants: variants(a.variants)


if __name__ == '__main__':
    main()
