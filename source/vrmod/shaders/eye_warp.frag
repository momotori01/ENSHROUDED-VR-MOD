#version 450
// The kept colour at the warped grid's place (eye_warp.vert). In a cell that stretches over a depth step the
// fragment finds its own point instead: its ray (the new eye's, taken into the kept eye's view space) is walked from
// the cell's nearest to its farthest distance, a pixel of the kept picture a step, to the first place where it goes
// behind the surface the picture has there; the place is refined by bisection and taken when the ray and the
// surface agree there (a crossing). Where they disagree the ray only went behind the edge of something nearer (the kept eye could
// not see the point): the walk goes on. A ray that meets nothing is left out (mode 1: a hole) or takes the
// stretched colour (mode 0).
// Mode 2, the hole fill (drawn after mode 1, only in its holes, from this eye's own last picture): the ray is walked
// the same way through that picture; of the points it crosses, the first the other eye cannot see now (something
// nearer stands before it in the other eye's picture, set 1) is taken - that is what a hole is: a place the other
// eye could not see. A crossing the other eye does see is something that moved since the last picture (the hand or
// the weapon a frame ago) and is skipped while there is a better one; with nothing better it is kept.
layout(set = 0, binding = 0) uniform sampler2D colorTex;
layout(set = 0, binding = 1) uniform sampler2D depthTex;   // NEAREST
layout(set = 0, binding = 2) uniform Extra {
    mat4 otherToNew;   // mode 2: the other eye's view space -> this frame's view space of the eye made
    vec4 otherTan;     // tan of the other eye's fov
} extra;
layout(set = 1, binding = 0) uniform sampler2D otherColor;   // mode 2: the other eye's picture of this frame
layout(set = 1, binding = 1) uniform sampler2D otherDepth;   // mode 2: the other eye's depth of this frame
layout(push_constant) uniform Warp {
    mat4 oldToNew;
    vec4 oldTan;
    vec4 newTan;
    vec4 params;     // y: near, z: the distance for depth 0, w: mode
} warp;
layout(location = 0) in vec2 uv;
layout(location = 1) in float dist;
layout(location = 2) flat in vec2 cellRange;
layout(location = 0) out vec4 outColor;
// The colour is read with a Catmull-Rom filter (2026-10-21; the alternate-eye warp's "everything shook": in a Mock turn
// the eye made by the warp was a tenth less sharp than the eye drawn - mean gradient 20.2 .. 21.1 against 22.2 .. 23.2 -
// so each eye went sharp, soft, sharp at half the frame rate; the bilinear read of the other eye's picture softens it).
// Nine bilinear reads (the weights of the 4 x 4 texels folded), never below 0, blended 4:1 with the bilinear read. sharpRead = 0 (EVR_EYE_WARP_SHARP=0): the
// bilinear read as before.
layout(constant_id = 0) const int sharpRead = 1;
vec3 colourAt(sampler2D tex, vec2 at) {
    if (sharpRead == 0) return texture(tex, at).rgb;
    vec2 size = vec2(textureSize(tex, 0)), p = at * size, t1 = floor(p - 0.5) + 0.5, f = p - t1;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f), w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2, t0 = (t1 - 1.0) / size, t3 = (t1 + 2.0) / size, t12 = (t1 + w2 / w12) / size;
    vec3 c = textureLod(tex, vec2(t0.x, t0.y), 0.0).rgb * (w0.x * w0.y) + textureLod(tex, vec2(t12.x, t0.y), 0.0).rgb * (w12.x * w0.y) + textureLod(tex, vec2(t3.x, t0.y), 0.0).rgb * (w3.x * w0.y)
           + textureLod(tex, vec2(t0.x, t12.y), 0.0).rgb * (w0.x * w12.y) + textureLod(tex, vec2(t12.x, t12.y), 0.0).rgb * (w12.x * w12.y) + textureLod(tex, vec2(t3.x, t12.y), 0.0).rgb * (w3.x * w12.y)
           + textureLod(tex, vec2(t0.x, t3.y), 0.0).rgb * (w0.x * w3.y) + textureLod(tex, vec2(t12.x, t3.y), 0.0).rgb * (w12.x * w3.y) + textureLod(tex, vec2(t3.x, t3.y), 0.0).rgb * (w3.x * w3.y);
    // four fifths of it: alone it came out 3 % sharper than the drawn eye (mean gradient 23.6 against 23.0; bilinear 20.7)
    return mix(texture(tex, at).rgb, max(c, vec3(0.0)), 0.8);
}
float toDistance(float d) { return min(d > 1e-7 ? warp.params.y / d : warp.params.z, warp.params.z); }
// The kept depth between its texels (bilinear; the reversed depth is linear over a plane), so that the walk along the
// ray crosses a surface at one place and not at a texel's step: the crossing's place, and the colour taken there,
// are then true to a fraction of a pixel (with the nearest texel the strip beside an edge sat up to a pixel off).
// Not across a step between the four texels (the edge of something nearer): there the blend would be a skin
// stretched from the near thing to the far one, and the walk would stop on it (a smear beside every edge); the
// nearest texel is taken instead.
float distanceAt(vec2 at) {
    vec4 g = textureGather(depthTex, at, 0);   // (i0,j1) (i1,j1) (i1,j0) (i0,j0)
    vec4 d = vec4(toDistance(g.x), toDistance(g.y), toDistance(g.z), toDistance(g.w));
    float dMin = min(min(d.x, d.y), min(d.z, d.w)), dMax = max(max(d.x, d.y), max(d.z, d.w));
    if (dMax - dMin > 0.1 * dMin + 0.002) return toDistance(textureLod(depthTex, at, 0.0).r);
    vec2 fr = fract(at * vec2(textureSize(depthTex, 0)) - 0.5);
    return toDistance(mix(mix(g.w, g.z, fr.x), mix(g.x, g.y, fr.x), fr.y));
}
// a picture's place (0..1) of a point of its eye's view space
vec2 placeIn(vec3 p, vec4 tan) {
    float w = -p.z;
    return vec2((p.x / w - tan.x) / (tan.y - tan.x), (tan.w - p.y / w) / (tan.w - tan.z));
}
vec2 place(vec3 p) { return placeIn(p, warp.oldTan); }
bool inside(vec2 at) { return all(greaterThanEqual(at, vec2(0.0))) && all(lessThanEqual(at, vec2(1.0))); }
// the point of the ray p(s) = ((-s - b.z) / a.z) * a + b at the kept eye's distance s
vec3 along(vec3 a, vec3 b, float s) { return ((-s - b.z) / a.z) * a + b; }
// Walks on from step i (even in 1 / s, steps of a pixel): the distance of the next crossing of the ray with the kept
// surface, or -1 when the walk is over. The state (i, front, sPrev) lets the walk go on past a crossing.
float nextCrossing(vec3 a, vec3 b, float iNear, float iFar, int steps, inout int i, inout bool front, inout float sPrev) {
    while (i <= steps) {
        float s = 1.0 / mix(iNear, iFar, float(i) / float(steps));
        ++i;
        vec2 p = place(along(a, b, s));
        if (!inside(p)) { front = false; sPrev = s; continue; }
        float there = distanceAt(p);
        bool behind = s >= there;
        if (behind && front) {
            float lo = sPrev, hi = s;
            for (int k = 0; k < 5; ++k) {
                float m = 0.5 * (lo + hi);
                if (m >= distanceAt(place(along(a, b, m)))) hi = m; else lo = m;
            }
            float m = 0.5 * (lo + hi);
            front = false;
            sPrev = s;
            // a crossing: the surface holds the ray's distance at the place - not the edge of something nearer
            if (abs(m - distanceAt(place(along(a, b, m)))) <= 0.02 * m + 0.003) return m;
            continue;
        }
        front = !behind;
        sPrev = s;
    }
    return -1.0;
}
// Mode 2: whether the other eye cannot see the point p (kept eye's view space) now: something nearer stands in front
// of it in the other eye's picture. Outside that picture, or too close to it: unknown, taken as hidden.
bool hiddenFromOther(vec3 p) {
    vec3 n = (warp.oldToNew * vec4(p, 1.0)).xyz;
    mat3 R2 = mat3(extra.otherToNew);
    vec3 o = (n - extra.otherToNew[3].xyz) * R2;   // R2^T (n - T2)
    float d = -o.z;
    if (d < 0.02) return true;
    vec2 at = placeIn(o, extra.otherTan);
    if (!inside(at)) return true;
    return toDistance(textureLod(otherDepth, at, 0.0).r) < d * 0.98 - 0.003;
}
// Mode 2, with nothing of this eye's last picture fit for the hole (what lay there has moved since: the hand or the
// weapon a frame ago): the background beside the occluder, from the other eye's picture of this frame. The ray is
// walked in that eye from near to far; once its place has been behind the occluder (the other eye's surface nearer
// than the ray's point) and comes out again, the colour there is taken - the background smeared across the hole,
// never a ghost of the weapon. A ray that was never behind anything (a hole at the picture's edge) keeps the fallback.
vec4 beyondTheOccluder(vec3 rn, vec4 fallback) {
    mat3 R2 = mat3(extra.otherToNew);
    vec3 a2 = rn * R2, b2 = -(extra.otherToNew[3].xyz * R2);
    if (a2.z >= -1e-3) return fallback;
    vec2 size2 = vec2(textureSize(otherColor, 0));
    float sNear = 0.12, sFar = warp.params.z;
    float gap = length((placeIn(along(a2, b2, sFar), extra.otherTan) - placeIn(along(a2, b2, sNear), extra.otherTan)) * size2);
    int steps = clamp(int(ceil(gap)), 2, 96);
    float iNear = 1.0 / sNear, iFar = 1.0 / sFar;
    bool wasBehind = false;
    for (int i = 0; i <= steps; ++i) {
        float s = 1.0 / mix(iNear, iFar, float(i) / float(steps));
        vec2 at = placeIn(along(a2, b2, s), extra.otherTan);
        if (!inside(at)) continue;
        float there = toDistance(textureLod(otherDepth, at, 0.0).r);
        if (there < s * 0.98 - 0.003) { wasBehind = true; continue; }
        if (wasBehind) return vec4(colourAt(otherColor, at), 1.0);
    }
    return fallback;
}
void main() {
    vec2 size = vec2(textureSize(colorTex, 0));
    // this fragment's ray in the new eye's view space, taken into the kept eye's: p(t) = t * a + b
    vec2 f = gl_FragCoord.xy / size;
    vec3 rn = vec3(warp.newTan.x + (warp.newTan.y - warp.newTan.x) * f.x, warp.newTan.w - (warp.newTan.w - warp.newTan.z) * f.y, -1.0);
    mat3 R = mat3(warp.oldToNew);
    vec3 a = rn * R, b = -(warp.oldToNew[3].xyz * R);   // R^T rn, -R^T T
    if (warp.params.w > 1.5) {
        vec4 plain = vec4(colourAt(colorTex, f), 1.0);   // the picture as it is: the last resort
        if (a.z >= -1e-3) { outColor = plain; return; }
        float sNear = 0.12, sFar = warp.params.z;
        float gap = length((place(along(a, b, sFar)) - place(along(a, b, sNear))) * size);
        if (gap < 0.75) {
            // the ray's place hardly depends on the distance: the surface is at the distance the picture has there
            float s = distanceAt(place(along(a, b, 1.0)));
            s = distanceAt(place(along(a, b, s)));
            vec3 p = along(a, b, s);
            outColor = hiddenFromOther(p) ? vec4(colourAt(colorTex, place(p)), 1.0) : beyondTheOccluder(rn, plain);
            return;
        }
        int steps = clamp(int(ceil(gap)), 2, 128);
        float iNear = 1.0 / sNear, iFar = 1.0 / sFar;
        int i = 0;
        bool front = false;
        float sPrev = sNear;
        for (int n = 0; n < 4; ++n) {   // up to four surfaces along the ray
            float s = nextCrossing(a, b, iNear, iFar, steps, i, front, sPrev);
            if (s < 0.0) break;
            vec3 p = along(a, b, s);
            if (hiddenFromOther(p)) { outColor = vec4(colourAt(colorTex, place(p)), 1.0); return; }
        }
        outColor = beyondTheOccluder(rn, plain);
        return;
    }
    bool stepped = a.z < -1e-3 && cellRange.y > cellRange.x * 1.002 + 0.001;
    float sNear = cellRange.x * 0.98, sFar = min(cellRange.y * 1.02, warp.params.z);
    float gap = 0.0;
    if (stepped) gap = length((place(along(a, b, sFar)) - place(along(a, b, sNear))) * size);   // how far the ray's places in the kept picture lie apart
    if (gap < 0.75) { outColor = vec4(colourAt(colorTex, uv), 1.0); return; }   // one surface: the stretched place is the right one
    int steps = clamp(int(ceil(gap)), 2, 128);
    float iNear = 1.0 / sNear, iFar = 1.0 / sFar;
    int i = 0;
    bool front = false;
    float sPrev = sNear;
    float s = nextCrossing(a, b, iNear, iFar, steps, i, front, sPrev);
    if (s >= 0.0) { outColor = vec4(colourAt(colorTex, place(along(a, b, s))), 1.0); return; }
    if (warp.params.w > 0.5) discard;
    outColor = vec4(colourAt(colorTex, uv), 1.0);
}
