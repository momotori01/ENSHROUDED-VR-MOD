#pragma once
#include <evr/motion_intent.hpp>
#include <evr/weapon_mount.hpp>
namespace evr::motion {
// Explicit display trial for measured axe/hammer assets. VHVR reference:
// acquire near the handle, keep front/rear order, solve orientation from both
// hands and let the dominant grip slide along the handle. No combat authority.
struct LiveWieldPose {
 Pose pose=identityPose();bool active=false;float mainY=0,offY=0,distance=0;
};
class LiveWield {
 bool active_=false,armed_=false,vhvr_=false,rightBehind_=false;float offY_=0;
 uint64_t binding_=0,session_=0,origin_=0,lastFrame_=0;XrTime time_=0;
 std::array<XrPath,2> profiles_{};
 void cancel(){active_=armed_=false;}
 // VHVR LocalWeaponWield/WeaponWield (docs/analysis/vhvr_spec_20261010/melee.md 2.2-2.4).
 // Enter: the left grip held (user rule 2026-10-11: the weapon is already in the right
 // hand, so its grip is not asked for; VHVR asks for both), the line between the hands
 // within 60 degrees of the weapon axis (either hand may be the rear one), and the hands
 // no farther apart along the axis than half the weapon. No reach to the handle is
 // required. While two-handed the weapon lies on the line between the hands; VHVR's
 // default slide puts the rear hand on the rear grip when the hands are far apart, the
 // front hand on the main grip when close. The left grip released returns to one hand
 // (hold to keep, VHVR's non-sticky mode).
 LiveWieldPose vhvr(const HandFrame& f,uint64_t binding,const NativeMeleeMount& p,Vec3 scale,Pose single,bool allowed){
  LiveWieldPose out;out.pose=single;
  const auto& left=f.hands[0];const auto& right=f.hands[1];
  const bool scope=binding&&p.known&&p.tipY>p.rearY&&f.focused&&left.tracked()&&right.tracked()&&left.grabActive&&
   finite(left.grip.position)&&finite(right.grip.position)&&finite(scale)&&scale.x>0&&scale.y>0&&scale.z>0&&allowed;
  const bool changed=binding_!=binding||session_!=f.sessionGeneration||origin_!=f.originGeneration||
   profiles_[0]!=left.profile||profiles_[1]!=right.profile||f.frame<=lastFrame_||f.predictedTime<=time_||
   (time_&&f.predictedTime-time_>250000000);
  if(changed||!scope)active_=false;
  binding_=binding;session_=f.sessionGeneration;origin_=f.originGeneration;profiles_={left.profile,right.profile};lastFrame_=f.frame;time_=f.predictedTime;
  if(!scope)return out;
  if(!left.grab){active_=false;return out;}
  const auto between=left.grip.position-right.grip.position;const float distance=length(between);
  if(!std::isfinite(distance)||distance<.05f||distance>1.8f){active_=false;return out;}
  const auto forward=rotate(single.orientation,{0,1,0});
  if(!active_){
   const float along=dot(between,forward);
   if(std::abs(along)>.5f*(p.tipY-p.rearY)*scale.y)return out;
   const float angle=std::acos(std::clamp(along/distance,-1.f,1.f))*180.f/pi;
   if(angle<60)rightBehind_=true;else if(angle>120)rightBehind_=false;else return out;
   active_=true;
  }
  const auto& rear=rightBehind_?right:left;const auto& front=rightBehind_?left:right;
  const auto axis=(front.grip.position-rear.grip.position)*(1/distance);
  const float grip=p.gripInModel.y*scale.y,reach=(p.rearReach>0?p.rearReach:p.gripInModel.y)*scale.y;
  const float toMain=p.mainBehind>0&&rightBehind_?p.mainBehind*scale.y:std::min(distance,reach); // rear hand to the main grip point
  const auto q=normalized(multiply(alignDirections(forward,axis),single.orientation));
  out.pose={q,rear.grip.position+axis*(toMain-grip)};out.active=true;out.distance=distance;
  const float rearY=(grip-toMain)/scale.y,frontY=(grip-toMain+distance)/scale.y;
  out.mainY=rightBehind_?rearY:frontY;out.offY=rightBehind_?frontY:rearY;
  return out;
 }
public:
 LiveWield()=default;
 explicit LiveWield(bool vhvrEntry):vhvr_(vhvrEntry){}
 void reset(){const bool mode=vhvr_;*this=LiveWield{};vhvr_=mode;}
 bool vhvrEntry()const{return vhvr_;}
 LiveWieldPose update(const HandFrame& f,uint64_t binding,const NativeMeleeMount& p,Vec3 scale,Pose single,bool allowed){
  if(vhvr_)return vhvr(f,binding,p,scale,single,allowed);
  LiveWieldPose out;out.pose=single;
  const auto& left=f.hands[0];const auto& right=f.hands[1];
  const bool scope=binding&&p.known&&f.focused&&left.tracked()&&right.tracked()&&left.grabActive&&right.grabActive&&
   finite(left.grip.position)&&finite(right.grip.position)&&finite(scale)&&scale.x>0&&scale.y>0&&scale.z>0&&allowed;
  const bool changed=binding_!=binding||session_!=f.sessionGeneration||origin_!=f.originGeneration||
   profiles_[0]!=left.profile||profiles_[1]!=right.profile||f.frame<=lastFrame_||f.predictedTime<=time_||
   (time_&&f.predictedTime-time_>250000000);
  if(changed||!scope)cancel();
  binding_=binding;session_=f.sessionGeneration;origin_=f.originGeneration;profiles_={left.profile,right.profile};lastFrame_=f.frame;time_=f.predictedTime;
  if(!scope)return out;
  if(!right.grab){active_=false;armed_=true;return out;}
  if(!left.grab){active_=false;armed_=false;return out;}
  const auto actual=left.grip.position-right.grip.position;const float distance=length(actual);
  if(!std::isfinite(distance)||distance<.12f||distance>1.2f){cancel();return out;}
  if(!active_){
   if(!armed_)return out;
   const auto local=compose(inverse(single),left.grip).position;
   const Vec3 model{local.x/scale.x,local.y/scale.y,local.z/scale.z};
   // The measured rear/native-left grip is at 0 and the native-right grip is
   // p.gripInModel.y. A 2 cm endpoint margin covers the observed <9 mm spread.
   // Acquisition radius 6 cm is a trial setting, not a measured combat shape.
   if(std::hypot(local.x,local.z)>.06f||model.y<-.02f||model.y>p.gripInModel.y-.10f){cancel();return out;}
   offY_=model.y;active_=true;armed_=false;
  }
  // Keep the support grip fixed on the shaft. Moving the right hand changes
  // its axial coordinate. Account for the measured palm's radial offset.
  const auto pivot=p.gripInModel;
  const float radial=std::pow(pivot.x*scale.x,2.f)+std::pow(pivot.z*scale.z,2.f);
  if(distance*distance<=radial){cancel();return out;}
  const float mainY=offY_+std::sqrt(distance*distance-radial)/scale.y;
  if(mainY<-.02f||mainY>pivot.y+.02f){cancel();return out;}
  const Vec3 main{pivot.x*scale.x,mainY*scale.y,pivot.z*scale.z},off{0,offY_*scale.y,0};
  const auto correction=alignDirections(rotate(single.orientation,off-main),actual);
  const auto q=normalized(multiply(correction,single.orientation));
  out.pose={q,right.grip.position-rotate(q,main)};out.active=true;out.mainY=mainY;out.offY=offY_;out.distance=distance;
  return out;
 }
};
}
