#version 450
// Alternate-eye rendering, second and third stage (2026-10-15, user: "the PureDark way"): the eye not drawn in this
// frame shows a picture warped to the pose of this frame. A grid of cells over a kept picture; every vertex takes the
// depth the game wrote there, becomes a point in the kept eye's view space (the game's depth is reversed and
// infinite: depth = near / distance, 0 at the sky), moves into the new eye's view space and is projected with the
// new field of view. The fragments sample the kept colour at the vertex's old place (perspective-correct).
// Second stage: the kept picture is this eye's last drawn one (mode 0: the grid stretches over a depth step, so the
// picture has no holes). Third stage: the kept picture is the other eye's picture of this same frame (mode 1).
// A cell that stretches over a depth step covers the strip between the near and the far side in the new picture;
// its fragments do not take the stretched colour but find, each for its own ray, the first point of the kept
// picture's surface along it (eye_warp.frag: a search along the ray between the cell's nearest and farthest
// distance). Where the ray meets nothing the kept eye saw, the fragment is left out (mode 1: a hole) or takes the
// stretched colour (mode 0). So the edges are the picture's own, not the grid's (before: the grid's 6 px saw-teeth).
// Mode 2 (the hole fill, after mode 1): one triangle over the whole picture at depth 0.5, drawn only where the grid
// wrote no depth (the holes); its fragments take this eye's own last picture (eye_warp.frag).
layout(set = 0, binding = 1) uniform sampler2D depthTex;   // NEAREST
// The kept picture's things' own motion over its game frame (2026-10-23, the mod's own frames for every refresh,
// include/evr/afw_pacing.hpp pacing 2): the game's motion vectors less the camera's (layer/eye_motion_fix.hpp), in UV,
// previous - current. A refresh between the game's frames carries a moving thing on by motion.x of its frame's motion: the
// hands and the weapon, which move with the player, keep their place while the world is carried on by the walk.
layout(set = 0, binding = 3) uniform sampler2D motionTex;
layout(push_constant) uniform Warp {
    mat4 oldToNew;   // kept eye's view space -> new eye's view space, for vec4(point, 1)
    vec4 oldTan;     // tan of the kept fov: left, right, down, up (left and down negative)
    vec4 newTan;     // the same for this frame's fov
    vec4 params;     // x: cells along each side, y: near (depth = near / distance), z: the distance shown for depth 0, w: mode (0 stretch, 1 holes, 2 fill)
    vec4 motion;     // x: the game frames to carry the things' motion on by (0: none)
} warp;
layout(location = 0) out vec2 uv;
layout(location = 1) out float dist;              // the distance at this vertex
layout(location = 2) flat out vec2 cellRange;     // the nearest and the farthest distance at the cell's four corners
layout(location = 3) flat out vec4 cellMotion;    // the things' own motion (times motion.x) at the cell's nearest corner (xy) and farthest (zw)
// The player's own things (2026-10-24; the mod's own frames for every refresh while walking, Mock game-20261024-033215 /
// 034401: the staff in the hand doubled and sawn, and clean with the walk's carry left out): the hands and what they hold
// move with the player, not with the world - a kept picture carried on by the walk (keptAt) carried them too. A thing is the
// player's own when it is nearer than ownNear and moved by itself over its frame by ownMotion pixels or more (the motion is
// the game's less the camera's: the world's is about 0, the held staff's the walk's). params.w = mode + 4 x separation:
// 0 none, 1 the world alone (the player's own things left out), 2 the player's own things alone (drawn uncarried, after the
// world); 3 / 4 the same by the distance alone (the kept picture had no motion). include/evr/eye_warp.hpp, EVR_AFW_OWN_NEAR.
layout(constant_id = 3) const float ownNear = 2.0;
layout(constant_id = 4) const float ownMotion = 1.5;
layout(constant_id = 5) const int ownCells = 1;   // 1: the player's own things' pass draws only the cells that hold them (EVR_AFW_OWN_CELLS=0: all)
float modeOf() { return mod(warp.params.w, 4.0); }
int separationOf() { int s = int(warp.params.w / 4.0 + 0.01); return s >= 3 ? s - 2 : s; }
bool ownByDistance() { return int(warp.params.w / 4.0 + 0.01) >= 3; }
float distanceAt(vec2 at) {
    float d = textureLod(depthTex, at, 0.0).r;
    return min(d > 1e-7 ? warp.params.y / d : warp.params.z, warp.params.z);
}
bool ownThing(vec2 at) {
    if (distanceAt(at) >= ownNear) return false;
    if (ownByDistance()) return true;
    vec2 m = textureLod(motionTex, at, 0.0).xy * vec2(textureSize(motionTex, 0));
    return dot(m, m) >= ownMotion * ownMotion;
}
// the things' own motion at a place; with the world alone none for the player's own things (left out there)
vec2 motionAt(vec2 at) { return separationOf() == 1 && ownThing(at) ? vec2(0.0) : textureLod(motionTex, at, 0.0).xy; }
void main() {
    if (modeOf() > 1.5) {
        vec2 corner = vec2(gl_VertexIndex == 1 ? 3.0 : -1.0, gl_VertexIndex == 2 ? 3.0 : -1.0);
        gl_Position = vec4(corner, 0.5, 1.0);
        uv = corner * 0.5 + 0.5;
        dist = 0.0;
        cellRange = vec2(0.0);
        cellMotion = vec4(0.0);
        return;
    }
    int cells = int(warp.params.x);
    int quad = gl_VertexIndex / 6, corner = gl_VertexIndex % 6;
    int qx = quad % cells, qy = quad / cells;
    // Painter's order: the new eye is at c = -R^T T in the kept eye's view space, beside it (the other eye) - of two
    // points that meet in the new picture, the nearer lies further towards c's side in the kept picture; the cells
    // are drawn towards that side, so the nearer is drawn last and shows.
    float side = -dot(warp.oldToNew[0].xyz, warp.oldToNew[3].xyz);
    if (side < 0.0) qx = cells - 1 - qx;
    int ox = (corner == 1 || corner == 3 || corner == 4) ? 1 : 0;
    int oy = (corner == 2 || corner == 4 || corner == 5) ? 1 : 0;
    float inv = 1.0 / float(cells);
    uv = vec2(float(qx + ox), float(qy + oy)) * inv;
    vec2 c0 = vec2(float(qx), float(qy)) * inv;
    float d00 = distanceAt(c0), d10 = distanceAt(c0 + vec2(inv, 0.0)), d01 = distanceAt(c0 + vec2(0.0, inv)), d11 = distanceAt(c0 + vec2(inv, inv));
    cellRange = vec2(min(min(d00, d10), min(d01, d11)), max(max(d00, d10), max(d01, d11)));
    // the player's own things alone: a cell with none of them near is left out - 5 x 5 places half a cell apart, from half a cell
    // before it to half a cell after it (the 3 x 3 inside it missed slivers of the staff's edge, which the world left out too: the
    // edge sawn, Mock game-20261024-040231)
    if (separationOf() == 2 && ownCells != 0) {
        bool any = false;
        for (int j = 0; j <= 4 && !any; ++j)
            for (int i = 0; i <= 4 && !any; ++i) any = ownThing(c0 + (vec2(float(i), float(j)) - 1.0) * (0.5 * inv));
        if (!any) { gl_Position = vec4(10.0, 10.0, 0.0, 1.0); dist = 0.0; cellMotion = vec4(0.0); return; }
    }
    cellMotion = vec4(0.0);
    if (warp.motion.x != 0.0) {
        // a cell over the edge of a thing that moves by itself (the staff in the hand) and of the world behind it: the walk
        // of a fragment's ray (eye_warp.frag) takes the near corner's motion near and the far corner's far
        vec2 corners[4] = vec2[4](c0, c0 + vec2(inv, 0.0), c0 + vec2(0.0, inv), c0 + vec2(inv, inv));
        float ds[4] = float[4](d00, d10, d01, d11);
        int near = 0, far = 0;
        for (int i = 1; i < 4; ++i) { if (ds[i] < ds[near]) near = i; if (ds[i] > ds[far]) far = i; }
        cellMotion = vec4(motionAt(corners[near]), motionAt(corners[far])) * warp.motion.x;
    }
    dist = ox == 0 ? (oy == 0 ? d00 : d01) : (oy == 0 ? d10 : d11);
    // where the thing at this place has moved on to (its own motion; the colour is still read at uv)
    vec2 moved = warp.motion.x != 0.0 ? uv - motionAt(uv) * warp.motion.x : uv;
    float x = warp.oldTan.x + (warp.oldTan.y - warp.oldTan.x) * moved.x;
    float y = warp.oldTan.w - (warp.oldTan.w - warp.oldTan.z) * moved.y;   // row 0 is the top of the picture
    vec4 p = warp.oldToNew * vec4(x * dist, y * dist, -dist, 1.0);
    float w = -p.z;
    if (w < 0.02) { gl_Position = vec4(10.0, 10.0, 0.0, 1.0); return; }   // behind the new eye: off the picture
    float u2 = (p.x / w - warp.newTan.x) / (warp.newTan.y - warp.newTan.x);
    float v2 = (warp.newTan.w - p.y / w) / (warp.newTan.w - warp.newTan.z);
    // the depth (2026-10-23): nearer smaller, below the hole fill's 0.5 - used where the grid is drawn with the depth test
    // (include/evr/eye_warp.hpp pipelineTested); the painter's order needs none
    gl_Position = vec4((u2 * 2.0 - 1.0) * w, (v2 * 2.0 - 1.0) * w, 0.49 * w / (w + 1.0) * w, w);
}
