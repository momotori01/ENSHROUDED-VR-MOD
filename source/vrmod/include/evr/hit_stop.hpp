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
        std::array<Pose,6> olderGrip{},olderAim{};unsigned olderCount=0;   // the six shown before it (a stop that rewinds)
        double pendingSeconds=0;bool pending=false;unsigned pendingRewind=0;
        struct Past {uint64_t frame=0;Pose grip=identityPose(),aim=identityPose();};
        std::array<Past,10> past{};unsigned pastCount=0;         // the shown poses of the last frames by number, newest first
        double pendingAt=-1;                                      // a stop at a moment shown (frame and fraction), or -1
    };
    std::array<Hand,2> hands_{};
    static Quat towardIdentity(Quat q,double keep){
        if(q.w<0)q={-q.x,-q.y,-q.z,-q.w};
        const float k=float(keep);Quat r{q.x*k,q.y*k,q.z*k,q.w*k+(1-k)};
        return normalized(r);
    }
    static double angle(Quat q){const double w=std::min(1.0,std::abs(double(q.w)));return 2*std::acos(w);}
    static Pose blend(const Pose& a,const Pose& b,float t){
        Quat q=b.orientation;const auto& p=a.orientation;if(p.x*q.x+p.y*q.y+p.z*q.z+p.w*q.w<0)q={-q.x,-q.y,-q.z,-q.w};
        Pose r;r.position=a.position+(b.position-a.position)*t;r.orientation=normalized(Quat{p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,p.z+(q.z-p.z)*t,p.w+(q.w-p.w)*t});return r;}
    // The shown pose at a moment (a frame number and a fraction): between the two frames about it; before the oldest kept
    // the oldest; after the newest, on from the two newest as they went (up to 1.5 frames past the newest; one kept: it).
    static void atMoment(const Hand& s,double at,Pose& grip,Pose& aim){
        if(s.pastCount>=2&&at>double(s.past[0].frame)&&s.past[0].frame>s.past[1].frame){
            const auto& a=s.past[1];const auto& b=s.past[0];const double to=std::min(at,double(b.frame)+1.5);
            const float u=float((to-double(a.frame))/double(b.frame-a.frame));grip=blend(a.grip,b.grip,u);aim=blend(a.aim,b.aim,u);return;}
        const Hand::Past* newer=nullptr;const Hand::Past* older=nullptr;
        for(unsigned i=0;i<s.pastCount;++i){const auto& p=s.past[i];if(double(p.frame)<=at){older=&p;break;}newer=&p;}
        if(!older){const auto& o=s.past[s.pastCount-1];grip=o.grip;aim=o.aim;return;}
        if(!newer||double(older->frame)==at){grip=older->grip;aim=older->aim;return;}
        const float t=float((at-double(older->frame))/double(newer->frame-older->frame));
        grip=blend(older->grip,newer->grip,t);aim=blend(older->aim,newer->aim,t);
    }
public:
    HitStopSettings settings{};
    unsigned stops=0;
    // Stop these hands (bit 0 left, bit 1 right) for `seconds`, from the next frame on, at the pose they were last shown - or,
    // with rewind 1..6, at the pose shown that many frames before it (2026-10-30: the strike's ray answers a frame late, and a
    // fast tool's head had passed the surface by up to 0.37 m - headset run 20261030-020637; and the stop may wait for the
    // stroke's press, up to 0.1 s after the touch - controller_ui_fixture.hpp) - or, with `atFrame` (a hand frame's number and
    // a fraction, not below 0), at the pose shown at that moment, between the two frames about it (2026-10-30, user: "the axe
    // stops well before the tree", headset run 20261030-023058: the game's frames came 26..50 ms apart, the head going
    // 0.2..0.4 m in one - whole frames are too coarse). The weapon's blade is reckoned from the shown hands
    // (motion_visual_bridge.hpp), so their poses by frame number are the ones the blade's moment is found in.
    void stop(unsigned handMask,double seconds,unsigned rewind=0,double atFrame=-1){
        if(!(seconds>0)||!std::isfinite(seconds))return;
        for(unsigned h=0;h<2;++h)if(handMask&(1u<<h)){hands_[h].pending=true;hands_[h].pendingSeconds=std::min(1.0,seconds);hands_[h].pendingRewind=std::min(rewind,6u);
            hands_[h].pendingAt=std::isfinite(atFrame)&&atFrame>=0?atFrame:-1;}
    }
    bool active(unsigned h)const{return h<2&&(hands_[h].stopped||hands_[h].chasing);}
    void reset(){hands_={};}
    // `now` in seconds (any steady clock). The frame's shown hands in, the hands as stopped or chasing out.
    HandFrame apply(HandFrame f,double now){
        for(unsigned h=0;h<2;++h){
            auto& s=hands_[h];auto& hand=f.hands[h];
            if(!f.focused||!hand.tracked()){s=Hand{};continue;}   // (the older poses go too)
            if(s.pending&&s.shownValid){
                s.pending=false;s.stopped=true;s.chasing=false;s.until=now+s.pendingSeconds;
                if(s.pendingAt>=0&&s.pastCount)atMoment(s,s.pendingAt,s.grip,s.aim);
                else{const unsigned k=std::min(s.pendingRewind,s.olderCount);
                    s.grip=k?s.olderGrip[k-1]:s.shownGrip;s.aim=k?s.olderAim[k-1]:s.shownAim;}
                ++stops;
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
            if(s.shownValid){for(size_t i=s.olderGrip.size()-1;i>0;--i){s.olderGrip[i]=s.olderGrip[i-1];s.olderAim[i]=s.olderAim[i-1];}
                s.olderGrip[0]=s.shownGrip;s.olderAim[0]=s.shownAim;s.olderCount=std::min<unsigned>(s.olderCount+1,unsigned(s.olderGrip.size()));}
            s.shownGrip=hand.grip;s.shownAim=hand.aim;s.shownValid=true;
            {const Hand::Past shown{f.frame,hand.grip,hand.aim};
             if(s.pastCount&&s.past[0].frame==f.frame)s.past[0]=shown;
             else{for(size_t i=s.past.size()-1;i>0;--i)s.past[i]=s.past[i-1];s.past[0]=shown;s.pastCount=std::min<unsigned>(s.pastCount+1,unsigned(s.past.size()));}}
        }
        return f;
    }
};
}
