#pragma once
#include <evr/guard_intent.hpp>
namespace evr::motion {
// Value-only routing of the two grip buttons (2026-10-04 user plan):
//   right grip in the right-shoulder zone -> main weapon, right-hip zone -> staff/wand,
//   left grip in the left-shoulder zone -> bow, left-hip zone -> shield,
//   right grip held outside every zone for the long-press time -> quick wheel (RB level),
//   left grip outside the zones -> two-hand support grip (level).
// 2026-10-05 18:50 user spec: shoulder zones may overlap the temple gesture regions.
// Equipping uses the grip and the gesture uses the stick, so an active gesture no
// longer blocks grip presses. Shoulders may use a wide head-relative "reach over the
// shoulder" region (VHVR Pose.getBackReachLocation used as a design reference only).
// A press is classified once, at its press edge, and owns the button until release:
// a zone press never becomes wheel/support, and a support/wheel press never becomes
// an equip command by drifting into a zone. Equip commands are one-shot edges; the
// equipping hand then needs a calm period before its swings may start attacks.
// No native writes, slot lookup or default body dimensions live here: zone offsets
// are explicit per-user calibration relative to the calibrated navel/body yaw.
enum class GripZone {None,RightShoulder,LeftShoulder,RightHip,LeftHip};
enum class EquipRequest {None,MainWeapon,Bow,Staff,Shield};
// Gesture is retained for stable log numbering; the router no longer produces it.
enum class GripBlock {None,Disabled,NativeUnknown,UiOrEvent,Gesture,Tracking,Invalid,Discontinuity};
struct GripZoneOffset {float right=0,up=0,forward=0;}; // metres from navel in body axes
// Head-relative same-side shoulder region in body axes (metres). Enabled when lateral>0;
// then the shoulder spheres are unused. A point counts when it is between `below` under
// and `above` over the HMD, on the hand's own side up to `lateral` out, and no further
// forward than `forward` (0 = the HMD's coronal plane: beside or behind the head).
// keepMargin widens every bound for the "still inside" report of a press already made.
struct GripShoulderReach {
    float below=0,above=0,lateral=0,forward=0,keepMargin=0;
    bool enabled()const{return lateral>0;}
};
struct GripRouterCalibration {
    GripZoneOffset rightShoulder,leftShoulder,rightHip,leftHip;
    GripShoulderReach shoulderReach;
    float enterRadius=0,exitRadius=0;          // exit > enter (hysteresis for "still inside")
    float calmSpeed=0;XrDuration calmTime=0;   // equip -> attack re-arm
    XrDuration longPress=0,maximumGap=0;
    bool valid()const{
        const float v[]{rightShoulder.right,rightShoulder.up,rightShoulder.forward,leftShoulder.right,leftShoulder.up,leftShoulder.forward,
            rightHip.right,rightHip.up,rightHip.forward,leftHip.right,leftHip.up,leftHip.forward,enterRadius,exitRadius,calmSpeed,
            shoulderReach.below,shoulderReach.above,shoulderReach.lateral,shoulderReach.forward,shoulderReach.keepMargin};
        for(float x:v)if(!std::isfinite(x))return false;
        const auto& r=shoulderReach;
        const bool shoulders=r.enabled()?(r.below>0&&r.below<=1&&r.above>=0&&r.above<=.5f&&r.lateral<=1&&r.forward>=-.5f&&r.forward<=.3f&&r.keepMargin>=0&&r.keepMargin<=.2f):
            (r.lateral==0&&rightShoulder.right>0&&leftShoulder.right<0&&rightShoulder.up>rightHip.up&&leftShoulder.up>leftHip.up);
        return enterRadius>0&&exitRadius>enterRadius&&calmSpeed>0&&calmTime>0&&longPress>0&&maximumGap>0&&
            rightHip.right>0&&leftHip.right<0&&shoulders;
    }
};
struct GripAuthority {
    bool enabled=false,known=false,local=false,alive=false,uiOrEvent=true;
    bool gestureActive=false; // temple gesture active: reported only, never blocks grips
    bool wheelOverlay=false;  // native wheel visible: only the wheel press may continue
};
struct GripHandRoute {GripZone pressZone=GripZone::None,zone=GripZone::None;bool held=false,consumed=false;};
struct GripRoute {
    uint64_t frame=0;GripBlock blocked=GripBlock::Disabled;bool valid=false;
    EquipRequest equip=EquipRequest::None;unsigned equipHand=2; // one-shot edge
    bool wheelHeld=false,supportHeld=false;                     // levels
    bool gestureOverlap=false;                                   // grips routed while a temple gesture is active
    std::array<bool,2> attackRearmRequired{};                    // after equip, until calm
    std::array<GripHandRoute,2> hands{};
};
inline GripZoneOffset gripZoneOffset(const GripRouterCalibration& c,GripZone z){
    switch(z){case GripZone::RightShoulder:return c.rightShoulder;case GripZone::LeftShoulder:return c.leftShoulder;
        case GripZone::RightHip:return c.rightHip;case GripZone::LeftHip:return c.leftHip;default:return {};}
}
// Only same-side zones exist for each hand; crossing the body never equips.
inline EquipRequest gripZoneRequest(unsigned hand,GripZone z){
    if(hand==1&&z==GripZone::RightShoulder)return EquipRequest::MainWeapon;
    if(hand==1&&z==GripZone::RightHip)return EquipRequest::Staff;
    if(hand==0&&z==GripZone::LeftShoulder)return EquipRequest::Bow;
    if(hand==0&&z==GripZone::LeftHip)return EquipRequest::Shield;
    return EquipRequest::None;
}
class GripRouter {
    GripRouterCalibration c;HandFrame previous{};GuardBody previousBody{};bool havePrevious=false;
    struct HandState {bool held=false,mustRelease=true;GripZone pressZone=GripZone::None;XrTime pressedAt=0;bool wheel=false,rearm=false;XrTime calmSince=0;};
    std::array<HandState,2> s{};
    void reset(){havePrevious=false;for(auto& h:s){h.held=h.wheel=false;h.pressZone=GripZone::None;h.pressedAt=0;h.mustRelease=true;}}
    GripZone classify(const GuardBody& b,Vec3 p,unsigned hand,GripZone keep)const{
        const Vec3 right=cross(b.forward,Vec3{0,1,0}),d=p-b.navel;
        const float lr=dot(d,right),up=d.y,fw=dot(d,b.forward);
        GripZone best=GripZone::None;float bestDistance=0;
        for(auto z:{GripZone::RightShoulder,GripZone::LeftShoulder,GripZone::RightHip,GripZone::LeftHip}){
            if(gripZoneRequest(hand,z)==EquipRequest::None)continue;
            if(c.shoulderReach.enabled()&&(z==GripZone::RightShoulder||z==GripZone::LeftShoulder)){
                // Inside the shoulder region counts as distance 0, so it wins over a hip sphere.
                const auto& r=c.shoulderReach;const float m=z==keep?r.keepMargin:0.f,side=z==GripZone::RightShoulder?1.f:-1.f;
                const Vec3 h=p-b.head;const float hl=dot(h,right)*side,hu=h.y,hf=dot(h,b.forward);
                if(hu>=-(r.below+m)&&hu<=r.above+m&&hl>-m&&hl<=r.lateral+m&&hf<=r.forward+m){best=z;bestDistance=0;}
                continue;
            }
            const auto o=gripZoneOffset(c,z);const float dx=lr-o.right,dy=up-o.up,dz=fw-o.forward;
            const float distance=std::sqrt(dx*dx+dy*dy+dz*dz),limit=z==keep?c.exitRadius:c.enterRadius;
            if(distance<=limit&&(best==GripZone::None||distance<bestDistance)){best=z;bestDistance=distance;}
        }
        return best;
    }
public:
    explicit GripRouter(GripRouterCalibration calibration):c(calibration){if(!c.valid())throw std::invalid_argument("grip zone calibration required");}
    GripRoute update(const HandFrame& f,const GuardBody& body,const GripAuthority& n){
        GripRoute o;o.frame=f.frame;
        auto stop=[&](GripBlock b){o.blocked=b;reset();for(unsigned h=0;h<2;h++)o.attackRearmRequired[h]=s[h].rearm;return o;};
        if(!n.enabled)return stop(GripBlock::Disabled);
        if(!n.known||!n.local||!n.alive)return stop(GripBlock::NativeUnknown);
        if(!body.known||!body.calibration||body.frame!=f.frame||body.session!=f.sessionGeneration||body.origin!=f.originGeneration||body.time!=f.predictedTime||
           !f.frame||!f.sessionGeneration||!f.originGeneration||f.predictedTime<=0||!finite(body.navel)||!finite(body.forward)||!finite(body.head)||
           std::abs(body.forward.y)>1e-5f||std::abs(length(body.forward)-1.f)>1e-4f)return stop(GripBlock::Invalid);
        if(!f.focused)return stop(GripBlock::Tracking);
        for(unsigned h=0;h<2;h++)if(!f.hands[h].tracked()||!f.hands[h].grabActive||!f.hands[h].profile||!finite(f.hands[h].grip.position))return stop(GripBlock::Tracking);
        float dt=0;
        if(havePrevious){
            bool continuous=f.sessionGeneration==previous.sessionGeneration&&f.originGeneration==previous.originGeneration&&f.frame>previous.frame&&
                f.predictedTime>previous.predictedTime&&f.predictedTime-previous.predictedTime<=c.maximumGap&&body.calibration==previousBody.calibration;
            for(unsigned h=0;h<2;h++)continuous=continuous&&f.hands[h].profile==previous.hands[h].profile;
            if(!continuous)return stop(GripBlock::Discontinuity);
            dt=float(f.predictedTime-previous.predictedTime)*1e-9f;
        }
        // UI and events keep ownership: nothing new starts, held presses are cancelled
        // and must be released before any later classification. A temple gesture does
        // not: it owns the right stick only (2026-10-05 18:50 user spec).
        const bool blockedHere=n.uiOrEvent;
        for(unsigned h=0;h<2;h++){
            auto& st=s[h];const auto& hand=f.hands[h];
            if(st.rearm){
                const float speed=havePrevious?length(hand.grip.position-previous.hands[h].grip.position)/dt:0;
                if(havePrevious&&std::isfinite(speed)&&speed<=c.calmSpeed){if(!st.calmSince)st.calmSince=f.predictedTime;if(f.predictedTime-st.calmSince>=c.calmTime)st.rearm=false;}
                else st.calmSince=0;
            }
            if(!hand.grab){st.held=st.wheel=false;st.pressZone=GripZone::None;st.pressedAt=0;st.mustRelease=false;continue;}
            if(st.mustRelease||blockedHere){st.held=st.wheel=false;st.pressZone=GripZone::None;st.mustRelease=true;continue;}
            if(!st.held){ // press edge: classify once
                st.held=true;st.pressedAt=f.predictedTime;st.pressZone=n.wheelOverlay?GripZone::None:classify(body,hand.grip.position,h,GripZone::None);
                if(st.pressZone!=GripZone::None){o.equip=gripZoneRequest(h,st.pressZone);o.equipHand=h;st.rearm=true;st.calmSince=0;}
            }
            if(h==1&&st.pressZone==GripZone::None&&f.predictedTime-st.pressedAt>=c.longPress)st.wheel=true;
        }
        havePrevious=true;previous=f;previousBody=body;
        o.valid=true;o.blocked=blockedHere?GripBlock::UiOrEvent:GripBlock::None;o.gestureOverlap=n.gestureActive;
        for(unsigned h=0;h<2;h++){
            auto& r=o.hands[h];r.held=s[h].held;r.pressZone=s[h].pressZone;r.consumed=s[h].pressZone!=GripZone::None;
            r.zone=classify(body,f.hands[h].grip.position,h,s[h].pressZone);o.attackRearmRequired[h]=s[h].rearm;
        }
        o.wheelHeld=s[1].wheel&&s[1].held;
        // While the native wheel is open the left hand keeps no support/equip role.
        o.supportHeld=s[0].held&&s[0].pressZone==GripZone::None&&!n.wheelOverlay;
        if(o.equip!=EquipRequest::None&&o.equipHand==1&&o.wheelHeld)throw std::logic_error("zone press cannot also hold the wheel");
        return o;
    }
};
} // namespace evr::motion
