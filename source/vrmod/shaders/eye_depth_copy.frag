#version 450
// The game's depth of an eye (the image it hands DLSS; sampled, never copied: it has no transfer-source usage) into
// the eye warp's own R32 image, pixel for pixel (eye_warp.hpp keep, with ui_transfer.vert's full-screen triangle).
layout(set = 0, binding = 0) uniform sampler2D depthTex;
layout(location = 0) out float outDepth;
void main() { outDepth = texelFetch(depthTex, ivec2(gl_FragCoord.xy), 0).r; }
