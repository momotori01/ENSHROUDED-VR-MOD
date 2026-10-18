#pragma once
#include <evr/guard_intent.hpp>
#include <evr/head_gesture.hpp>
namespace evr::motion {
// Explicit body calibration: no guessed user height or game head bone.
// Head pitch/roll never rotate the calibrated vertical navel offset.
class GuardBodyTracker {
    Vec3 offset_;uint64_t calibration_;float yaw_=0;bool have_=false;
    uint64_t frame_=0,session_=0,origin_=0;XrTime time_=0;
public:
    GuardBodyTracker(Vec3 headToNavel,uint64_t calibration):offset_(headToNavel),calibration_(calibration){
        if(!finite(offset_)||offset_.y>=0||!calibration_)throw std::invalid_argument("explicit head-to-navel calibration required");
    }
    GuardBody update(const HandFrame& f,const GestureHead& h){
        GuardBody b;b.frame=f.frame;b.session=f.sessionGeneration;b.origin=f.originGeneration;b.time=f.predictedTime;b.calibration=calibration_;
        if(!f.focused||!f.frame||!f.sessionGeneration||!f.originGeneration||f.predictedTime<=0||
           h.frame!=f.frame||h.session!=f.sessionGeneration||h.origin!=f.originGeneration||h.time!=f.predictedTime||
           (h.flags&15)!=15||!finite(h.pose.position)){have_=false;return b;}
        try {
            const bool sameEpoch=have_&&session_==f.sessionGeneration&&origin_==f.originGeneration;
            if(sameEpoch&&(f.frame<=frame_||f.predictedTime<=time_)){have_=false;return b;}
            const auto forward=rotate(h.pose.orientation,{0,0,-1});
            if(!sameEpoch&&forward.x*forward.x+forward.z*forward.z<=1e-10f){have_=false;return b;}
            yaw_=heading(h.pose.orientation,sameEpoch?yaw_:0.f);
            const auto q=euler(yaw_,0,0);b.navel=h.pose.position+rotate(q,offset_);b.forward=rotate(q,{0,0,-1});b.head=h.pose.position;
            b.known=true;have_=true;frame_=f.frame;session_=f.sessionGeneration;origin_=f.originGeneration;time_=f.predictedTime;return b;
        }catch(...){have_=false;return b;}
    }
};
struct GuardInputResult {uint16_t buttons=0;GuardIntent intent{};bool fresh=false,edge=false,released=false;};
// Native LB has its own parry edge. A valid posture without a newly armed entry
// may NOT re-press LB after startup, tracking/UI loss or native permission loss.
// No synthetic Parry72, actor writes, timing override or queued permission.
class GuardInput {
    GuardCalibration config_;GuardGate gate_;uint64_t frame_=0,session_=0,origin_=0,lastNow_=0;
    bool held_=false;GuardIntent intent_{};
    void reset(){held_=false;frame_=session_=origin_=0;gate_=GuardGate(config_);intent_={};}
public:
    explicit GuardInput(GuardCalibration c):config_(c),gate_(c){}
    GuardInputResult update(uint64_t now,const HandFrame& f,uint64_t receipt,const GuardBody& b,const GuardEquipment& w,const GuardAuthority& n){
        GuardInputResult out;const bool was=held_;
        if(!receipt||now<receipt||now-receipt>200||(lastNow_&&now<lastNow_)||
           !n.enabled||!n.known||!n.local||!n.alive||n.uiOrEvent||n.reservedGesture||
           !f.focused||!b.known||b.frame!=f.frame||b.time!=f.predictedTime||b.session!=f.sessionGeneration||b.origin!=f.originGeneration){
            reset();lastNow_=now;out.released=was;return out;
        }
        lastNow_=now;out.fresh=true;
        const bool newFrame=f.frame!=frame_||f.sessionGeneration!=session_||f.originGeneration!=origin_;
        if(newFrame){
            intent_=gate_.update(f,b,w,n);frame_=f.frame;session_=f.sessionGeneration;origin_=f.originGeneration;
            if(!intent_.blockHeld)held_=false;
            else if(!held_&&intent_.parryCandidate){held_=true;out.edge=true;}
        }
        // Authority is sampled on every frontend call, even a repeated hand
        // sample. Clearing the latch cannot create a delayed edge on recovery.
        if(!n.canBlock||((n.attackRequested||n.attackInProgress)&&!n.attackAndGuardVerified))held_=false;
        if((w.kind==GuardKind::TwoHanded&&!twoHandSupportValid(f,w.support))||
           (w.kind==GuardKind::Shield&&(w.shieldHand>1||!f.hands[w.shieldHand].tracked()))||
           intent_.binding!=w.binding){reset();out.fresh=false;}
        out.intent=intent_;out.buttons=held_?0x100:0;out.released=was&&!held_;return out;
    }
};
} // namespace evr::motion
