#pragma once
#include <memory>
#include <evr/melee_session.hpp>
#include <evr/motion_attack_fixture.hpp>
namespace evr::game {
// Test-only lifetime around the UNLIMITED coordinator. It stops after a declared
// one/two native actions so existing GPU/lifetime evidence remains comparable.
class MeleeSessionFixture {
    motion::MeleeSession session_{{2,.3f,.6f,2,100000000,12,30},{120,1000,200}};
    motion::Weapon weapon_;unsigned limit_;bool gesture_,connected_=false,accepted_=false;
    uint64_t begin_=0,lastRequest_=0;MotionAttackFixture::Phase phase_=MotionAttackFixture::Phase::WaitConnection;
public:
    using Phase=MotionAttackFixture::Phase;using Result=MotionAttackFixture::Result;
    motion::MeleeCommand lastCommand;
    MeleeSessionFixture(motion::Weapon weapon,unsigned limit,bool gesture):weapon_(weapon),limit_(limit),gesture_(gesture){
        if(!weapon.twoHanded||weapon.mainHand!=1||limit<1||limit>2)throw std::invalid_argument("measured finite session fixture required");
    }
    Phase phase()const{return phase_;}
    void closeEpoch(){
        if(phase_==Phase::Complete||phase_==Phase::Aborted){lastCommand={};return;}
        lastCommand=session_.suspend(motion::MeleeStop::EpochChanged);phase_=Phase::Aborted;
    }
    Result update(uint64_t now,bool connected,bool normalUi,const motion::HandFrame& hands,uint64_t handMs,const MotionCombatObservation& s,const motion::HeadGestureResult* gesture=nullptr){
        lastCommand={}; // Never replay an event after the bounded harness has stopped.
        Result result;result.phase=phase_;result.request=lastRequest_;result.accepted=accepted_;result.completed=unsigned(session_.completed());
        if(phase_==Phase::Complete||phase_==Phase::Aborted)return result;
        auto abort=[&](){phase_=result.phase=Phase::Aborted;result.pulseLevel=0;return result;};
        if(!begin_)begin_=now;
        if(now<begin_||now-begin_>90000||!normalUi||!s.valid||!s.alive||!s.ready||!s.world||!s.binding||!s.nativeMs)return abort();
        if(!connected){if(connected_)return abort();return result;}connected_=true;
        motion::MeleeNative native;native.world=s.world;native.entity=s.entity;native.item=s.item;native.binding=s.binding;native.milliseconds=s.nativeMs;
        native.worlds=s.worlds;native.worldSerial=s.worlds.empty()?0:s.worlds.input.serial;
        native.known=s.valid;native.local=true;native.alive=s.alive;native.gameplay=normalUi&&s.ready;native.canPrimary=s.canPrimary;native.active=s.active;
        native.usedItem=s.usedItem;native.sequence=s.sequence;native.trigger=s.trigger;native.triggerTime=s.triggerTime;native.playedTime=s.playedTime;native.currentTime=s.currentTime;native.length=s.length;
        const bool reserved=gesture_&&(!gesture||motion::gestureInhibitsCombat(hands,*gesture));
        weapon_.binding=s.binding;lastCommand=session_.update(now,hands,handMs,weapon_,native,{true,true,reserved});
        const auto& command=lastCommand;
        if(command.revoked)return abort();
        if(command.issued){lastRequest_=command.issued;accepted_=false;}
        if(command.accepted)accepted_=true;
        if(session_.completed()>=limit_)phase_=Phase::Complete;
        else phase_=command.level?Phase::Press:command.current?Phase::Observe:Phase::WaitSwing;
        result.phase=phase_;result.request=lastRequest_;result.accepted=accepted_;result.completed=unsigned(session_.completed());
        result.pulseLevel=command.level;result.gesture=command.intent;result.remaining=command.remaining;result.total=command.total;return result;
    }
};
}
