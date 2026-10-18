#version 450
// VR status tint (2026-10-06, user: "vignettes are unpleasant in VR; a slightly red or blue rim is enough").
// Outer rim of one eye only: the temporal side, top and bottom. The nasal side stays clear because each eye's
// nasal edge lies inside the other eye's clear view and would be seen by one eye only.
layout(push_constant) uniform Tint {
    vec4 color; // rgb = per-channel multiplier at full strength, a = strength at the outermost pixel
    vec4 shape; // x: -1 left eye (tint the left edge), +1 right eye; y: band width as a fraction of the height; zw: 1/extent
} pc;
layout(location=0) out vec4 outColor;
void main() {
    vec2 px = gl_FragCoord.xy;
    vec2 extent = 1.0 / pc.shape.zw;
    float side = pc.shape.x < 0.0 ? px.x : extent.x - px.x;
    float edge = min(side, min(px.y, extent.y - px.y)) / extent.y;
    float t = 1.0 - smoothstep(0.0, pc.shape.y, edge);
    // Multiplicative tint (blend: dst * src colour): shifts the rim towards red/blue without brightening it, so a
    // dark scene never gets a glowing frame (game134957: an additive lavender rim read as a frame at night).
    outColor = vec4(mix(vec3(1.0), pc.color.rgb, pc.color.a * t * t), 1.0);
}
