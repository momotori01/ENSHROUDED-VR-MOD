#version 450
layout(set=0,binding=0) uniform sampler2D sourceUi;
// The mod's own picture (the VR bug report's button and notes, hooks/bug_report.hpp): display values, straight alpha,
// laid over `overlay` (x0, y0, x1, y1 in pixels; none while x1 <= x0) after the fade and the cursor's dot is not over it.
layout(set=0,binding=1) uniform sampler2D overlayPicture;
// HUD auto-hide (include/evr/hud_fade.hpp): the two `keep` rectangles (x0, y0, x1, y1 in pixels) are always fully
// shown - what the game pins to things in the world lands there - and everything else is scaled by `outside`
// (1 = as the game drew it).
// The menu pointer (hooks/menu_pointer.hpp): `cursor` is x, y in pixels, the radius in pixels (0 = none) and the tone
// (1 light, 2 the green of a held button) of the dot that shows where the hand's ray meets the image.
// The aim mark (hooks/aim_mark.hpp): while it is drawn into the eyes' images, `hole` (x, y, radius in pixels; radius 0 =
// none) takes the game's own reticle - the dot in the middle of the HUD - out of this image.
// The desktop's picture (include/evr/ui_transfer.hpp): `display` above 0.5 leaves the values as the game stored them
// - display values, premultiplied - for a target that holds display values and blends them as the game itself does.
layout(push_constant) uniform Fade { vec4 keep; vec4 keepWide; vec4 cursor; vec4 hole; float outside; float display; vec4 overlay; } fade;
bool inside(vec4 r) { return all(greaterThanEqual(gl_FragCoord.xy,r.xy)) && all(lessThan(gl_FragCoord.xy,r.zw)); }
layout(location=0) out vec4 color;
vec3 srgbDecode(vec3 x) {
    return mix(pow((x + 0.055) / 1.055, vec3(2.4)), x / 12.92, lessThanEqual(x,vec3(0.04045)));
}
void main() {
    // Native UI stores premultiplied display values in UNORM. Convert coverage separately.
    // A zero-alpha additive contribution is preserved instead of being divided by zero.
    // Flattened gamma-space overlap cannot be inverted exactly; see docs/ui_rendering.md.
    vec4 p = texelFetch(sourceUi,ivec2(gl_FragCoord.xy),0);
    vec3 linear = fade.display > 0.5 ? p.rgb : (p.a > 0.0 ? srgbDecode(p.rgb / p.a) * p.a : srgbDecode(p.rgb));
    color = vec4(linear,p.a) * (inside(fade.keep) || inside(fade.keepWide) ? 1.0 : fade.outside);
    if (fade.hole.z > 0.0) color *= smoothstep(fade.hole.z, fade.hole.z + 2.0, distance(gl_FragCoord.xy, fade.hole.xy));
    if (fade.overlay.z > fade.overlay.x && inside(fade.overlay)) {
        vec4 o = texture(overlayPicture, (gl_FragCoord.xy - fade.overlay.xy) / (fade.overlay.zw - fade.overlay.xy));
        vec3 c = fade.display > 0.5 ? o.rgb : srgbDecode(o.rgb);
        color = vec4(c * o.a, o.a) + color * (1.0 - o.a);
    }
    if (fade.cursor.z > 0.0) {
        // A dark rim under a light dot, both premultiplied like the image.
        float d = distance(gl_FragCoord.xy, fade.cursor.xy);
        float rim = (1.0 - smoothstep(fade.cursor.z + 1.5, fade.cursor.z + 3.0, d)) * 0.85;
        color = vec4(0.0, 0.0, 0.0, rim) + color * (1.0 - rim);
        float dotAlpha = 1.0 - smoothstep(fade.cursor.z - 1.0, fade.cursor.z + 0.5, d);
        vec3 tone = fade.cursor.w > 1.5 ? vec3(0.2, 1.0, 0.3) : vec3(1.0);
        color = vec4(tone * dotAlpha, dotAlpha) + color * (1.0 - dotAlpha);
    }
}
