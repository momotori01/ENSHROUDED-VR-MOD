#pragma once
// The consumable held in the right hand (consumable_hold.hpp) is shown by the model the game draws the item's icon with
// (keen::ItemInfo.iconModel, an ObjectReference<keen::Model> at +1404 = 0x57c of the 1.0 reflection; the registry holds
// the item as the archive does - category at 0xcc, item_registry.hpp). In the 1.0 game data every one of the 237
// consumables, animal and pet foods has one (docs/analysis/consumable_models_20261023). The game itself shows nothing in
// the hand when an item is used: the consumables have no visual entity (the water pouch aside) and their use sequences
// spawn no model. The icon models stand along +Y from about 0 and are drawn large for the icon (the small healing potion
// 31 cm, the bandage 43 cm, mushrooms 31..54 cm), so the one in the hand is scaled down to a hand's size and held as a
// weapon is held - its +Y along the fist (weapon_mount.hpp mountLiveWeapon) - with the middle of its bounds in the grip.
#include <evr/weapon_mount.hpp>
#include <algorithm>
#include <stdexcept>
namespace evr::motion {
constexpr uint64_t itemIconModel=0x57c;
struct ConsumableMount {Pose pose;float scale=1;};
// lo/hi: the model's bounds (its descriptor +16/+32); size: the longest side in the hand (metres), never enlarged.
inline ConsumableMount mountConsumable(Pose grip,Vec3 lo,Vec3 hi,float size,float pitchDegrees=vhvrWeaponPitchDegrees){
    if(!finite(lo)||!finite(hi)||!(hi.x>lo.x&&hi.y>lo.y&&hi.z>lo.z)||!(size>0)||!std::isfinite(size))throw std::invalid_argument("consumable model bounds");
    const float longest=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
    ConsumableMount m;m.scale=std::min(1.f,size/longest);m.pose=mountLiveWeapon(grip,pitchDegrees);
    m.pose.position=m.pose.position-rotate(m.pose.orientation,(lo+hi)*(.5f*m.scale));
    return m;
}
// How a consumable lies in the hand (2026-10-23, user after the first headset run: "the item is not at the middle of the
// palm"; "popcorn, walnuts and the like are in no container - their bottom should come to the palm, as if heaped on the
// hand"). Fist: held as a bottle, as above, the fingers closed around it. Palm: lying on the open palm, the model's
// bottom (its lowest Y) on the palm, its +Y out of the palm.
// Tail: a whole fish, as the caught fish is held while fishing (motion_body_ik.hpp; 2026-10-23, user: "the fish standing
// upright on the hand is odd - hold it as when fishing"): the fist round the tail fin's base, 20 % of its length in from
// the tail's end, the fish hanging head first out of the fist's little-finger side (a held blade's way turned round).
enum class ConsumableHold:uint8_t {Fist=0,Palm=1,Tail=2};
// A rotation from the three axes it turns X, Y and Z into (orthonormal, right-handed).
inline Quat quatFromAxes(Vec3 x,Vec3 y,Vec3 z){
    const float t=x.x+y.y+z.z;Quat q;
    if(t>0){const float s=std::sqrt(t+1)*2;q={(y.z-z.y)/s,(z.x-x.z)/s,(x.y-y.x)/s,.25f*s};}
    else if(x.x>y.y&&x.x>z.z){const float s=std::sqrt(1+x.x-y.y-z.z)*2;q={.25f*s,(y.x+x.y)/s,(z.x+x.z)/s,(y.z-z.y)/s};}
    else if(y.y>z.z){const float s=std::sqrt(1+y.y-x.x-z.z)*2;q={(y.x+x.y)/s,.25f*s,(z.y+y.z)/s,(z.x-x.z)/s};}
    else{const float s=std::sqrt(1+z.z-x.x-y.y)*2;q={(z.x+x.z)/s,(z.y+y.z)/s,.25f*s,(x.y-y.x)/s};}
    return normalized(q);
}
// The palm of a hand from its rest-pose joints (all in one space): the wrist and the bases of the index, middle and little
// fingers. +Y out of the palm, +Z from the fingers toward the wrist; the point halfway from the wrist to the middle
// finger's base, moved out to the palm's skin by surface metres. right: the right hand (its thumb on the other side).
inline Pose palmFrame(Vec3 wrist,Vec3 index,Vec3 middle,Vec3 little,bool right,float surface=.015f){
    const auto unit=[](Vec3 v){const float n=length(v);if(!(n>1e-5f)||!std::isfinite(n))throw std::invalid_argument("palm joints coincide");return v*(1/n);};
    const Vec3 toFingers=unit(middle-wrist),across=unit(index-little);
    Vec3 out=unit(right?cross(across,toFingers):cross(toFingers,across));
    const Vec3 z=toFingers*-1.f;const Vec3 y=unit(out-z*dot(out,z));const Vec3 x=cross(y,z);
    Pose p;p.orientation=quatFromAxes(x,y,z);p.position=(wrist+middle)*.5f+y*surface;return p;
}
// On the palm: the model's +Y along the palm's, the middle of its bounds over the palm's middle, its bottom on the skin.
inline ConsumableMount mountOnPalm(Pose palm,Vec3 lo,Vec3 hi,float size){
    if(!finite(lo)||!finite(hi)||!(hi.x>lo.x&&hi.y>lo.y&&hi.z>lo.z)||!(size>0)||!std::isfinite(size))throw std::invalid_argument("consumable model bounds");
    const float longest=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z});
    ConsumableMount m;m.scale=std::min(1.f,size/longest);m.pose.orientation=normalized(palm.orientation);
    const Vec3 base{(lo.x+hi.x)*.5f,lo.y,(lo.z+hi.z)*.5f};
    m.pose.position=palm.position-rotate(m.pose.orientation,base*m.scale);
    return m;
}
// By the tail: the longest axis is the fish's length, the head at its maximum (the models face +Z; the guess of
// motion_body_ik.hpp, which the fishing kept); the grip at gripShare of it from the tail's end.
// size 0: the model's own size (a fish as large as the caught one: the same model, its template's scale 1 - 2026-10-23).
inline ConsumableMount mountByTail(Pose grip,Vec3 lo,Vec3 hi,float size,float pitchDegrees=vhvrWeaponPitchDegrees,float gripShare=.2f){
    if(!finite(lo)||!finite(hi)||!(hi.x>lo.x&&hi.y>lo.y&&hi.z>lo.z)||!(size>=0)||!std::isfinite(size))throw std::invalid_argument("consumable model bounds");
    const Vec3 d=hi-lo;const float half=.70710678f;Quat r;Vec3 base=(lo+hi)*.5f;float longest;
    if(d.z>=d.x&&d.z>=d.y){r={half,0,0,half};base.z=lo.z+gripShare*d.z;longest=d.z;}            // about X by +90: Z to -Y
    else if(d.x>=d.y){r={0,0,-half,half};base.x=lo.x+gripShare*d.x;longest=d.x;}               // about Z by -90: X to -Y
    else{r={1,0,0,0};base.y=lo.y+gripShare*d.y;longest=d.y;}                                     // about X by 180: Y to -Y
    ConsumableMount m;m.scale=size>0?std::min(1.f,size/longest):1.f;const auto held=mountLiveWeapon(grip,pitchDegrees);
    m.pose.orientation=normalized(multiply(held.orientation,r));m.pose.position=held.position-rotate(m.pose.orientation,base*m.scale);
    return m;
}
// Its longest side along the fist (+Y) or, on the palm, its longest level side along the fingers (the palm's Z): the turn
// of the model before it is held (identity when that side is +Y already / Z already).
inline Quat longestAlong(Vec3 lo,Vec3 hi,ConsumableHold hold){
    const Vec3 d=hi-lo;const float half=.70710678f;
    if(hold==ConsumableHold::Fist){
        if(d.x>=d.y&&d.x>=d.z)return Quat{0,0,half,half};      // about Z by +90 degrees: X to Y
        if(d.z>=d.y&&d.z>=d.x)return Quat{-half,0,0,half};     // about X by -90 degrees: Z to Y
        return Quat{0,0,0,1};
    }
    return d.x>d.z?Quat{0,-half,0,half}:Quat{0,0,0,1};          // about Y by -90 degrees: X to Z
}
// The bounds of the model turned by r.
inline void turnedBounds(Quat r,Vec3 lo,Vec3 hi,Vec3& outLo,Vec3& outHi){
    bool first=true;
    for(int k=0;k<8;++k){const Vec3 c=rotate(r,Vec3{k&1?hi.x:lo.x,k&2?hi.y:lo.y,k&4?hi.z:lo.z});
        if(first){outLo=outHi=c;first=false;continue;}
        outLo={std::min(outLo.x,c.x),std::min(outLo.y,c.y),std::min(outLo.z,c.z)};outHi={std::max(outHi.x,c.x),std::max(outHi.y,c.y),std::max(outHi.z,c.z)};}
}
} // namespace evr::motion
