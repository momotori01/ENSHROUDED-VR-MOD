#version 450
// The aim mark (include/evr/aim_mark.hpp, hooks/aim_mark.hpp): a light dot with a dark rim, drawn into each eye's image
// where the aimed point - 128 m along the bow's, staff's or wand's line - shows in that eye. Both eyes get it at
// that depth, so it lies on what is aimed at and not on the HUD plate 1.5 m ahead.
// Blend: straight alpha over the eye's colour; the eye image's alpha channel is left as it is.
// The throw's guide (include/evr/live_throw.hpp; user 2026-10-18 12:40: "a short laser rather than a dotted line"):
// to.z 1 - a beam from place.xy to to.xy, a white glow round a light core, fainter towards its far end (red until 13:20,
// the user: "the throw's laser white").
// The stamina ring (to.z 2; 2026-10-26, layer/native_xr.hpp evrSetStamina - the mod's own, in place of the game's floating
// ring): a ring of radius place.z and width to.x round place.xy, filled clockwise from the top for the share to.y, the rest a
// faint dark track, a dark rim round it; teal, red while to.w is 1 (run out).
layout(push_constant) uniform Mark {
    vec4 place; // x, y in pixels of the eye image, radius in pixels, strength 0..1
    vec4 to;    // the beam's other end (x, y), z 1 for a beam, 0 for the dot; the ring: width, share, z 2, run out
} pc;
layout(location=0) out vec4 outColor;
void main() {
    if (pc.to.z > 1.5) {
        vec2 v = gl_FragCoord.xy - pc.place.xy;
        float d = abs(length(v) - pc.place.z), half_ = pc.to.x * 0.5;
        float band = 1.0 - smoothstep(half_ - 0.75, half_ + 0.75, d);
        float rim = 1.0 - smoothstep(half_ + 0.5, half_ + 2.0, d);
        float turn = fract(atan(v.x, -v.y) / 6.2831853 + 1.0);   // 0 at the top, growing clockwise (y runs down)
        bool filled = turn <= pc.to.y;
        vec3 fill = pc.to.w > 0.5 ? vec3(0.95, 0.32, 0.28) : vec3(0.42, 0.95, 0.78);
        vec3 colour = filled ? fill : vec3(0.08);
        float alpha = max(band * (filled ? 1.0 : 0.45), rim * 0.5);
        outColor = vec4(band > 0.0 ? colour : vec3(0.0), alpha * pc.place.w);
        return;
    }
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
