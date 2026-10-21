#version 450
// The game's depth of an eye (the image it hands DLSS; sampled, never copied: it has no transfer-source usage) into
// the eye warp's own R32 image, pixel for pixel (eye_warp.hpp keep, with ui_transfer.vert's full-screen triangle).
// shift (2026-10-21, EVR_EYE_WARP_UNJITTER): the game draws the depth with DLSS's jitter - each frame the whole
// picture moves by a part of a pixel - while the colour kept beside it is DLSS's output, which has no jitter. Read at
// the place shifted by the jitter, the kept depth stands where the colour stands (between texels: bilinear, the
// reversed depth is linear over a plane; across a step between the four texels the nearest one).
layout(set = 0, binding = 0) uniform sampler2D depthTex;
layout(push_constant) uniform Shift { vec2 by; } shift;
layout(location = 0) out float outDepth;
void main() {
    if (shift.by == vec2(0.0)) { outDepth = texelFetch(depthTex, ivec2(gl_FragCoord.xy), 0).r; return; }
    ivec2 size = textureSize(depthTex, 0);
    vec2 at = gl_FragCoord.xy + shift.by - 0.5;   // texel centres at whole numbers
    ivec2 i0 = ivec2(floor(at));vec2 f = at - vec2(i0);
    ivec2 lo = ivec2(0), hi = size - 1;
    float d00 = texelFetch(depthTex, clamp(i0, lo, hi), 0).r, d10 = texelFetch(depthTex, clamp(i0 + ivec2(1, 0), lo, hi), 0).r;
    float d01 = texelFetch(depthTex, clamp(i0 + ivec2(0, 1), lo, hi), 0).r, d11 = texelFetch(depthTex, clamp(i0 + ivec2(1, 1), lo, hi), 0).r;
    float dMin = min(min(d00, d10), min(d01, d11)), dMax = max(max(d00, d10), max(d01, d11));
    // reversed depth = near / distance: a step of a tenth of the distance between the texels is an edge
    if (dMin <= 0.0 || dMax > dMin * 1.1) { outDepth = texelFetch(depthTex, clamp(ivec2(floor(at + 0.5)), lo, hi), 0).r; return; }
    outDepth = mix(mix(d00, d10, f.x), mix(d01, d11, f.x), f.y);
}
