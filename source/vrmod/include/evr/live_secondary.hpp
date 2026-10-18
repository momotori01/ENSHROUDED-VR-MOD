#pragma once
#include <evr/live_melee.hpp>
#include <array>
namespace evr::motion {
// VHVR's secondary attack by motion (RoomscaleSecondaryAttackUtils.IsSecondaryAttack; docs/analysis/
// vhvr_spec_20261010/melee.md 3.8): at the moment a stroke attacks, the way the hands moved decides between the
// weapon's first and second attack. In Enshrouded the second attack of a melee weapon is its heavy attack: every
// melee weapon of the game data has an action started by the trigger "Heavy", the bare hands have none
// (docs/analysis/weapon_mount_20261010/item-sequences).
//
// MotionHistory is VHVR's PhysicsEstimator for one point (melee.md 3.2): a record every fixed step (0.02 s), the
// velocity over the last eight steps, the mean of the last eight such velocities, and for "the longest locomotion"
// every fourth step the oldest position of the window (sixteen of them, about 1.3 s). Positions are in tracking
// space, so walking does not count (VHVR measures against the camera rig for the same reason).
class MotionHistory {
 struct Sample {XrTime time=0;Vec3 position{};};
 static constexpr XrDuration step=20000000,maximumGap=100000000;
 std::array<Sample,9> positions_{};unsigned positionCount_=0;
 std::array<Vec3,8> velocities_{};unsigned velocityCount_=0;
 std::array<Sample,16> sparse_{};unsigned sparseCount_=0,ticks_=0;
 Vec3 current_{};XrTime last_=0;
 template<class T,size_t N> static void push(std::array<T,N>& a,unsigned& n,const T& v){
  if(n==N){for(size_t i=1;i<N;++i)a[i-1]=a[i];--n;}
  a[n++]=v;}
public:
 void reset(){positionCount_=velocityCount_=sparseCount_=ticks_=0;last_=0;current_={};}
 void add(XrTime time,Vec3 position){
  if(!finite(position)||time<=0){reset();return;}
  if(last_&&(time<=last_||time-last_>maximumGap))reset();
  last_=time;current_=position;
  if(positionCount_&&time-positions_[positionCount_-1].time<step)return;   // between two steps
  push(positions_,positionCount_,Sample{time,position});
  if(positionCount_>=2){const auto& oldest=positions_[0];
   push(velocities_,velocityCount_,(position-oldest.position)*(1e9f/float(time-oldest.time)));}
  if(++ticks_%4==0)push(sparse_,sparseCount_,positions_[0]);
 }
 Vec3 velocity()const{return velocityCount_?velocities_[velocityCount_-1]:Vec3{};}
 Vec3 averageVelocity()const{Vec3 sum{};for(unsigned i=0;i<velocityCount_;++i)sum=sum+velocities_[i];return velocityCount_?sum*(1.f/float(velocityCount_)):sum;}
 // The displacement from the farthest of the sparse records of the last `seconds` to where the point is now.
 Vec3 longestLocomotion(float seconds)const{
  Vec3 best{};float size=0;
  for(unsigned i=0;i<sparseCount_;++i){if(float(last_-sparse_[i].time)*1e-9f>seconds)continue;
   const auto d=current_-sparse_[i].position;const float n=length(d);if(n>size){size=n;best=d;}}
  return best;
 }
};
// Which of VHVR's rules a weapon takes. Native weapon types (weapon_category.hpp): OneHandAxe 0, OneHandClub 1,
// OneHandSword 2, GreatSword 10, Dagger 13. VHVR has none for sledges and spears; its battle axe and polearm rule
// belongs to Valheim's own moves of those weapons and is not taken over (two-hand axes, clubs and hammers: none).
enum class SecondaryStyle:uint8_t {None,Sword,AxeClub,DualKnives};
inline SecondaryStyle secondaryStyle(uint8_t weaponType,bool pair){
 if(weaponType==2||weaponType==10)return SecondaryStyle::Sword;
 if(weaponType==0||weaponType==1)return SecondaryStyle::AxeClub;
 if(weaponType==13&&pair)return SecondaryStyle::DualKnives;
 return SecondaryStyle::None;
}
inline const char* secondaryStyleName(SecondaryStyle s){
 switch(s){case SecondaryStyle::Sword:return "sword";case SecondaryStyle::AxeClub:return "axe_club";case SecondaryStyle::DualKnives:return "dual_knives";default:return "none";}
}
// Everything in tracking space. "main" is the hand that struck, "other" the second hand.
struct SecondaryMotion {
 bool twoHanded=false;
 Vec3 forward{0,1,0},otherForward{0,1,0};     // the axis of the weapon in the striking hand / of the weapon in the other hand
 Vec3 mainVelocity{},otherVelocity{};         // the hands' velocities of this frame
 Vec3 mainAverage{},otherAverage{};           // MotionHistory::averageVelocity of the hands
 Vec3 mainLongest{};                          // the striking hand's longest locomotion of the last second
 Vec3 hitLongest{};                           // the hit point's (middle of the blade) longest locomotion of the last half second
 Vec3 mainPosition{},otherPosition{},player{};// the hands, and the body (below the head)
};
struct SecondaryResult {bool secondary=false,thrust=false,strongThrust=false,strongSwing=false,hook=false,bothHands=false;};
inline float degreesBetween(Vec3 a,Vec3 b){
 const float n=length(a)*length(b);if(n<1e-8f)return 180.f;
 return std::acos(std::clamp(dot(a,b)/n,-1.f,1.f))*180.f/pi;
}
// swingSpeed: VHVR SwingSpeedRequirement (3 m/s).
inline SecondaryResult secondaryAttack(SecondaryStyle style,const SecondaryMotion& m,float swingSpeed=3.f){
 SecondaryResult r;const float axis=length(m.forward);
 if(style==SecondaryStyle::None||axis<1e-4f||!finite(m.forward)||!finite(m.mainVelocity)||!finite(m.mainAverage)||!finite(m.mainLongest)||!finite(m.hitLongest))return r;
 const auto forward=m.forward*(1/axis);
 // A thrust: the hand's average velocity within 30 degrees of the weapon (two hands 40).
 r.thrust=degreesBetween(m.mainAverage,forward)<(m.twoHanded?40.f:30.f);
 const float thrustSpeed=std::max(swingSpeed,3.f);
 r.strongThrust=r.thrust&&(m.twoHanded?dot(m.mainVelocity+m.otherVelocity,forward)>thrustSpeed:
  dot(m.mainVelocity,forward)>thrustSpeed&&dot(m.mainLongest,forward)>=1.f);
 // A strong swing: without the part of the movement that goes sideways around the body.
 const auto withoutSide=[&](Vec3 v,Vec3 hand){
  const auto side=cross(Vec3{0,1,0},hand-m.player);const float n=length(side);
  return n<1e-5f?v:v-side*(dot(v,side)/(n*n));};
 const float strong=std::clamp(1.5f*swingSpeed,3.f,9.f);
 r.strongSwing=m.twoHanded?length(withoutSide(m.mainVelocity,m.mainPosition))+length(withoutSide(m.otherVelocity,m.otherPosition))>strong:
  length(withoutSide(m.mainVelocity,m.mainPosition))>=strong&&length(withoutSide(m.hitLongest,m.mainPosition))>=1.5f;
 // A hook: fast on average, a metre travelled that way within the last second, both within 30 degrees.
 {const float speed=length(m.mainAverage);
  r.hook=speed>=std::clamp(1.25f*swingSpeed,3.f,9.f)&&dot(m.mainLongest,m.mainAverage)>=speed&&degreesBetween(m.mainLongest,m.mainAverage)<=30.f;}
 // Both hands strike: each hand moves along its own weapon at 1 m/s or more, 4 m/s together, the hands less than 0.75 m apart.
 {const float other=length(m.otherForward);
  if(other>1e-4f&&finite(m.otherForward)&&finite(m.otherAverage)){
   const float a=dot(m.mainAverage,forward),b=dot(m.otherAverage,m.otherForward*(1/other));
   r.bothHands=a>=1.f&&b>=1.f&&a+b>=4.f&&length(m.mainPosition-m.otherPosition)<.75f;}}
 switch(style){
 case SecondaryStyle::Sword:r.secondary=r.strongThrust;break;
 case SecondaryStyle::AxeClub:r.secondary=!r.thrust&&r.strongSwing;break;
 case SecondaryStyle::DualKnives:r.secondary=r.bothHands||r.hook;break;
 default:break;}
 return r;
}
}
