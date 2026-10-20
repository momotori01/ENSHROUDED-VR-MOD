#pragma once
#include <evr/math.hpp>
#include <evr/motion_actions.hpp>
#include <algorithm>
#include <array>
#include <cmath>
namespace evr::motion {
// The hit stop (2026-10-20, user: "give the ordinary melee attack a hit stop: when it hits, with the hit effect - one-hand
// sword 0.1 s, two-hand 0.2 s, tools such as the pickaxe and the felling axe 0.3 s; after the stop the hand chases to
// where it is now; hitting several, it stops each time").
// Only what is shown stops - the hand, the held item and the body read the shown hands (pose_smoothing.hpp). Strokes,
// the hit volume, aim and gestures keep the hands as tracked, so the game's own hit and the next stroke are not delayed.
// A stopped hand keeps the pose it was shown at (its grip and aim, in the tracking space - the body moves on with it).
// When the stop ends, the difference to the tracked hand (in the hand's own frame) shrinks by exp(-t / chaseSeconds):
// about 95 % in three times that. Another hit while a hand is stopped or chasing stops it again where it is shown.
struct HitStopSettings {
    double chaseSeconds=.04;          // time constant of the chase after the stop
    double doneMetres=.002,doneRadians=.01;   // close enough: the hand is shown as tracked again
};
// The stop of a weapon class (seconds): a tool (no target gate - pickaxe, felling axe and the like) 0.3, a two-hand
// weapon 0.25, a one-hand weapon (and a fist) 0.15 (2026-10-20 18:10, user after trying 0.1 / 0.2: "one-hand 0.15,
// two-hand 0.25 - decided").
inline double hitStopSeconds(bool tool,bool twoHanded){return tool?.3:twoHanded?.25:.15;}
class HitStop {
    struct Hand {
        bool stopped=false,chasing=false,shownValid=false;double until=0,last=0;
        Pose grip=identityPose(),aim=identityPose();     // the stopped pose (shown)
        Pose offset=identityPose();                      // chasing: shown grip = tracked grip ∘ offset
        Pose aimLocal=identityPose();                    // chasing: aim in the stopped grip's frame
        Pose shownGrip=identityPose(),shownAim=identityPose();   // the last shown, where a stop begins
        double pendingSeconds=0;bool pending=false;
    };
    std::array<Hand,2> hands_{};
    static Quat towardIdentity(Quat q,double keep){
        if(q.w<0)q={-q.x,-q.y,-q.z,-q.w};
        const float k=float(keep);Quat r{q.x*k,q.y*k,q.z*k,q.w*k+(1-k)};
        return normalized(r);
    }
    static double angle(Quat q){const double w=std::min(1.0,std::abs(double(q.w)));return 2*std::acos(w);}
public:
    HitStopSettings settings{};
    unsigned stops=0;
    // Stop these hands (bit 0 left, bit 1 right) for `seconds`, from the next frame on, at the pose they were last shown.
    void stop(unsigned handMask,double seconds){
        if(!(seconds>0)||!std::isfinite(seconds))return;
        for(unsigned h=0;h<2;++h)if(handMask&(1u<<h)){hands_[h].pending=true;hands_[h].pendingSeconds=std::min(1.0,seconds);}
    }
    bool active(unsigned h)const{return h<2&&(hands_[h].stopped||hands_[h].chasing);}
    void reset(){hands_={};}
    // `now` in seconds (any steady clock). The frame's shown hands in, the hands as stopped or chasing out.
    HandFrame apply(HandFrame f,double now){
        for(unsigned h=0;h<2;++h){
            auto& s=hands_[h];auto& hand=f.hands[h];
            if(!f.focused||!hand.tracked()){s=Hand{};continue;}
            if(s.pending&&s.shownValid){
                s.pending=false;s.stopped=true;s.chasing=false;s.until=now+s.pendingSeconds;
                s.grip=s.shownGrip;s.aim=s.shownAim;++stops;
            }else if(s.pending)s.pending=false;   // nothing shown yet: no pose to stop at
            if(s.stopped&&now>=s.until){
                // The chase begins from the stopped pose: its difference to the tracked hand, in the hand's own frame.
                s.stopped=false;s.chasing=true;s.offset=compose(inverse(hand.grip),s.grip);s.aimLocal=compose(inverse(s.grip),s.aim);s.last=now;
            }
            if(s.stopped){hand.grip=s.grip;hand.aim=s.aim;}
            else if(s.chasing){
                const double dt=std::clamp(now-s.last,0.0,.1);s.last=now;
                const double keep=std::exp(-dt/std::max(1e-3,settings.chaseSeconds));
                s.offset.position=s.offset.position*float(keep);s.offset.orientation=towardIdentity(s.offset.orientation,keep);
                if(length(s.offset.position)<=settings.doneMetres&&angle(s.offset.orientation)<=settings.doneRadians){s.chasing=false;}
                else{const Pose grip=compose(hand.grip,s.offset);hand.aim=compose(grip,s.aimLocal);hand.grip=grip;}
            }
            s.shownGrip=hand.grip;s.shownAim=hand.aim;s.shownValid=true;
        }
        return f;
    }
};
}
