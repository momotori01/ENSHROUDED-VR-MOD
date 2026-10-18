#pragma once
#include <evr/math.hpp>
#include <evr/motion_actions.hpp>
#include <array>
#include <cmath>
#include <cstdint>
namespace evr::motion {
// Sneaking by crouching, and the view reset by a pose (user, 2026-10-18 morning: "sneak when the headset comes down
// 30 % from the standing height", "the view reset as VHVR has it: the palms looked at for 3 seconds"; also by the
// headset's own reset, which hooks/view_resource_probe.cpp takes from the runtime).
//
// Crouching (VHVR locomotion_input.md 3.6, VRPlayer.cs:2362-2387): the eye height under "the standing eye height x
// 0.7" is crouched, and it stays so until it is over that threshold x 1.05 (73.5 % of the standing height: a little
// higher, so that a head held near 70 % does not switch on and off). The standing eye height is the game's eye
// height of the first-person camera (rootCamera::calibratedHeight, 1.65 m); the head's height is that plus how far
// the headset is above or below where it was at the last reset (the view's origin).
struct RoomCrouchSettings {float reference=1.65f,enter=.7f,leave=1.05f;};
class RoomCrouch {
    bool crouched_=false;
public:
    bool crouched()const{return crouched_;}
    void reset(){crouched_=false;}
    // drop: the headset's height now minus at the last reset, metres (negative when lower).
    bool update(float drop,const RoomCrouchSettings& s={}){
        if(!std::isfinite(drop)||!(s.reference>0))return crouched_;
        const float ratio=(s.reference+drop)/s.reference;
        if(!crouched_&&ratio<s.enter)crouched_=true;
        else if(crouched_&&ratio>s.enter*s.leave)crouched_=false;
        return crouched_;
    }
};
// The game's sneak is a key that switches it on and off (the player's input settings can make it a held key instead;
// both are handled). When to press it (VHVR ControlPatches.cs:742-761):
//   crouched and the character does not sneak: press (crouching comes first);
//   not crouched, the character sneaks, and that sneak was the crouch's: press, to stand up;
//   a sneak the player chose with the stick stays while the body stands.
// A press waits for the character's state to answer the one before (pressGap) before another is sent.
struct SneakPressResult {bool hold=false;bool pressed=false;bool entering=false;};
class SneakPresser {
    bool bodySneak_=false;uint64_t lastPress_=0;bool pressedBefore_=false;
public:
    uint64_t pressGap=600,pressLength=100;
    bool bodySneak()const{return bodySneak_;}
    void reset(){bodySneak_=false;lastPress_=0;pressedBefore_=false;}
    // allowed: gameplay, the character on its feet (not swimming, sitting, climbing ...); sneaking: the actor's
    // InSneakMode; toggle: the player's settings switch sneak with each press.
    SneakPressResult update(bool crouched,bool sneaking,bool toggle,bool allowed,uint64_t now){
        SneakPressResult out;
        if(!allowed){pressedBefore_=false;return out;}
        if(!toggle){   // held: the key down while crouched
            out.hold=crouched;out.pressed=crouched&&!pressedBefore_;out.entering=out.pressed;pressedBefore_=crouched;bodySneak_=crouched;return out;}
        pressedBefore_=false;
        if(lastPress_&&now>=lastPress_&&now-lastPress_<pressLength){out.hold=true;return out;}
        if(!sneaking&&!crouched)bodySneak_=false;
        const bool ready=!lastPress_||now<lastPress_||now-lastPress_>=pressGap;
        if(!ready)return out;
        if(crouched&&!sneaking){bodySneak_=true;lastPress_=now;out.hold=out.pressed=out.entering=true;}
        else if(!crouched&&sneaking&&bodySneak_){lastPress_=now;out.hold=out.pressed=true;}
        return out;
    }
};
// The view reset pose (VHVR locomotion_input.md 2.9, VRControls.cs:15-21, 405-438): both hands in front of the face -
// the left one within 0.2 m of (-0.1, 0, 0.1 ahead) from the headset, the right one of (0.1, 0, 0.1 ahead) - for 3
// seconds, no trigger and no grip pressed meanwhile. Once; the hands have to leave the pose before it counts again.
// The points are in the headset's own space (OpenXR: -Z ahead).
struct RecenterPoseSettings {float side=.1f,ahead=.1f,radius=.2f;uint64_t hold=3000;float trigger=.5f;};
class RecenterPose {
    uint64_t since_=0;bool fired_=false;
public:
    RecenterPoseSettings settings{};
    // How long the pose has been held (0 when not held).
    uint64_t held(uint64_t now)const{return since_&&now>=since_?now-since_:0;}
    bool inPose(const HandFrame& f,const Pose& head)const{
        if(!f.focused)return false;
        const Quat toHead=inverse(head.orientation);
        for(unsigned h=0;h<2;++h){const auto& hand=f.hands[h];
            if(!hand.tracked())return false;
            if(hand.triggerActive&&hand.trigger>=settings.trigger)return false;
            if(hand.grabActive&&hand.grab)return false;
            const Vec3 local=rotate(toHead,hand.grip.position-head.position);
            const Vec3 target{h?settings.side:-settings.side,0,-settings.ahead};
            if(!(length(local-target)<=settings.radius))return false;
        }
        return true;
    }
    // True once, when the pose has been held for settings.hold. allowed false (no tracking, the head gesture) ends it.
    bool update(const HandFrame& f,const Pose& head,bool allowed,uint64_t now){
        if(!allowed||!inPose(f,head)){since_=0;fired_=false;return false;}
        if(!since_){since_=now?now:1;return false;}
        if(fired_||now<since_||now-since_<settings.hold)return false;
        fired_=true;return true;
    }
};
}
