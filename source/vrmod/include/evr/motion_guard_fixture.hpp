#pragma once
#include <memory>
#include <evr/controller_guard_fixture.hpp>
#include <evr/guard_input.hpp>
namespace evr::game {
// Finite test adapter around the same posture->LB policy intended for the real
// input adapter. Two proven holds; only the original game can accept blocking.
class MotionGuardFixture {
    motion::GuardInput gate_;motion::GuardEquipment weapon_;
    GuardObservation owner_{};uint64_t begin_=0,last_=0,holdAt_=0,releaseAt_=0;
    bool connected_=false,seen_=false;unsigned holds_=0;uint16_t previous_=0;
    ControllerGuardFixture::Phase phase_=ControllerGuardFixture::Phase::WaitConnection;
public:
    using Phase=ControllerGuardFixture::Phase;
    struct Result {Phase phase=Phase::WaitConnection;uint16_t buttons=0;motion::GuardInputResult input;};
    MotionGuardFixture(motion::GuardCalibration c,motion::Weapon w):gate_(c){weapon_.kind=motion::GuardKind::TwoHanded;weapon_.support=w;}
    Phase phase()const{return phase_;}unsigned holds()const{return holds_;}
    Result update(uint64_t now,bool connected,bool hud,const GuardObservation& o,const motion::HandFrame& h,uint64_t receipt,const motion::GuardBody& b){
        Result r;r.phase=phase_;if(phase_==Phase::Complete||phase_==Phase::Aborted)return r;
        auto abort=[&](){phase_=r.phase=Phase::Aborted;r.buttons=0;return r;};
        if(!begin_)begin_=last_=now;
        if(now<last_||now-begin_>90000||!hud||!o.valid||!o.root||!o.item||!o.binding||o.attacking||
           (o.digital!=0&&o.digital!=(1ull<<30)&&o.digital!=((1ull<<30)|(1ull<<4))))return abort();
        last_=now;
        if(!connected_){if(!connected)return r;if(o.blocking||o.digital)return abort();connected_=true;owner_=o;phase_=Phase::Settle;}
        if(!connected||o.root!=owner_.root||o.item!=owner_.item||o.binding!=owner_.binding)return abort();
        weapon_.binding=weapon_.support.binding=o.binding;
        motion::GuardAuthority n;n.enabled=n.known=n.local=n.alive=true;n.uiOrEvent=false;n.canBlock=n.canRequestParry=o.canBlock;
        r.input=gate_.update(now,h,receipt,b,weapon_,n);r.buttons=r.input.buttons;
        if(r.buttons&&!previous_){if(holds_>=2||releaseAt_&&(!seen_||o.blocking||o.digital))return abort();holdAt_=now;seen_=false;phase_=Phase::Hold;}
        if(previous_&&o.blocking&&(o.digital&(1ull<<30)))seen_=true;
        if(!r.buttons&&previous_){if(!seen_||now-holdAt_<1200)return abort();++holds_;releaseAt_=now;seen_=false;phase_=Phase::Release;}
        if(releaseAt_&&!r.buttons){seen_|=!o.blocking&&!o.digital;if(holds_==2&&seen_&&!o.blocking&&!o.digital&&now-releaseAt_>=1000)phase_=Phase::Complete;}
        previous_=r.buttons;r.phase=phase_;return r;
    }
};
}
