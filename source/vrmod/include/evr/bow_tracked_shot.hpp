#pragma once
#include <evr/bow_physical_trial.hpp>
#include <evr/bow_aim.hpp>
namespace evr::motion {
// Owns values, never borrowed game pointers. One release, one consume, bounded
// to this tracked actor/binding/session/origin and the immediate native shot.
struct BowTrackedShotValue {
 BowWorldAim aim{};BowPhysicalGeometry geometry{};Vec3 nock{};
 uint64_t frame=0,milliseconds=0,session=0,origin=0;XrTime predicted=0;
 std::array<XrPath,2> profiles{};
};
class BowTrackedShot {
 bool armed_=false,consumed_=false;BowTrackedShotValue value_{};
 static bool scope(const HandFrame& f,const BowPhysicalGeometry& g){
  return g.sourceWorld&&g.entity&&g.binding&&g.frame==f.frame&&f.frame&&f.sessionGeneration&&f.originGeneration&&f.predictedTime>0&&f.focused&&f.hands[0].tracked()&&f.hands[1].tracked();
 }
public:
 void invalidate(){armed_=false;}
 const BowTrackedShotValue& value()const{return value_;}
 bool arm(const HandFrame& f,const BowPhysicalGeometry& g,uint64_t now,float minimumForward=.5f){
  if(armed_||consumed_||!now||!scope(f,g))return false;
  try{
   const auto delta=rotate(inverse(g.model.orientation),f.hands[1].grip.position-g.model.position);
   const Vec3 nock{delta.x/g.scale.x,delta.y/g.scale.y,delta.z/g.scale.z};
   const auto aim=knownBowWorldAim(g.worldPosition,g.worldOrientation,g.scale,nock,minimumForward);
   value_={aim,g,nock,f.frame,now,f.sessionGeneration,f.originGeneration,f.predictedTime,{f.hands[0].profile,f.hands[1].profile}};
  }catch(const std::exception&){return false;}
  armed_=true;return true;
 }
 // The one-shot diagnostic keeps 200 ms/8 frames. Repeatable play (BowDraw) passes
 // the native aim window instead, so a slow frame does not swallow the shot.
 bool consume(const HandFrame& f,const BowPhysicalGeometry& g,uint64_t now,uint32_t nativeEntity,BowTrackedShotValue& out,uint64_t maximumMs=200,uint64_t maximumFrames=8){
  if(!armed_||consumed_)return false;
  const auto& v=value_;
  const bool same=scope(f,g)&&nativeEntity==v.geometry.entity&&g.entity==nativeEntity&&g.sourceWorld==v.geometry.sourceWorld&&g.binding==v.geometry.binding&&
   f.sessionGeneration==v.session&&f.originGeneration==v.origin&&f.hands[0].profile==v.profiles[0]&&f.hands[1].profile==v.profiles[1]&&
   now>=v.milliseconds&&now-v.milliseconds<=maximumMs&&f.frame>=v.frame&&f.frame-v.frame<=maximumFrames&&f.predictedTime>=v.predicted;
  armed_=false;if(!same)return false;consumed_=true;out=v;return true;
 }
};
}
