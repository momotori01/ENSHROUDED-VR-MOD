#version 450
// The aim mark (include/evr/aim_mark.hpp, hooks/aim_mark.hpp): a light dot with a dark rim, drawn into each eye's image
// where the aimed point - 128 m along the bow's, staff's or wand's line - shows in that eye. Both eyes get it at
// that depth, so it lies on what is aimed at and not on the HUD plate 1.5 m ahead.
// Blend: straight alpha over the eye's colour; the eye image's alpha channel is left as it is.
// The throw's guide (include/evr/live_throw.hpp; user 2026-10-18 12:40: "a short laser rather than a dotted line"):
// to.z 1 - a beam from place.xy to to.xy, a white glow round a light core, fainter towards its far end (red until 13:20,
// the user: "the throw's laser white").
layout(push_constant) uniform Mark {
    vec4 place; // x, y in pixels of the eye image, radius in pixels, strength 0..1
    vec4 to;    // the beam's other end (x, y), z 1 for a beam, 0 for the dot
} pc;
layout(location=0) out vec4 outColor;
void main() {
    if (pc.to.z > 0.5) {
        vec2 a = pc.place.xy, ab = pc.to.xy - pc.place.xy, p = gl_FragCoord.xy;
        float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 1e-4), 0.0, 1.0);
        float d = distance(p, a + ab * t), r = pc.place.z;
        float core = 1.0 - smoothstep(r * 0.30, r * 0.55, d);
        float glow = 1.0 - smoothstep(r * 0.35, r + 1.0, d);
        float fade = mix(1.0, 0.3, t);
        outColor = vec4(mix(vec3(0.82, 0.86, 0.9), vec3(1.0), core), max(glow * 0.6, core) * pc.place.w * fade);
        return;
    }
    float d = distance(gl_FragCoord.xy, pc.place.xy);
    float rim = (1.0 - smoothstep(pc.place.z + 1.0, pc.place.z + 2.5, d)) * 0.8;
    float core = 1.0 - smoothstep(pc.place.z - 1.0, pc.place.z + 0.5, d);
    // Inside the dot the colour is light, around it the picture is darkened, further out nothing changes.
    outColor = vec4(vec3(core), max(rim, core) * pc.place.w);
}
