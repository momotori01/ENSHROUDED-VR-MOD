#pragma once
#include <evr/combat_worlds.hpp>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace evr::motion {
// Values only; World numbers are identity tokens, never dereferenced here.
struct ColliderPose {
    bool valid=false;
    uint64_t frame=0,session=0,origin=0,milliseconds=0,serial=0,clientWorld=0;
    int64_t predictedTime=0;
    uint32_t root=0,item=0,child=0;
    std::array<uint8_t,56> transform{};
    std::array<uint8_t,48> attachment{};CombatWorlds worlds{};
    // The weapon's tip along the model's +Y, metres (the target gate's blade end; 0: not known). motion_collider_apply.hpp
    // starts the hit volume's sweep where the tip was, not where the grip was.
    float tipY=0;
};
struct ColliderEvidence {
    uint64_t now=0,frame=0,session=0,origin=0,handMs=0,actorMs=0,visualMs=0;
    uint64_t clientWorld=0,actorWorld=0,impactWorld=0;
    uint32_t root=0,actorEntity=0,item=0,visualItem=0,visualChild=0,impactRoot=0,impactItem=0,weaponPide=0,itemPide=0;
    bool normalUi=false,firstPerson=false,tracked=false,active=false,alive=false,ready=false,ownerStable=false,slotStable=false;
    std::array<uint8_t,48> visualAttachment{},impactAttachment{};
};
// The model scale contains animation/asset deformation; collision geometry
// retains native simulation scale and padding. Only tracked rigid pose crosses.
inline std::array<uint8_t,56> colliderTransform(const ColliderPose& p,const std::array<uint8_t,56>& native){
    auto out=native;std::memcpy(out.data(),p.transform.data(),40);return out;
}
// eitherHand: live play also takes weapons the game attaches to the right hand (slot 8); the finite trials
// proved slot 9 only.
inline const char* colliderPoseRejection(const ColliderPose& p,const ColliderEvidence& e,bool eitherHand=false){
    if(!p.worlds.empty())return "world_epoch_live_proof_required";
    auto fresh=[&](uint64_t t,uint64_t limit){return t&&e.now>=t&&e.now-t<=limit;};
    if(!p.valid||!p.frame||!p.serial||p.predictedTime<=0)return "pose_absent";
    if(!fresh(p.milliseconds,200)||!fresh(e.handMs,200)||!fresh(e.actorMs,200)||!fresh(e.visualMs,500))return "stale";
    if(!p.session||!p.origin||p.session!=e.session||p.origin!=e.origin||e.frame<p.frame||e.frame-p.frame>2)return "epoch";
    if(!e.normalUi||!e.firstPerson||!e.tracked||!e.active||!e.alive||!e.ready)return "gameplay";
    if(!p.clientWorld||p.clientWorld!=e.clientWorld||!e.actorWorld||e.actorWorld!=e.impactWorld)return "world";
    if(!p.root||p.root!=e.root||p.root!=e.actorEntity||p.root!=e.impactRoot||!p.item||p.item!=e.item||p.item!=e.visualItem||p.item!=e.impactItem||!p.child||p.child!=e.visualChild||!e.weaponPide||e.weaponPide!=e.itemPide||!e.ownerStable)return "ownership";
    if(!e.slotStable||p.attachment!=e.visualAttachment||p.attachment!=e.impactAttachment)return "attachment";
    uint32_t parent=0;std::memcpy(&parent,p.attachment.data()+44,4);
    if(parent!=p.root||p.attachment[40]!=0||(p.attachment[41]!=9&&!(eitherHand&&p.attachment[41]==8)))return "slot";
    // Only independently proved neutral attachment calibration is supported.
    std::array<float,10> offset{};std::memcpy(offset.data(),p.attachment.data(),40);
    for(unsigned i=0;i<10;i++)if(offset[i]!=(i==9?1.f:0.f))return "calibration";
    std::array<float,7> qs{};std::memcpy(qs.data(),p.transform.data()+24,28);
    float norm=0;for(unsigned i=0;i<7;i++)if(!std::isfinite(qs[i]))return "geometry";
    for(unsigned i=0;i<4;i++)norm+=qs[i]*qs[i];
    if(std::abs(norm-1.f)>1e-3f||qs[4]<=0||qs[5]<=0||qs[6]<=0)return "geometry";
    return nullptr;
}
}
