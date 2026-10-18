#pragma once
#include <evr/motion_actions.hpp>
#include <algorithm>
#include <array>
#include <cmath>
namespace evr::motion {
// A little smoothing for the hands that are shown (user after the headset run game-20261013-141010: "so that the
// hands do not tremble in small steps, I want just a little smoothing").
// The 1-euro filter (Casiez, Roussel, Vogel 2012): a low pass whose cutoff rises with the speed, so a hand held
// still is steadied and a hand that moves is followed almost as it is. At 30 samples a second and the default 4 Hz a
// still hand takes 46 % of each new sample (a tremble comes out at about half its size, a slow drift lags 40 ms);
// at 0.5 m/s the cutoff is 10 Hz, at 3 m/s 40 Hz (89 % of each sample: 1 cm behind).
// Only what is shown takes these poses - hand, held item, body. Strokes, aim and gestures keep the poses as tracked.
struct PoseSmoothingSettings {
    float cutoff=4.f;                 // Hz, position and orientation of a hand at rest; 0 or less: no smoothing
    float positionBeta=12.f;          // Hz per m/s
    float orientationBeta=4.f;        // Hz per rad/s
    float derivativeCutoff=1.f;       // Hz, for the speed itself
    float resetMetres=.5f,resetSeconds=.25f;   // a jump or a gap: start again from the tracked pose
};
class PoseSmoother {
    bool started_=false;Pose value_=identityPose(),raw_=identityPose();float speed_=0,turn_=0;XrTime last_=0;
    static float share(float cutoff,float dt){const float tau=1/(6.2831853f*cutoff);return 1/(1+tau/dt);}
public:
    void reset(){started_=false;}
    Pose update(const Pose& raw,XrTime time,const PoseSmoothingSettings& s){
        const float dt=started_?float(double(time-last_)*1e-9):0.f;
        if(!(s.cutoff>0)||!finite(raw.position)||!started_||!(dt>0)||dt>s.resetSeconds||length(raw.position-value_.position)>s.resetMetres){
            started_=finite(raw.position)&&s.cutoff>0;value_=raw_=raw;speed_=turn_=0;last_=time;return raw;}
        const float keep=share(s.derivativeCutoff,dt);
        speed_+=(length(raw.position-raw_.position)/dt-speed_)*keep;
        float d=std::abs(raw.orientation.x*raw_.orientation.x+raw.orientation.y*raw_.orientation.y+raw.orientation.z*raw_.orientation.z+raw.orientation.w*raw_.orientation.w);
        turn_+=(2*std::acos(std::min(1.f,d))/dt-turn_)*keep;
        const float a=share(s.cutoff+s.positionBeta*speed_,dt),b=share(s.cutoff+s.orientationBeta*turn_,dt);
        value_.position=value_.position+(raw.position-value_.position)*a;
        // The shorter way round, then back to unit length.
        const auto& q=value_.orientation;const auto& r=raw.orientation;
        const float sign=q.x*r.x+q.y*r.y+q.z*r.z+q.w*r.w<0?-1.f:1.f;
        value_.orientation=normalized(Quat{q.x+(r.x*sign-q.x)*b,q.y+(r.y*sign-q.y)*b,q.z+(r.z*sign-q.z)*b,q.w+(r.w*sign-q.w)*b});
        raw_=raw;last_=time;return value_;
    }
};
// A hand whose tracking drops stays where it was shown. VHVR: the hand objects stay where SteamVR last put them and the
// body's IK goes on with them; whether a hand "is active" is asked for the pointers only (VRPlayer.cs:780-786).
// Headset run game-20261013-141010: one controller untracked for one frame (5445) took the hands, the held items and
// the whole body out of both eyes for that frame - the hands and the body are shown only with both hands tracked.
// A pose the runtime still calls valid (estimated from the controller's own sensors) is shown as it comes; without
// one the last shown pose stays. Only while the session has the focus, only for what is shown (strokes, aim and
// gestures keep the hands as tracked), and never before the hand was tracked once since the session and its origin
// last changed.
class HandHold {
    struct Kept {Pose grip=identityPose(),aim=identityPose();bool have=false;};
    std::array<Kept,2> kept_{};uint64_t session_=0,origin_=0;
    static bool valid(bool active,XrSpaceLocationFlags flags,const Pose& p){return active&&(flags&3)==3&&finite(p.position);}
public:
    enum class State:uint8_t{None,Tracked,Estimated,Held};
    std::array<State,2> state{};
    HandFrame update(HandFrame f){
        if(f.sessionGeneration!=session_||f.originGeneration!=origin_){session_=f.sessionGeneration;origin_=f.originGeneration;kept_={};}
        for(unsigned h=0;h<2;++h){auto& hand=f.hands[h];auto& k=kept_[h];
            const bool aimTracked=hand.aimActive&&(hand.aimFlags&15)==15;
            if(hand.tracked()){k.grip=hand.grip;k.aim=aimTracked?hand.aim:hand.grip;k.have=true;state[h]=State::Tracked;continue;}
            if(!f.focused||!k.have){state[h]=State::None;continue;}
            const bool estimated=valid(hand.gripActive,hand.gripFlags,hand.grip);
            if(estimated)k.grip=hand.grip;else hand.grip=k.grip;
            if(aimTracked||valid(hand.aimActive,hand.aimFlags,hand.aim))k.aim=hand.aim;else hand.aim=k.aim;
            hand.gripActive=hand.aimActive=true;hand.gripFlags|=15;hand.aimFlags|=15;
            state[h]=estimated?State::Estimated:State::Held;
        }
        return f;
    }
};
// The hands of a frame as they are shown: grip and aim pose of each tracked hand smoothed, everything else as it is.
class HandSmoothing {
    std::array<PoseSmoother,2> grip_{},aim_{};
public:
    PoseSmoothingSettings settings{};
    HandFrame update(HandFrame f){
        for(unsigned h=0;h<2;++h){auto& hand=f.hands[h];
            if(f.focused&&hand.tracked())hand.grip=grip_[h].update(hand.grip,f.predictedTime,settings);else grip_[h].reset();
            if(f.focused&&hand.aimActive&&(hand.aimFlags&15)==15)hand.aim=aim_[h].update(hand.aim,f.predictedTime,settings);else aim_[h].reset();}
        return f;
    }
};
}
