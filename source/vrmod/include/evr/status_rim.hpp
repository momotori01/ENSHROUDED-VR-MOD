#pragma once
#include <algorithm>
#include <array>
#include <cmath>
namespace evr::game {
// The game's post vignette (render parameter +0x268: colour r, g, b and its alpha) as the share of the VR rim
// (hooks/view_resource_probe.cpp -> the layer's edge tint), 0 .. 1.
// The HUD's "Low Health Vignette" (0xaf3290) writes the parameter only while its own strength is above nothing: the
// style's colour (0.643, 0.149, 0.149 in the headset run game-20261013-105615) with an alpha that pulses 0.8 .. 1.
// Otherwise the HUD puts it back every frame to black with alpha 1 (0xa036f0, 0xaabfb0; the constructor 0x766020
// has white with alpha 1): "no vignette" is a colour, not an alpha of nothing. Until 2026-10-13 the alpha alone was
// taken, so the red rim was drawn at its full strength all the time (edge_tint_frame of every run looked at:
// game-20261013-005905, -105615, the Mock runs).
inline float lowHealthRim(const std::array<float,4>& v){
    for(const float c:v)if(!std::isfinite(c))return 0;
    const float most=std::max({v[0],v[1],v[2]}),least=std::min({v[0],v[1],v[2]});
    if(most<1/255.f||least>254/255.f)return 0;   // black or white: no vignette
    return std::clamp(v[3],0.f,1.f);
}
}
