#pragma once
#include <evr/math.hpp>
#include <evr/motion_actions.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
namespace evr::motion {
// Swimming by the arms (2026-10-20, user: "add swimming by hand - VHVR had it", "switchable in the settings"), as VHVR's
// gestured locomotion (docs/analysis/vhvr_spec_20261010/locomotion_input.md 5.0 and 5.2, GesturedLocomotionManager.cs):
//   - a hand counts while the character swims, the hand is under the water's surface and the hand is not "held back"
//     (VHVR's StopGesturedLocomotion: the thumb on the stick or a button, the trigger pulled, the grip closed; here the
//     trigger, the grip and the face buttons only - see below);
//   - its push: the hand's speed along its palm's direction (left hand: the grip's +X, right hand: -X - OpenXR's grip
//     space, the palm's normal) less 0.5 m/s, toward the opposite of the hand's movement;
//   - both hands' pushes added and laid flat: under 0.375 m/s nothing, else twice that is the wanted velocity;
//   - the velocity goes toward the wanted one by (step / 1 s) each step (VHVR's decay time in the water: a stroke
//     carries on and fades over a few seconds); under 0.0625 m/s it is nothing;
//   - it is given as the left stick along the head's forward and right (the sent camera follows the head while
//     swimming - include/evr/walking_head_camera.hpp swimHead), added to the stick.
// Headset run 20261020-112023 (user: "I cannot swim with my hands"; bug report 112732, its hands replayed through this
// computation - scratchpad swim_replay.py): the strokes were 0.07..0.35 m under the head, a third of the hands' rows
// were above the surface taken 0.2 m under the head, many more were held back by the stick (the thumb on it), and a
// stroke lasted about 80 ms, so the velocity rose to 0.06..0.37 m/s - a tenth of the stick at VHVR's 1 m/s per full
// throw (Valheim's swimming; Enshrouded's is another). Now: the surface 0.05 m under the head (still a guess - the
// game's water level is not read), the thumb and the stick do not hold a hand back (the stick's own push is added to
// the strokes anyway), and 0.25 m/s is the full throw (fitted to that one recording: its strokes then give 0.4..1 of
// the stick for a second or two; EVR_SWIM_STICK_SCALE).
struct SwimStrokeSettings {
    float palmDeadSpeed=.5f,minimumPush=.375f,pushGain=2.f,decaySeconds=1.f,minimumSpeed=.03f,metresPerSecondPerStick=.25f;
    float surfaceBelowHead=.05f;
    float maximumHandSpeed=15.f;   // a tracking jump, not a stroke
    double maximumStep=.1;         // seconds; a longer gap starts over
};
// Up and down (2026-10-20 12:20, user: "can the hands also move me up and down in the water - stroke up, stroke down to
// dive and come up"): the strokes' upward part is kept apart in the same way (the push opposite to the hands: hands
// pushed down lift, hands pulled up sink) as `up` in m/s and `upStick` (the same 0.25 m/s for a full throw); the game's
// own diving inputs are pressed from it (controller_ui_fixture.hpp, include/evr/holster.hpp swim inputs).
struct SwimStrokeResult {float stickX=0,stickY=0,up=0,upStick=0;bool stroking=false,moving=false,strokingUp=false;Vec3 velocity{0,0,0},wanted{0,0,0};std::array<bool,2> counted{};};
class SwimStroke {
    std::array<Vec3,2> last_{};std::array<bool,2> have_{};XrTime lastTime_=0;Vec3 velocity_{0,0,0};float up_=0;
public:
    void reset(){have_={};lastTime_=0;velocity_={0,0,0};up_=0;}
    // A hand held back: the trigger pulled, the grip closed, a face button down (VHVR also the thumb on the stick - not
    // here, see above).
    static bool heldBack(const HandSample& h){
        if(h.triggerActive&&h.trigger>=.5f)return true;
        if(h.grabActive&&h.grab)return true;
        return (h.primaryActive&&h.primaryPressed)||(h.secondaryActive&&h.secondaryPressed);
    }
    // swimming: the character is in the water and the game's play has the input (not a menu). head: the headset in the
    // hands' space. Called once per new frame of the hands.
    SwimStrokeResult update(const HandFrame& hands,const Pose& head,bool swimming,const SwimStrokeSettings& s={}){
        SwimStrokeResult out;
        if(!swimming||!hands.focused||!finite(head.position)){reset();return out;}
        const double dt=lastTime_&&hands.predictedTime>lastTime_?double(hands.predictedTime-lastTime_)*1e-9:0.;
        const bool stepOk=dt>0&&dt<=s.maximumStep;lastTime_=hands.predictedTime;
        Vec3 push{0,0,0};const float surface=head.position.y-s.surfaceBelowHead;
        for(unsigned i=0;i<2;++i){const auto& h=hands.hands[i];
            if(!h.tracked()||!finite(h.grip.position)){have_[i]=false;continue;}
            const auto at=h.grip.position;const bool had=have_[i];const auto before=last_[i];last_[i]=at;have_[i]=true;
            if(!had||!stepOk)continue;
            const auto v=(at-before)*float(1./dt);const float speed=length(v);
            if(!(speed<=s.maximumHandSpeed)||speed<1e-4f)continue;
            if(at.y>=surface||heldBack(h))continue;
            out.counted[i]=true;
            const auto palm=rotate(h.grip.orientation,Vec3{i==0?1.f:-1.f,0,0});
            const float amount=std::max(0.f,dot(v,palm)-s.palmDeadSpeed);
            push=push-v*(amount/speed);
        }
        const float pushUp=push.y;push.y=0;
        const float pushSize=length(push);out.wanted=pushSize>=s.minimumPush?push*s.pushGain:Vec3{0,0,0};
        out.stroking=pushSize>=s.minimumPush;
        out.strokingUp=std::abs(pushUp)>=s.minimumPush;const float wantedUp=out.strokingUp?pushUp*s.pushGain:0.f;
        if(stepOk){const float k=float(std::min(1.,dt/double(s.decaySeconds)));velocity_=velocity_+(out.wanted-velocity_)*k;up_=up_+(wantedUp-up_)*k;}
        out.velocity=velocity_;
        if(std::abs(up_)>=s.minimumSpeed){out.up=up_;out.upStick=std::clamp(up_/s.metresPerSecondPerStick,-1.f,1.f);}
        const float flat=std::hypot(velocity_.x,velocity_.z);
        if(flat<s.minimumSpeed)return out;
        // the head's forward (-Z) and right (+X), laid flat
        auto flatten=[](Vec3 v){v.y=0;const float n=length(v);return n>1e-4f?v*(1/n):Vec3{0,0,0};};
        const auto forward=flatten(rotate(head.orientation,Vec3{0,0,-1})),right=flatten(rotate(head.orientation,Vec3{1,0,0}));
        if(length(forward)<.5f||length(right)<.5f)return out;
        out.moving=true;
        out.stickY=std::clamp(dot(velocity_,forward)/s.metresPerSecondPerStick,-1.f,1.f);
        out.stickX=std::clamp(dot(velocity_,right)/s.metresPerSecondPerStick,-1.f,1.f);
        return out;
    }
};
}
