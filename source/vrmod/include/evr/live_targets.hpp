#pragma once
#include <evr/math.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

// VHVR starts a melee attack only when the weapon's box enters a target's collider, and looks at the hand speed at that
// moment (docs/analysis/vhvr_spec_20261010/melee.md, 3.3 and 3.4): a stroke through the air does nothing. This mod cannot
// ask the game's physics whether the weapon touches something, so it uses what it can read: the positions of the
// entities near the player that have Health (characters, trees, things that can be destroyed). A swing becomes an
// attack only while the blade is near one of them; whether the attack then hits is decided by the game's own hit
// volume on the weapon, as before. The reach around a target is deliberately generous: too small would swallow real
// hits, too large only lets a near miss start an attack.
namespace evr::motion {
struct LiveTarget {
    uint32_t entity=0;
    std::array<int64_t,3> position{}; // world, Q32 fixed point (2^32 per metre), the entity's origin (feet, base of a trunk)
    bool character=false;             // has an Actor: a person or a creature, with a body about 2 m high
    // keen::ecs::Destructible without a health (2026-10-20, user: "the sword does not break what cannot be locked on -
    // tables, pots; only a jump's fall attack does"): small things, a smaller reach (objectRadius, objectHeight)
    bool object=false;
};
struct LiveTargetReach {
    float characterRadius=.6f; // metres around the vertical axis of a character: its body (the aim mark stops there)
    // A stroke at a character counts from further out: the game's own hit boxes of a weapon reach 0.2..0.3 m to the
    // side of the blade and from the hilt (-0.2 m) past the tip (docs/analysis/contact_geometry_20261004_1948.json),
    // and a creature's body is wider than its axis. Headset run 20261014-124515: the strokes that attacked were
    // 0.26..0.60 m from the axis - bunched at the old limit 0.6 - and as many strokes again were taken for air
    // (user: "the one-hand sword often missed").
    float characterStrike=.85f;
    float otherRadius=1.3f;    // trees and objects: the big trees by the home are hit 1.1 m from their axis (game161403)
    float height=2.2f;         // the axis is taken this high from the origin
    // A table, a pot, a crate. (Headset run 20261020-100726, user: "strokes at pots miss easily": the strokes were pressed
    // with the blade 0.6..0.8 m from a pot's axis, and the hit volume came 80 ms later, after the blade had passed - the
    // hit volume now sweeps from the press on, motion_collider_apply.hpp sweepFromPress; the reach stays.)
    float objectRadius=.75f,objectHeight=1.2f;   // about a metre across and up to a metre high
};
// Shortest distance between the segments a..b and c..d.
inline float segmentDistance(Vec3 a,Vec3 b,Vec3 c,Vec3 d) {
    const Vec3 u=b-a,v=d-c,w=a-c;
    const float uu=dot(u,u),uv=dot(u,v),vv=dot(v,v),uw=dot(u,w),vw=dot(v,w);
    const float denominator=uu*vv-uv*uv;
    float s=0,t=0;
    if(uu<1e-9f&&vv<1e-9f)return length(w);
    if(uu<1e-9f)t=std::clamp(vw/vv,0.f,1.f);
    else if(vv<1e-9f)s=std::clamp(-uw/uu,0.f,1.f);
    else{
        s=denominator>1e-9f?std::clamp((uv*vw-vv*uw)/denominator,0.f,1.f):0.f;
        t=std::clamp((uv*s+vw)/vv,0.f,1.f);
        s=std::clamp((uv*t-uw)/uu,0.f,1.f);
    }
    return length(w+u*s-v*t);
}
// Distance in metres from the blade (two world Q32 points) to the target's axis; the blade is taken relative to the target.
inline float bladeDistance(const std::array<int64_t,3>& from,const std::array<int64_t,3>& to,const LiveTarget& target,const LiveTargetReach& reach={}) {
    constexpr double q=1.0/4294967296.0;
    const Vec3 a{float(double(from[0]-target.position[0])*q),float(double(from[1]-target.position[1])*q),float(double(from[2]-target.position[2])*q)};
    const Vec3 b{float(double(to[0]-target.position[0])*q),float(double(to[1]-target.position[1])*q),float(double(to[2]-target.position[2])*q)};
    return segmentDistance(a,b,{0,0,0},{0,target.object?reach.objectHeight:reach.height,0});
}
// A point of the blade a little later, from its last two positions (world Q32).
// A stroke presses the game's attack button; the game reads the button on its next frame and starts the action on
// the next tick, and only then the hit volume exists: 80..140 ms after the stroke on the headset (run game145015,
// press to the first tick of the action; "the hit's sound is late"). VHVR attacks in the frame of the contact, which
// its game allows. Here the attack starts when the blade *will* be at the target after that delay, so the hit and
// its sound come when the real blade arrives. A gap or a jump of the tracking is not a movement: then the point
// stays where it is, and the movement that is added is limited.
inline std::array<int64_t,3> bladeAhead(const std::array<int64_t,3>& now,const std::array<int64_t,3>& before,double elapsedSeconds,double leadSeconds,double maximumMetres=.8) {
    constexpr double q=4294967296.0;
    if(!(elapsedSeconds>=.004)||!(elapsedSeconds<=.12)||!(leadSeconds>0)||!(maximumMetres>0))return now;
    double step[3],size=0;
    for(int i=0;i<3;++i){step[i]=double(now[i]-before[i])/q/elapsedSeconds*leadSeconds;size+=step[i]*step[i];}
    size=std::sqrt(size);
    if(!std::isfinite(size))return now;
    const double scale=size>maximumMetres?maximumMetres/size:1.0;
    return {now[0]+int64_t(step[0]*scale*q),now[1]+int64_t(step[1]*scale*q),now[2]+int64_t(step[2]*scale*q)};
}
inline bool bladeNear(const std::array<int64_t,3>& from,const std::array<int64_t,3>& to,const LiveTarget& target,const LiveTargetReach& reach={}) {
    const float distance=bladeDistance(from,to,target,reach);
    return std::isfinite(distance)&&distance<=(target.character?reach.characterStrike:target.object?reach.objectRadius:reach.otherRadius);
}
}
