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
    mat4 ownToNew;     // mode 1 with the eye's own history (warp.motion.w): its own last picture's view space -> this frame's
    vec4 ownTan;       // tan of that picture's fov
    vec4 history;      // x: the share of the other eye's new colour mixed into the own history (EVR_EYE_WARP_HISTORY_MIX); y: the
                       // box widened by this many times its size on each side (EVR_EYE_WARP_HISTORY_BOX; 0: as it is), below 0 no box
} extra;
layout(set = 0, binding = 3) uniform sampler2D motionTex;    // the things' own motion (eye_warp.vert)
layout(set = 1, binding = 0) uniform sampler2D otherColor;   // mode 2: the other eye's picture of this frame
layout(set = 1, binding = 1) uniform sampler2D otherDepth;   // mode 2: the other eye's depth of this frame
layout(push_constant) uniform Warp {
    mat4 oldToNew;
    vec4 oldTan;
    vec4 newTan;
    vec4 params;     // y: near, z: the distance for depth 0, w: mode
    vec4 motion;     // x: the game frames to carry the things' motion on by (eye_warp.vert); y: 1 = thin things are met (nextCrossing); z: the least walk (pixels) left a hole when it finds nothing (0: 0.75); w: 1 = the eye's own history (withHistory)
} warp;
layout(location = 0) in vec2 uv;
layout(location = 1) in float dist;
layout(location = 2) flat in vec2 cellRange;
layout(location = 3) flat in vec4 cellMotion;   // the motion at the cell's nearest corner (xy) and farthest (zw), eye_warp.vert
layout(location = 0) out vec4 outColor;
// The colour is read with a Catmull-Rom filter (2026-10-21; the alternate-eye warp's "everything shook": in a Mock turn
// the eye made by the warp was a tenth less sharp than the eye drawn - mean gradient 20.2 .. 21.1 against 22.2 .. 23.2 -
// so each eye went sharp, soft, sharp at half the frame rate; the bilinear read of the other eye's picture softens it).
// Nine bilinear reads (the weights of the 4 x 4 texels folded), never below 0, blended 4:1 with the bilinear read. sharpRead = 0 (EVR_EYE_WARP_SHARP=0): the
// bilinear read as before.
layout(constant_id = 0) const int sharpRead = 3;
layout(constant_id = 1) const float flatPixels = 0.3;   // include/evr/eye_warp.hpp, EVR_EYE_WARP_FLAT
// sharpRead = 3 (the default since 2026-10-23): Lanczos-3 over 6 x 6 texels, never below 0. The six weights of an axis
// from three sines: sin(pi (i - f)) = -(-1)^i sin(pi f), sin(pi (i - f) / 3) by the angle difference (the first form, with
// two sines a tap, cost the Mock about 5 frames a second).
void lanczosWeights(float f, out float w[6]) {
    float s = sin(3.14159265 * f), s3 = sin(1.04719755 * f), c3 = cos(1.04719755 * f);
    for (int k = 0; k < 6; ++k) {
        float i = float(k - 2), x = i - f;
        float a = ((k & 1) == 0 ? -s : s);   // i = k - 2: even k, even i
        float b = sin(1.04719755 * i) * c3 - cos(1.04719755 * i) * s3;
        w[k] = abs(x) < 1e-4 ? 1.0 : 3.0 * a * b / (9.8696044 * x * x);
    }
}
vec3 lanczosAt(sampler2D tex, vec2 at) {
    ivec2 size = textureSize(tex, 0);
    vec2 p = at * vec2(size) - 0.5, base = floor(p), f = p - base;
    float wx[6], wy[6];
    lanczosWeights(f.x, wx);
    lanczosWeights(f.y, wy);
    vec3 sum = vec3(0.0);float weight = 0.0;
    for (int j = 0; j < 6; ++j) {
        vec3 row = vec3(0.0);float rowWeight = 0.0;
        for (int i = 0; i < 6; ++i) {
            ivec2 t = clamp(ivec2(base) + ivec2(i - 2, j - 2), ivec2(0), size - 1);
            row += texelFetch(tex, t, 0).rgb * wx[i];rowWeight += wx[i];
        }
        sum += row * wy[j];weight += rowWeight * wy[j];
    }
    return max(sum / weight, vec3(0.0));
}
vec3 colourAt(sampler2D tex, vec2 at) {
    if (sharpRead == 0) return texture(tex, at).rgb;
    if (sharpRead == 3) return lanczosAt(tex, at);
    vec2 size = vec2(textureSize(tex, 0)), p = at * size, t1 = floor(p - 0.5) + 0.5, f = p - t1;
    vec2 w0 = f * (-0.5 + f * (1.0 - 0.5 * f)), w1 = 1.0 + f * f * (-2.5 + 1.5 * f), w2 = f * (0.5 + f * (2.0 - 1.5 * f)), w3 = f * f * (-0.5 + 0.5 * f);
    vec2 w12 = w1 + w2, t0 = (t1 - 1.0) / size, t3 = (t1 + 2.0) / size, t12 = (t1 + w2 / w12) / size;
    vec3 c = textureLod(tex, vec2(t0.x, t0.y), 0.0).rgb * (w0.x * w0.y) + textureLod(tex, vec2(t12.x, t0.y), 0.0).rgb * (w12.x * w0.y) + textureLod(tex, vec2(t3.x, t0.y), 0.0).rgb * (w3.x * w0.y)
           + textureLod(tex, vec2(t0.x, t12.y), 0.0).rgb * (w0.x * w12.y) + textureLod(tex, vec2(t12.x, t12.y), 0.0).rgb * (w12.x * w12.y) + textureLod(tex, vec2(t3.x, t12.y), 0.0).rgb * (w3.x * w12.y)
           + textureLod(tex, vec2(t0.x, t3.y), 0.0).rgb * (w0.x * w3.y) + textureLod(tex, vec2(t12.x, t3.y), 0.0).rgb * (w12.x * w3.y) + textureLod(tex, vec2(t3.x, t3.y), 0.0).rgb * (w3.x * w3.y);
    // four fifths of it: alone it came out 3 % sharper than the drawn eye (mean gradient 23.6 against 23.0; bilinear 20.7)
    // sharpRead = 2 (EVR_EYE_WARP_SHARP=2, 2026-10-23): Catmull-Rom alone. In the self-check's same frame (the made right
    // eye against the drawn one, holes left out, grass by the house) the 4:1 blend came out 0.86 as sharp as the drawn eye.
    if (sharpRead == 2) return max(c, vec3(0.0));
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
// The kept picture's place whose thing has moved on to the place `at` (its own motion carried on, eye_warp.vert): one step
// back along the motion found there (the motion is smooth within a thing).
vec2 sourceOf(vec2 at) { return warp.motion.x != 0.0 ? at + textureLod(motionTex, at, 0.0).xy * warp.motion.x : at; }
vec2 place(vec3 p) { return sourceOf(placeIn(p, warp.oldTan)); }
// A grid cell's walk (2026-10-23): a point of the ray at distance s looks for the kept picture's place whose thing has moved
// there by the motion of the cell's corner at that depth - its nearest corner's near, its farthest's far (a thing moving
// by itself in front of the world: the staff in the hand, carried on with the player while the world is carried by the walk).
vec2 placeAtDistance(vec3 p, float s) {
    if (warp.motion.x == 0.0) return placeIn(p, warp.oldTan);
    float t = cellRange.y > cellRange.x ? clamp((s - cellRange.x) / (cellRange.y - cellRange.x), 0.0, 1.0) : 0.0;
    return placeIn(p, warp.oldTan) + mix(cellMotion.xy, cellMotion.zw, step(0.5, t));
}
bool inside(vec2 at) { return all(greaterThanEqual(at, vec2(0.0))) && all(lessThanEqual(at, vec2(1.0))); }
// The eye's own last drawn picture where it shows the same surface (2026-10-23, the user: "max still shakes - not usable
// yet"; Mock: the eye's pictures in turn, drawn and made, both stood off the mean of their neighbours by twice the motion's
// step, most along the skyline, the leaves and the ground - each eye showed its own DLSS picture and the other eye's in
// turns, whose grain at edges, leaves and grass differs): the point found in the other eye's picture is looked up in the
// eye's own last picture (set 1 in mode 1); where that holds the same distance, its colour is taken, kept inside the
// colours of the other eye's 3 x 3 texels around the place (as a TAA keeps its history: what moved or changed since comes
// from the other eye). PureDark's AFW has the like as its previous frame warping. p: the point in the kept eye's view
// space, at: its place in the kept picture, made: the colour read there.
vec3 withHistory(vec3 p, vec2 at, vec3 made) {
    vec3 n = (warp.oldToNew * vec4(p, 1.0)).xyz;
    mat3 Rh = mat3(extra.ownToNew);
    vec3 o = (n - extra.ownToNew[3].xyz) * Rh;   // Rh^T (n - Th)
    float d = -o.z;
    if (d < 0.02) return made;
    vec2 h = placeIn(o, extra.ownTan);
    if (!inside(h)) return made;
    // something else lies there in the own picture (moved, or hidden then). warp.motion.w 2: the own picture's distances
    // around the place - an edge's pixel has the colour of both sides and the depth of one, and the outlines kept the other
    // eye's colour (2026-10-23, the user: "the outlines still shake, even standing"; Mock: what flickered still were the
    // twigs, the leaves and the blades against what lies behind).
    if (warp.motion.w > 1.5) {
        vec2 t2 = 1.0 / vec2(textureSize(otherDepth, 0));
        float near = 1e9, far = 0.0;
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i) {
                float x = toDistance(textureLod(otherDepth, h + vec2(float(i), float(j)) * t2, 0.0).r);
                near = min(near, x);
                far = max(far, x);
            }
        if (d < near * 0.97 - 0.01 || d > far * 1.03 + 0.01) return made;
    } else if (abs(toDistance(textureLod(otherDepth, h, 0.0).r) - d) > 0.03 * d + 0.01) return made;
    // No box by default (2026-10-23 15:20; Mock, the same still place, the eye's pictures in turn against the mean of their
    // neighbours, mean / 99th percentile: one eye a frame without the history 2.69 / 22.4, with the box 0.97 / 9.6, the box
    // widened once 0.67 / 6.4, without the box 0.45 / 4.5, both eyes drawn 0.44 / 2.0; walking the same against the motion).
    // The box carried the other eye's own grain into the eye: the outlines still shook. What moved is left out by the
    // distances above. Mixing the other eye's colour in made it worse (0.5: 1.15 / 11.0).
    vec3 own = colourAt(otherColor, h);
    if (extra.history.y >= 0.0) {
        vec2 texel = 1.0 / vec2(textureSize(colorTex, 0));
        vec3 lo = made, hi = made;
        for (int j = -1; j <= 1; ++j)
            for (int i = -1; i <= 1; ++i) {
                vec3 c = textureLod(colorTex, at + vec2(float(i), float(j)) * texel, 0.0).rgb;
                lo = min(lo, c);
                hi = max(hi, c);
            }
        vec3 wide = (hi - lo) * extra.history.y;
        own = clamp(own, lo - wide, hi + wide);
    }
    return mix(own, made, extra.history.x);
}
// the kept eye's view-space point at the place `at` of its picture, at the distance d
vec3 pointAt(vec2 at, float d) {
    return vec3((warp.oldTan.x + (warp.oldTan.y - warp.oldTan.x) * at.x) * d, (warp.oldTan.w - (warp.oldTan.w - warp.oldTan.z) * at.y) * d, -d);
}
bool ownHistory() { return warp.motion.w > 0.5 && warp.params.w > 0.5 && warp.params.w < 1.5; }
// the place a walk looks at: the grid's by its cell's corners, the hole fill's (mode 2) by the motion found there
vec2 walkPlace(vec3 p, float s) { return warp.params.w > 1.5 ? place(p) : placeAtDistance(p, s); }
// the point of the ray p(s) = ((-s - b.z) / a.z) * a + b at the kept eye's distance s
vec3 along(vec3 a, vec3 b, float s) { return ((-s - b.z) / a.z) * a + b; }
// Walks on from step i (even in 1 / s, steps of a pixel): the distance of the next crossing of the ray with the kept
// surface, or -1 when the walk is over. The state (i, front, sPrev) lets the walk go on past a crossing.
float nextCrossing(vec3 a, vec3 b, float iNear, float iFar, int steps, inout int i, inout bool front, inout float sPrev) {
    while (i <= steps) {
        float s = 1.0 / mix(iNear, iFar, float(i) / float(steps));
        ++i;
        vec2 p = walkPlace(along(a, b, s), s);
        if (!inside(p)) { front = false; sPrev = s; continue; }
        float there = distanceAt(p);
        bool behind = s >= there;
        // A thin thing (a twig, a leaf, a blade of grass: a pixel or two wide) is met only at the one step that lands on it;
        // when that step is still in front of it and the next lies beyond it, the ray passes its distance between the two
        // and the walk went on to what lies behind - about every other time (2026-10-23, the user: "the outlines still shake,
        // even standing"; Mock: what still flickered were the twigs against the sky and the blades of grass). Taken as met
        // when the next step would be past it.
        if (!behind && front && warp.motion.y > 0.5 && i <= steps) {
            float sNext = 1.0 / mix(iNear, iFar, float(i) / float(steps));
            if (there < sNext && abs(there - distanceAt(walkPlace(along(a, b, there), there))) <= 0.02 * there + 0.003) {
                front = false;
                sPrev = s;
                return there;
            }
        }
        if (behind && front) {
            float lo = sPrev, hi = s;
            for (int k = 0; k < 5; ++k) {
                float m = 0.5 * (lo + hi);
                if (m >= distanceAt(walkPlace(along(a, b, m), m))) hi = m; else lo = m;
            }
            float m = 0.5 * (lo + hi);
            front = false;
            sPrev = s;
            // a crossing: the surface holds the ray's distance at the place - not the edge of something nearer
            if (abs(m - distanceAt(walkPlace(along(a, b, m), m))) <= 0.02 * m + 0.003) return m;
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
    if (stepped) gap = length((walkPlace(along(a, b, sFar), sFar) - walkPlace(along(a, b, sNear), sNear)) * size);   // how far the ray's places in the kept picture lie apart
    if (gap < flatPixels) {   // one surface: the stretched place is the right one
        vec3 c = colourAt(colorTex, uv);
        outColor = vec4(ownHistory() ? withHistory(pointAt(uv, dist), uv, c) : c, 1.0);
        return;
    }
    int steps = clamp(int(ceil(gap)), 2, 128);
    float iNear = 1.0 / sNear, iFar = 1.0 / sFar;
    int i = 0;
    bool front = false;
    float sPrev = sNear;
    float s = nextCrossing(a, b, iNear, iFar, steps, i, front, sPrev);
    if (s >= 0.0) {
        vec3 p = along(a, b, s);
        vec2 at = walkPlace(p, s);
        vec3 c = colourAt(colorTex, at);
        outColor = vec4(ownHistory() ? withHistory(p, at, c) : c, 1.0);
        return;
    }
    // a walk below the old tolerance (0.75 pixel) that found no surface keeps the stretched place, as before 2026-10-23,
    // instead of a hole: only the places the walk finds are taken more exactly (EVR_EYE_WARP_FLAT)
    if (warp.params.w > 0.5 && gap >= max(0.75, warp.motion.z)) discard;
    outColor = vec4(colourAt(colorTex, uv), 1.0);
}
