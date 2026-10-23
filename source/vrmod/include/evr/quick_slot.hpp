#pragma once
#include <evr/live_pad.hpp>
namespace evr::motion {
// Quick slot by hand motion (user decision 2026-10-11, after VHVR's quick menu,
// docs/analysis/vhvr_spec_20261010/ui_hud.md section 5): holding the right B button opens
// the game's own quick wheel; the right hand, moved from where it was when the wheel
// opened, points at an item; letting the button go takes that item. Within the dead
// distance the release changes nothing. A short press stays the game's own Y button.
// The wheel, its items and the equipping are the game's: this only holds the wheel
// button and steers the wheel's stick.
// By the wrist's roll (user, 2026-10-18 after the headset run of 13:30: "select the quick slots not by moving the hand but
// by rolling it, like turning a dial"): the twist of the right controller about its own forward axis since the wheel
// opened, twice over, is the stick's angle - clockwise from the top for a twist to the right (as the user sees it),
// a quarter turn of the wrist half the wheel, 22.5 degrees one of eight slots. Past 12 degrees it points, under 8 back at
// the centre (nothing chosen). `roll` false: the hand's movement (VHVR's way, until 13:50).
// The centre is a fixed angle of the wrist, not where it was when the wheel opened (user after the headset run of 15:00:
// "with the angle at opening as the base, items I cannot reach come up when the hand is already twisted; take the angle of
// the second half of report 2, where I pressed again and again, as the centre"): that report's right grip, B pressed
// fourteen times, rolled 42 to 48 degrees counter-clockwise from level (as the user sees it), 45 on average. So the twist is
// measured from 45 degrees of roll against the horizon about the controller's forward axis (levelRoll). `rollFromOpen`
// true: from the wheel's opening (14:40).
// Holding the interaction (2026-10-20, both players of an online session: "I cannot revive the other"): the game revives
// a fallen player while its interaction is held for reviveDuration, which the tap above never does and a longer hold
// turned into the wheel. With `holdInteract` (the caller saw a fallen player close by) a press of B is the game's Y from the
// moment it goes down until it comes up, and the wheel does not open.
struct QuickSlotSettings {
 bool roll=true;             // the wrist's roll as a dial (false: the hand's movement)
 // 12 and 8 degrees. The stick's angle counts from the edge of the centre (rollRelease), not from the centre itself (2026-10-23,
 // the user: "the dial sticks at the top and jumps - the top two items are hard to choose"): the centre, where nothing is
 // chosen, lay on the top item's direction, so the top was reached only at the edge of the centre and the dial jumped across
 // it. Now just past the centre either way is the top, a turn to the right goes clockwise from it, to the left
 // counter-clockwise; the gain 2.2 keeps the bottom at about a quarter turn of the wrist (8 + 82 degrees).
 float rollEngage=.2094f,rollRelease=.1396f,rollGain=2.2f;
 float rollCentre=.7854f;    // 45 degrees counter-clockwise from level: the wrist's rest on the wheel (nothing chosen)
 bool rollFromOpen=false;    // true: the centre is the wrist's roll when the wheel opened
 // The stick's hold (2026-10-23; the user's report 014216 "the UI and the quick slots blink": while the wheel was used,
 // the wrist at 10..12 degrees turned the pointed slot back and forth within a few frames). The stick keeps its angle until
 // the wrist has turned it this much further, then takes the wrist's angle: a wrist shaking on the border of two items
 // leaves the wheel on one. 2 degrees of the stick, 1 of the wrist (user, 15:00 the same day: "the dial catches with 3 degrees,
 // make it 1"). (Until 14:50 the stick went to the middle of one of eight slots - the wheel has more items, and those
 // between the eight could not be chosen.) 0: no hold.
 float rollHold=.0349f;
 uint64_t holdMs=250;        // "long press": shorter is the ordinary Y tap
 uint64_t tapMs=100;         // length of that Y tap
 float select=.07f;          // VHVR: 7 cm from the centre points at an item
 float release=.05f;         // hysteresis back to the centre
 uint64_t confirmMs=120;     // the stick stays on the item this long after the wheel button is let go
 XrDuration maximumGap=250000000;
};
struct QuickSlotResult {
 uint16_t buttons=0;bool steer=false;float x=0,y=0; // steer: replace the right stick with (x, y)
 bool opened=false,chosen=false,cancelled=false,tapped=false,held=false;   // held: a hold of the interaction began
};
class QuickSlot {
 enum class State {Idle,Pending,Open,Confirm,Tap,Blocked,Hold};
 QuickSlotSettings settings_{};State state_=State::Blocked;
 uint64_t since_=0,session_=0,origin_=0;XrTime time_=0;
 Vec3 centre_{},right_{1,0,0};Quat openRoll_{0,0,0,1};float x_=0,y_=0,twist_=0;bool pointing_=false;
 float stick_=0;bool stickHeld_=false;   // the stick's angle (from the top, clockwise) while pointing
 void idle(State s=State::Idle){state_=s;pointing_=false;x_=y_=0;stickHeld_=false;}
public:
 static constexpr uint16_t wheel=LivePadMapper::RB,tap=LivePadMapper::Y;
 QuickSlot()=default;explicit QuickSlot(QuickSlotSettings s):settings_(s){}
 const QuickSlotSettings& settings()const{return settings_;}
 float twist()const{return twist_;}   // the wrist's roll from the centre (radians, + to the right), for the record
 // The roll of an orientation against the horizon about its own forward axis (-Z), in (-pi, pi]: + counter-clockwise as
 // the user sees it (the right side up) - the Euler roll of the bug report's hand rows (eulerDegrees, third value), but
 // taken from the axes so that the controller's pitch does not enter. Pointing straight up or down there is no horizon: 0.
 static float levelRoll(Quat q){
  const Vec3 f=rotate(q,{0,0,-1}),r=rotate(q,{1,0,0});
  Vec3 right=cross(f,Vec3{0,1,0});const float n=length(right);if(n<1e-3f)return 0;right=right*(1/n);
  const Vec3 up=cross(right,f);
  return std::atan2(dot(r,up),dot(r,right));
 }
 // The roll of `now` against `from` about the controller's own forward axis (-Z), in (-pi, pi]: + clockwise as the user
 // sees it (looking along the controller).
 static float rollBetween(Quat from,Quat now){
  const Quat r=multiply(inverse(from),now);const float along=-r.z;   // the rotation's part about (0, 0, -1)
  float a=2.f*std::atan2(along,r.w);
  while(a>pi)a-=2*pi;while(a<=-pi)a+=2*pi;return a;
 }
 bool open()const{return state_==State::Open;}
 bool busy()const{return state_==State::Pending||state_==State::Open||state_==State::Confirm||state_==State::Hold;}
 bool holding()const{return state_==State::Hold;}
 // allowed: gameplay (or the wheel this gesture opened), fresh hands, no other gesture.
 // head: for the sideways direction; the mapping is frozen when the wheel opens.
 QuickSlotResult update(const HandFrame& f,Pose head,bool allowed,uint64_t now,bool holdInteract=false){
  QuickSlotResult out;const auto& hand=f.hands[1];
  const bool scope=allowed&&f.focused&&f.sessionGeneration&&hand.tracked()&&hand.secondaryActive&&finite(hand.grip.position)&&finite(head.position);
  const bool changed=session_!=f.sessionGeneration||origin_!=f.originGeneration||(time_&&f.predictedTime>time_&&f.predictedTime-time_>settings_.maximumGap);
  session_=f.sessionGeneration;origin_=f.originGeneration;time_=f.predictedTime;
  const bool down=scope&&hand.secondaryPressed;
  // Scope lost or tracking broken: let go of everything; the button must come up before the next use.
  if(!scope||changed){if(busy())out.cancelled=true;idle(down||!scope?State::Blocked:State::Idle);return out;}
  switch(state_){
  case State::Blocked:if(!down)idle();break;
  case State::Idle:if(down){if(holdInteract){state_=State::Hold;out.held=true;}else{state_=State::Pending;since_=now;}}break;
  case State::Hold:if(!down)idle();break;
  case State::Pending:
   if(down&&holdInteract){state_=State::Hold;out.held=true;}
   else if(!down){state_=State::Tap;since_=now;out.tapped=true;out.buttons=tap;}
   else if(now-since_>=settings_.holdMs){
    state_=State::Open;centre_=hand.grip.position;openRoll_=hand.grip.orientation;twist_=0;out.opened=true;stickHeld_=false;
    const auto forward=rotate(head.orientation,{0,0,-1});Vec3 flat{forward.x,0,forward.z};
    const float size=length(flat);
    // Sideways as seen by the user: level, to the right of where the head faces.
    if(size>.2f){flat=flat*(1/size);right_=cross(flat,Vec3{0,1,0});}
   }
   break;
  case State::Open:{
   if(settings_.roll){
    if(settings_.rollFromOpen)twist_=rollBetween(openRoll_,hand.grip.orientation);
    else{float t=settings_.rollCentre-levelRoll(hand.grip.orientation);while(t>pi)t-=2*pi;while(t<=-pi)t+=2*pi;twist_=t;}
    const float size=std::abs(twist_);
    if(size>settings_.rollEngage)pointing_=true;else if(size<settings_.rollRelease)pointing_=false;
    if(!pointing_){x_=y_=0;stickHeld_=false;}
    else{
     const float angle=settings_.rollGain*std::copysign(std::max(0.f,size-settings_.rollRelease),twist_);float d=angle-stick_;while(d>pi)d-=2*pi;while(d<=-pi)d+=2*pi;
     if(!stickHeld_||std::abs(d)>settings_.rollHold){stick_=angle;stickHeld_=true;}
     x_=std::sin(stick_);y_=std::cos(stick_);
    }
   }else{
   const auto d=hand.grip.position-centre_;const float sideways=dot(d,right_),up=d.y,distance=std::hypot(sideways,up);
   if(distance>settings_.select)pointing_=true;else if(distance<settings_.release)pointing_=false;
   if(pointing_&&distance>1e-4f){x_=sideways/distance;y_=up/distance;}else x_=y_=0;
   }
   if(!down){state_=State::Confirm;since_=now;if(pointing_)out.chosen=true;else out.cancelled=true;}
   break;}
  case State::Confirm:if(now-since_>=settings_.confirmMs)idle(down?State::Blocked:State::Idle);break;
  case State::Tap:if(now-since_>=settings_.tapMs)idle(down?State::Blocked:State::Idle);break;
  }
  if(state_==State::Open){out.buttons=wheel;out.steer=true;out.x=x_;out.y=y_;}
  else if(state_==State::Confirm){out.steer=true;out.x=x_;out.y=y_;} // wheel button up, stick still on the item
  else if(state_==State::Tap||state_==State::Hold)out.buttons=tap;
  return out;
 }
};
} // namespace evr::motion
