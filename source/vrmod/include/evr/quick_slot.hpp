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
struct QuickSlotSettings {
 bool roll=true;             // the wrist's roll as a dial (false: the hand's movement)
 float rollEngage=.2094f,rollRelease=.1396f,rollGain=2.f;   // 12 and 8 degrees; the wheel turns twice as far as the wrist
 float rollCentre=.7854f;    // 45 degrees counter-clockwise from level: the wrist's rest on the wheel (nothing chosen)
 bool rollFromOpen=false;    // true: the centre is the wrist's roll when the wheel opened
 uint64_t holdMs=250;        // "long press": shorter is the ordinary Y tap
 uint64_t tapMs=100;         // length of that Y tap
 float select=.07f;          // VHVR: 7 cm from the centre points at an item
 float release=.05f;         // hysteresis back to the centre
 uint64_t confirmMs=120;     // the stick stays on the item this long after the wheel button is let go
 XrDuration maximumGap=250000000;
};
struct QuickSlotResult {
 uint16_t buttons=0;bool steer=false;float x=0,y=0; // steer: replace the right stick with (x, y)
 bool opened=false,chosen=false,cancelled=false,tapped=false;
};
class QuickSlot {
 enum class State {Idle,Pending,Open,Confirm,Tap,Blocked};
 QuickSlotSettings settings_{};State state_=State::Blocked;
 uint64_t since_=0,session_=0,origin_=0;XrTime time_=0;
 Vec3 centre_{},right_{1,0,0};Quat openRoll_{0,0,0,1};float x_=0,y_=0,twist_=0;bool pointing_=false;
 void idle(State s=State::Idle){state_=s;pointing_=false;x_=y_=0;}
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
 bool busy()const{return state_==State::Pending||state_==State::Open||state_==State::Confirm;}
 // allowed: gameplay (or the wheel this gesture opened), fresh hands, no other gesture.
 // head: for the sideways direction; the mapping is frozen when the wheel opens.
 QuickSlotResult update(const HandFrame& f,Pose head,bool allowed,uint64_t now){
  QuickSlotResult out;const auto& hand=f.hands[1];
  const bool scope=allowed&&f.focused&&f.sessionGeneration&&hand.tracked()&&hand.secondaryActive&&finite(hand.grip.position)&&finite(head.position);
  const bool changed=session_!=f.sessionGeneration||origin_!=f.originGeneration||(time_&&f.predictedTime>time_&&f.predictedTime-time_>settings_.maximumGap);
  session_=f.sessionGeneration;origin_=f.originGeneration;time_=f.predictedTime;
  const bool down=scope&&hand.secondaryPressed;
  // Scope lost or tracking broken: let go of everything; the button must come up before the next use.
  if(!scope||changed){if(busy())out.cancelled=true;idle(down||!scope?State::Blocked:State::Idle);return out;}
  switch(state_){
  case State::Blocked:if(!down)idle();break;
  case State::Idle:if(down){state_=State::Pending;since_=now;}break;
  case State::Pending:
   if(!down){state_=State::Tap;since_=now;out.tapped=true;out.buttons=tap;}
   else if(now-since_>=settings_.holdMs){
    state_=State::Open;centre_=hand.grip.position;openRoll_=hand.grip.orientation;twist_=0;out.opened=true;
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
    if(pointing_){const float angle=settings_.rollGain*twist_;x_=std::sin(angle);y_=std::cos(angle);}else x_=y_=0;
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
  else if(state_==State::Tap)out.buttons=tap;
  return out;
 }
};
} // namespace evr::motion
