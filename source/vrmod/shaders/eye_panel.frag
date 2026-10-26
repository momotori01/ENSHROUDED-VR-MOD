#version 450
// The mod's own picture beside the aim mark (include/evr/eye_panel.hpp; 2026-10-26, the staff's spells). The picture lies on a
// plane in the room (a quad facing the head at the cluster's distance, layer/native_xr.hpp): h maps this eye's pixel to the
// picture's place on it (a homography - the inverse of the plane's projection into this eye), so both eyes see the same plane
// whatever their views' turn (the Index's views are turned 6 degrees outwards: a picture shifted by the same pixels in both
// eyes had a slanted, partly beyond-infinity depth - user 2026-10-26: "the focal distance is bad, very hard to see").
// Straight alpha over the eye's colour (the eye image's alpha channel is left as it is), faded by the strength.
layout(push_constant) uniform Panel {
    vec4 h0, h1, h2;  // the rows of h (xyz)
    vec4 more;        // x: strength 0..1
} pc;
layout(set = 0, binding = 0) uniform sampler2D picture;
layout(location = 0) out vec4 outColor;
void main() {
    vec3 p = vec3(gl_FragCoord.xy, 1.0);
    vec3 q = vec3(dot(pc.h0.xyz, p), dot(pc.h1.xyz, p), dot(pc.h2.xyz, p));
    if (q.z <= 0.0) discard;
    vec2 uv = q.xy / q.z;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) discard;
    vec4 c = texture(picture, uv);
    outColor = vec4(c.rgb, c.a * pc.more.x);
}
