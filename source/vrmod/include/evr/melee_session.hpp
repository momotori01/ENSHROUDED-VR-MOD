#pragma once
#include <evr/motion_intent.hpp>
#include <evr/combat_worlds.hpp>
namespace evr::motion {
// Values captured by a verified native adapter. Native timing is never compared
// to milliseconds; it is used only as an intra-action recovery ratio.
struct MeleeNative {
    uint64_t world=0,binding=0,milliseconds=0;uint32_t entity=0,item=0,usedItem=0,sequence=0;uint16_t trigger=0;
    int64_t triggerTime=0,playedTime=0,currentTime=0,length=0;
    bool known=false,local=false,alive=false,gameplay=false,canPrimary=false,active=false;
    // Zero is the existing finite diagnostic adapter, not verified lifetime coverage.
    uint64_t worldSerial=0;CombatWorlds worlds{};
};
struct MeleePermission {bool enabled=false,ownsInput=false,reserved=false;};
struct MeleeDelivery {
    uint64_t pulseMs=0,acknowledgementMs=0,maximumAgeMs=0;
    bool valid()const{return pulseMs>0&&acknowledgementMs>pulseMs&&maximumAgeMs>0;}
};
enum class MeleeStop {None,InputUnavailable,Tracking,NativeUnavailable,Stale,OwnerChanged,EpochChanged,InvalidAction,UnrelatedAction,AckTimeout,NativeRejected,Gesture,Grip};
struct MeleeCommand {
    MeleeStop reason=MeleeStop::None;
    uint64_t issued=0,accepted=0,retired=0,current=0;uint8_t level=0;
    bool suspended=true,revoked=false;Intent intent{};double remaining=0,total=0;
};
// Unlimited melee intent/ack/recovery coordinator. It neither submits damage
// nor writes native action/actor state. The transport and scoped contact ticket
// remain external. Start permission is conservative: native idle + CanPrimary.
class MeleeSession {
    IntentGate gate_;MeleeDelivery delivery_;MeleeNative owner_{};
    uint64_t request_=0,at_=0,lastNow_=0,frame_=0,session_=0,origin_=0;
    uint32_t sequence_=0;int64_t triggerTime_=0,priorTrigger_=0;
    bool acknowledged_=false;uint64_t completed_=0;
    void clearGesture(){(void)gate_.update({}, {}, {});frame_=session_=origin_=0;}
    MeleeCommand cancel(MeleeStop reason){MeleeCommand out;out.reason=reason;out.intent.blocked=reason==MeleeStop::Grip?Block::Grip:reason==MeleeStop::Tracking?Block::Tracking:Block::Disabled;out.retired=request_;out.revoked=request_!=0;request_=0;acknowledged_=false;clearGesture();return out;}
    bool sameOwner(const MeleeNative& n)const{return owner_.worlds==n.worlds&&owner_.world==n.world&&owner_.worldSerial==n.worldSerial&&owner_.entity==n.entity&&owner_.item==n.item&&owner_.binding==n.binding;}
public:
    MeleeSession(GestureConfig gesture,MeleeDelivery delivery):gate_(gesture),delivery_(delivery){if(!delivery.valid())throw std::invalid_argument("explicit native delivery limits required");}
    uint64_t completed()const{return completed_;}
    // Value-only cancellation; no fresh hand/native sample is required at exit.
    MeleeCommand suspend(MeleeStop reason){return cancel(reason);}
    MeleeCommand update(uint64_t now,const HandFrame& f,uint64_t receipt,const Weapon& w,const MeleeNative& n,const MeleePermission& permission){
        const bool rollback=lastNow_&&now<lastNow_;lastNow_=now;
        bool supported=f.focused&&w.kind==WeaponKind::Melee&&w.mainHand<2&&f.hands[w.mainHand].tracked();
        supported=supported&&f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0;
        try{for(unsigned h=0;h<2;h++)if(h==w.mainHand||w.twoHanded){supported=supported&&f.hands[h].tracked()&&f.hands[h].profile&&finite(f.hands[h].grip.position);(void)normalized(f.hands[h].grip.orientation);}}catch(...){supported=false;}
        if(!permission.enabled||!permission.ownsInput||permission.reserved)return cancel(MeleeStop::InputUnavailable);
        if(!supported)return cancel(MeleeStop::Tracking);
        if(!f.hands[w.mainHand].grabActive||!f.hands[w.mainHand].grab||(w.twoHanded&&!twoHandSupportValid(f,w)))return cancel(MeleeStop::Grip);
        if((!n.worlds.empty()&&(!n.worlds.valid()||n.worlds.input.world!=n.world||n.worlds.input.serial!=n.worldSerial))||!n.known||!n.local||!n.alive||!n.gameplay||!n.world||!n.entity||!n.item||!n.binding||w.binding!=n.binding)return cancel(MeleeStop::NativeUnavailable);
        if(rollback||!receipt||now<receipt||now-receipt>delivery_.maximumAgeMs||!n.milliseconds||now<n.milliseconds||now-n.milliseconds>delivery_.maximumAgeMs)return cancel(MeleeStop::Stale);
        if(owner_.world&&!sameOwner(n)){const bool epoch=!(owner_.worlds==n.worlds)||(owner_.world==n.world&&owner_.worldSerial!=n.worldSerial);owner_=n;return cancel(epoch?MeleeStop::EpochChanged:MeleeStop::OwnerChanged);}owner_=n;
        if(request_&&(f.sessionGeneration!=session_||f.originGeneration!=origin_))return cancel(MeleeStop::EpochChanged);
        MeleeCommand out;out.suspended=false;
        if(n.active&&(n.length<=0||n.playedTime<0||n.currentTime<n.playedTime||n.currentTime>n.length))return cancel(MeleeStop::InvalidAction);
        if(request_){
            if(n.active){
                if(n.trigger!=1||n.usedItem!=n.item||!n.sequence||n.triggerTime==priorTrigger_)return cancel(MeleeStop::UnrelatedAction);
                if(!acknowledged_){
                    if(now-at_>delivery_.acknowledgementMs)return cancel(MeleeStop::AckTimeout);
                    if(!gate_.acknowledge(request_))return cancel(MeleeStop::Gesture);
                    acknowledged_=true;sequence_=n.sequence;triggerTime_=n.triggerTime;out.accepted=request_;
                }
                if(sequence_!=n.sequence||triggerTime_!=n.triggerTime)return cancel(MeleeStop::UnrelatedAction);
                out.remaining=double(n.length-n.currentTime);out.total=double(n.length);
            }else if(acknowledged_){
                out.retired=request_;request_=0;acknowledged_=false;++completed_;clearGesture();
                // Do not arm from a cached hand sample when the native action
                // ends. A later neutral sample and a new physical swing follow.
                return out;
            }else if(now-at_>delivery_.acknowledgementMs)return cancel(MeleeStop::AckTimeout);
            else if(!n.canPrimary)return cancel(MeleeStop::NativeRejected);
        }
        if(f.frame!=frame_||f.sessionGeneration!=session_||f.originGeneration!=origin_){
            NativeCombat authority;authority.enabled=authority.known=authority.local=authority.alive=true;authority.uiOrEvent=false;
            authority.canStart=!request_&&!n.active&&n.canPrimary;authority.recoveryRemaining=out.remaining;authority.recoveryTotal=out.total;
            out.intent=gate_.update(f,w,authority);frame_=f.frame;session_=f.sessionGeneration;origin_=f.originGeneration;
            if(out.intent.cancelledRequest||(request_&&(out.intent.blocked==Block::Discontinuity||out.intent.blocked==Block::Invalid||out.intent.blocked==Block::Tracking||out.intent.blocked==Block::Grip)))return cancel(MeleeStop::Gesture);
            if(out.intent.request){
                if(request_||n.active||!n.canPrimary)return cancel(MeleeStop::NativeRejected);
                request_=out.issued=out.intent.request;at_=now;priorTrigger_=n.triggerTime;acknowledged_=false;
            }
        }
        out.current=request_;out.level=request_&&now-at_<delivery_.pulseMs?255:0;return out;
    }
};
} // namespace evr::motion
