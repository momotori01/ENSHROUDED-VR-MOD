#pragma once
#include <evr/motion_actions.hpp>
#include <array>
#include <cstdint>
namespace evr::motion {
// Value-only remapping. The caller supplies a same-time HMD/hand sample and
// native gamepad fields. No UI, player transform, XR API or game calls here.
struct GesturePad {
 XrVector2f left{},right{};uint16_t buttons=0;float leftTrigger=0,rightTrigger=0;
};
struct GestureHead {
 Pose pose=identityPose();XrSpaceLocationFlags flags=0;
 uint64_t frame=0,session=0,origin=0;XrTime time=0;
};
// 2026-10-05 18:50 user spec: the left hand at the left temple also turns the right stick
// into the D-pad (building and other compound input). Only the right stick and the two
// stick clicks change; the left stick keeps moving the player inside either region.
struct HeadGestureConfig {
 Vec3 rightTemple{.18f,0,0},leftTemple{-.18f,0,0};float enterRadius=.10f,exitRadius=.14f;
 XrDuration entryDwell=80000000,maximumGap=100000000,pulseDuration=30000000;
 float stickOn=.65f,stickOff=.45f,neutral=.20f,pulseAmplitude=.15f;
 bool leftHand=true;
};
struct HeadGestureResult {
 uint64_t frame=0,session=0,origin=0;XrTime time=0;
 // "left" means the gesture was left (ended) this frame; it is not the left hand.
 GesturePad pad,sourcePad;bool active=false,entered=false,left=false,resetSwing=false,valid=false;
 float pulseAmplitude=0;XrDuration pulseDuration=0;uint16_t mappedButtons=0;
 Vec3 handInHead{},leftHandInHead{};  // right / left grip in head space (left zero when unused)
 uint8_t activeHands=0;               // bit0 left hand region, bit1 right hand region
 unsigned enteredHand=1;              // hand whose arrival started the gesture (pulse target)
 bool leftTracked=false;
};
class HeadGesture {
 HeadGestureConfig c;
 // Each hand has its own region state; the gesture is active while either region is.
 struct Side {bool active=false,needOutside=true;XrTime entrySince=0;};
 std::array<Side,2> side_{};
 bool active_=false,rightReady_=false,leftClickReady_=false,rightClickReady_=false;
 uint64_t frame_=0,session_=0,origin_=0;XrTime time_=0;XrPath profile_=0,leftProfile_=0;
 int x_=0,y_=0;
 static constexpr uint16_t up=1,down=2,left=4,right=8,start=0x10,back=0x20,leftClick=0x40,rightClick=0x80;
 static bool finitePad(const GesturePad& p){
  const float a[]{p.left.x,p.left.y,p.right.x,p.right.y,p.leftTrigger,p.rightTrigger};
  for(auto v:a)if(!std::isfinite(v)||v< -1||v>1)return false;
  return p.leftTrigger>=0&&p.rightTrigger>=0;
 }
 void rearm(){rightReady_=leftClickReady_=rightClickReady_=false;x_=y_=0;}
 void invalid(){active_=false;side_={};rearm();frame_=session_=origin_=0;time_=0;profile_=leftProfile_=0;}
 // Returns whether this hand's region inhibits swings this frame; sets entered on activation.
 bool region(Side& s,float distance,XrTime now,bool& entered)const{
  bool reset=s.needOutside||s.entrySince!=0||distance<=c.enterRadius;
  if(s.needOutside){if(distance>=c.exitRadius)s.needOutside=false;s.entrySince=0;}
  else if(s.active){if(distance>=c.exitRadius){s.active=false;s.entrySince=0;reset=true;}}
  else if(distance<=c.enterRadius){
   if(!s.entrySince)s.entrySince=now;
   if(now-s.entrySince>=c.entryDwell){s.active=true;entered=true;reset=true;}
  }else s.entrySince=0;
  return reset||s.active;
 }
 int axis(float v,int was)const {
  if(v>=c.stickOn)return 1;if(v<=-c.stickOn)return -1;
  if(was==1&&v>c.stickOff)return 1;if(was==-1&&v< -c.stickOff)return -1;return 0;
 }
 void controls(HeadGestureResult& out,const GesturePad& in){
  if(std::abs(in.right.x)<=c.neutral&&std::abs(in.right.y)<=c.neutral)rightReady_=true;
  if(!(in.buttons&leftClick))leftClickReady_=true;
  if(!(in.buttons&rightClick))rightClickReady_=true;
  out.pad=in;
  if(!rightReady_)out.pad.right={};
  if(!leftClickReady_)out.pad.buttons&=~leftClick;
  if(!rightClickReady_)out.pad.buttons&=~rightClick;
  if(!active_)return;
  x_=rightReady_?axis(in.right.x,x_):0;y_=rightReady_?axis(in.right.y,y_):0;
  uint16_t mapped=(x_>0?right:x_<0?left:0)|(y_>0?up:y_<0?down:0);
  if(rightClickReady_&&(in.buttons&rightClick))mapped|=start;
  if(leftClickReady_&&(in.buttons&leftClick))mapped|=back;
  out.pad.right={};out.pad.buttons&=~(leftClick|rightClick);out.pad.buttons|=mapped;
  // Preserve unrelated fields; contradictory D-pad inputs cancel that axis.
  if((out.pad.buttons&(left|right))==(left|right))out.pad.buttons&=~(left|right);
  if((out.pad.buttons&(up|down))==(up|down))out.pad.buttons&=~(up|down);
  out.mappedButtons=mapped;
 }
public:
 explicit HeadGesture(HeadGestureConfig config={}):c(config){
  if(!finite(c.rightTemple)||c.rightTemple.x<=0||(c.leftHand&&(!finite(c.leftTemple)||c.leftTemple.x>=0))||!std::isfinite(c.enterRadius)||!std::isfinite(c.exitRadius)||
    c.enterRadius<=0||c.exitRadius<=c.enterRadius||c.entryDwell<=0||c.maximumGap<=0||
    c.pulseDuration<=0||c.pulseDuration>100000000||!std::isfinite(c.pulseAmplitude)||c.pulseAmplitude<=0||c.pulseAmplitude>1||
    !std::isfinite(c.stickOn)||!std::isfinite(c.stickOff)||!std::isfinite(c.neutral)||
    c.neutral<0||c.neutral>=c.stickOff||c.stickOff>=c.stickOn||c.stickOn>1)
   throw std::invalid_argument("invalid head gesture configuration");
 }
 HeadGestureResult update(const HandFrame& hands,const GestureHead& head,const GesturePad& in){
  HeadGestureResult out;out.frame=hands.frame;out.session=hands.sessionGeneration;out.origin=hands.originGeneration;out.time=hands.predictedTime;out.pad=in;out.sourcePad=in;const bool was=active_;
  auto cancel=[&](){invalid();out.left=was;out.resetSwing=true;out.pad.right={};out.pad.buttons&=~(leftClick|rightClick);return out;};
  if(!finitePad(in)){out.pad={};return cancel();}
  if(!hands.focused||!hands.frame||!hands.sessionGeneration||!hands.originGeneration||hands.predictedTime<=0||
   head.frame!=hands.frame||head.session!=hands.sessionGeneration||head.origin!=hands.originGeneration||head.time!=hands.predictedTime||
   (head.flags&15)!=15||!hands.hands[1].tracked()||!finite(head.pose.position)||!finite(hands.hands[1].grip.position))return cancel();
  const bool epoch=frame_&&(session_!=hands.sessionGeneration||origin_!=hands.originGeneration||profile_!=hands.hands[1].profile);
  const bool gap=frame_&&(hands.frame<=frame_||hands.predictedTime<=time_||hands.predictedTime-time_>c.maximumGap);
  if(epoch||gap)return cancel();
  try{out.handInHead=rotate(inverse(head.pose.orientation),hands.hands[1].grip.position-head.pose.position);}
  catch(...){return cancel();}
  if(!finite(out.handInHead))return cancel();
  // The left hand is optional: losing it (or a profile change) only clears its own
  // region, which then needs a fresh exit before it can start the gesture again.
  const auto& leftHand=hands.hands[0];
  bool leftOk=c.leftHand&&leftHand.tracked()&&finite(leftHand.grip.position)&&(!leftProfile_||leftProfile_==leftHand.profile);
  if(leftOk){
   try{out.leftHandInHead=rotate(inverse(head.pose.orientation),leftHand.grip.position-head.pose.position);}catch(...){leftOk=false;}
   leftOk=leftOk&&finite(out.leftHandInHead);
  }
  if(!leftOk){side_[0]={};leftProfile_=c.leftHand&&leftHand.tracked()?leftHand.profile:0;out.leftHandInHead={};}
  else leftProfile_=leftHand.profile;
  out.leftTracked=leftOk;
  frame_=hands.frame;session_=hands.sessionGeneration;origin_=hands.originGeneration;time_=hands.predictedTime;profile_=hands.hands[1].profile;
  out.valid=true;
  // Combat is inhibited from candidate entry, before the UI dwell expires,
  // and on its cancellation. Raising a hand into its reserved region must
  // not become a new swing during the confirmation delay.
  bool enteredRight=false,enteredLeft=false;
  out.resetSwing=region(side_[1],length(out.handInHead-c.rightTemple),hands.predictedTime,enteredRight);
  if(leftOk)out.resetSwing|=region(side_[0],length(out.leftHandInHead-c.leftTemple),hands.predictedTime,enteredLeft);
  active_=side_[0].active||side_[1].active;
  out.activeHands=uint8_t((side_[0].active?1:0)|(side_[1].active?2:0));
  if(active_&&!was){rearm();out.entered=true;out.enteredHand=enteredRight?1:0;out.resetSwing=true;
   out.pulseAmplitude=c.pulseAmplitude;out.pulseDuration=c.pulseDuration;}
  else if(!active_&&was){rearm();out.left=true;out.resetSwing=true;}
  out.active=active_;out.resetSwing|=active_;controls(out,in);return out;
 }
};
}
