#pragma once
#include <evr/hand_rig.hpp>
#include <evr/motion_intent.hpp>
#include <vector>
namespace evr::bodyik {
// IK for the player's own skeleton, after VHVR's use of VRIK
// (docs/analysis/vhvr_spec_20261010/player_ik.md 1.1-1.8, 9; the skeleton: enshrouded_skeleton.md).
// The game's animation is the input. The head, the chest block (chest, neck, clavicles, shoulders)
// and both arms are then replaced from the headset and the controllers; the spine, neck and
// shoulders start from the rest pose, so the walk animation does not sway the tracked upper body.
// The head turns about a point in the neck (eyeFromPivot), and the chest block hangs from that point:
// looking down or around then leaves the chest where it is, as on a real neck.
// The body's heading is the headset's. The hips keep their animated height as long as the spine can
// take the difference (it may shorten by torsoCompress, never lengthen); the legs keep the animated
// stride and foot height under the moved hips and the knees are solved again. Fingers, face and
// toes keep the animation.
// Everything is in the character model's space (origin at the feet, +Y up, rest pose facing +Z);
// value-only.
using hand::Joint;
struct Skeleton {
 uint16_t hips=1,chest=15,neck=16,head=17;
 std::vector<uint16_t> spine{10,11,12,13,14},upper{39,69}; // between hips and chest; rigid with the chest
 std::array<uint16_t,2> shoulder{40,70},upperTwist{41,71},elbow{42,72},foreTwist{43,73},wrist{44,74},socket{64,94},elbowHelper{98,99};
 std::array<uint16_t,2> thigh{2,6},knee{3,7},ankle{4,8},kneeHelper{101,100};
};
// The headset (midpoint of the eyes) and both hand sockets (the grips). A hand that is not tracked leaves its arm to
// the animation, hanging from the tracked chest.
// swim: in the water the pelvis and the legs are the animation's (VHVR while swimming: pelvis position weight 0, the
// legs not solved - player_ik.md 1.11, VRPlayer.cs:1552-1566, 1603-1651). The hips go toward where the animation has
// them, as far as the spine's length allows (VRIK keeps the head on its target and pulls the pelvis along), with the
// animated orientation; the legs keep the animation under them. Both are first turned to the headset's heading (VHVR's
// player model faces where the player looks, in the water too). Run game-20261013-185120: with the standing rule the
// swim animation's feet, at hip height behind the body, were put under upright hips again, turned by a heading taken
// from hips that face down - the legs stuck out in front of the player and swung round from frame to frame.
// hipsGiven: the pelvis is not worked out here but taken as it is - another player's body, whose owner's solve gave
// it (VHVR's receiving side follows the sent pelvis with full weight, player_sync.md 4.5; include/evr/player_sync.hpp).
// legsRest (0..1): the legs leave the animation for the rest pose hanging under the hips (1) - the double jump's flip, whose
// feet came up in front of the eyes (user, 2026-10-18 after the headset run of 13:30: "the double jump without the legs'
// animation"); between 0 and 1 a blend, for the way in and out.
// legsBack (radians): the hanging legs swung back by this much about the body's right (the glider's flying-squirrel pose,
// user 2026-10-22 23:30: "gliding is a flying squirrel's pose - can the legs, the body, stretch backward?"); 0 straight down.
// legsHeld (with heldWeight 0..1): the hanging legs are these instead of the rest pose's - the legs of the first jump,
// kept at the double jump's second press (legsNow below; user 2026-10-22 23:45: "can the double jump's legs move as in the
// first jump? suddenly standing straight bothers me a little"); toward the rest pose (swung back by legsBack) as the
// weight falls, for the glider.
struct Targets {Pose head;std::array<Pose,2> socket;std::array<bool,2> hand{true,true};bool swim=false;bool hipsGiven=false;Pose hips=identityPose();float legsRest=0;float legsBack=0;
 const std::vector<Pose>* legsHeld=nullptr;float heldWeight=0;};
struct Settings {
 Vec3 eyeInHead{0,.0522f,.1f}; // head joint -> midpoint of the eye joints in the rest pose (measured)
 // Neck pivot -> eyes, in the head's frame. Fitted to the headset poses of the first real-device run (2026-10-11,
 // game100334: the point that stays in place while the user looks around is .126 below and .098 behind the
 // headset, 3-9 mm rms over 300-frame windows). VHVR places its head target .1485 below and .081 behind.
 Vec3 eyeFromPivot{0,.13f,.1f};
 float torsoCompress=.25f;     // the hips-to-chest distance may shrink to this part less than rest before the hips go down
 float shoulderReach=.06f;     // a hand past the arm's length first draws its shoulder this far toward it
 float kneeForward=.05f;       // knee direction when the animated leg is straight (it fades out as the animation bends the knee)
};
struct Report {std::array<float,2> socketError{},armStretch{},legShort{};float hipsBelowAnimated=0,torso=1;Vec3 pivot{};Pose hips=identityPose();}; // hips: the solved pelvis, in the model's space
inline Pose pose(const Joint& j){return {j.orientation,j.position};}
// The solver's space stands upright (+Y up). The game leans the whole character model while it swims and dives (same
// run: 20 to 75 degrees), so the model's own space does not: the caller solves in the model's space stood upright -
// uprightPart(q), the turn of the model's orientation about +Y - and the rest of the orientation, leanPart(q), goes
// onto the animated root joints before the solve and comes off the result after it (lean()).
inline Quat uprightPart(Quat q){q=normalized(q);const float n=std::sqrt(q.y*q.y+q.w*q.w);return n<1e-4f?Quat{0,0,0,1}:Quat{0,q.y/n,0,q.w/n};}
inline Quat leanPart(Quat q){return normalized(multiply(inverse(uprightPart(q)),normalized(q)));}
inline float leanAngle(Quat q){const auto l=leanPart(q);return 2*std::acos(std::min(1.f,std::abs(l.w)));}
inline void lean(std::vector<Joint>& joints,const std::vector<uint16_t>& parents,Quat by){
 if(joints.size()!=parents.size())throw std::invalid_argument("body skeleton size");
 for(size_t i=0;i<joints.size();i++)if(parents[i]==UINT16_MAX){const auto p=compose(Pose{normalized(by),{0,0,0}},pose(joints[i]));joints[i].orientation=normalized(p.orientation);joints[i].position=p.position;}
}
inline Quat blend(Quat a,Quat b,float f){
 if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
 return normalized(Quat{a.x+(b.x-a.x)*f,a.y+(b.y-a.y)*f,a.z+(b.z-a.z)*f,a.w+(b.w-a.w)*f});
}
inline Vec3 unit(Vec3 v,Vec3 fallback){const float n=length(v);return n>1e-6f?v*(1/n):fallback;}
// Middle joint of a two-bone chain (law of cosines), bent toward pole. A target out of reach gives the straight chain.
inline Vec3 twoBoneMiddle(Vec3 root,Vec3 target,float a,float b,Vec3 pole){
 if(!finite(root)||!finite(target)||!finite(pole)||!(a>1e-4f)||!(b>1e-4f))throw std::invalid_argument("two bone input");
 const auto along=unit(target-root,{0,-1,0});
 const float distance=std::clamp(length(target-root),std::abs(a-b)+1e-4f,a+b-1e-4f);
 const float x=(a*a-b*b+distance*distance)/(2*distance),h=std::sqrt(std::max(a*a-x*x,0.f));
 auto side=pole-along*dot(pole,along);
 if(length(side)<1e-4f)side=cross(along,std::abs(along.x)<.8f?Vec3{1,0,0}:Vec3{0,0,1});
 return root+along*x+unit(side,{0,0,1})*h;
}
// Rotation taking the frame (direction, normal) onto another such frame; the normals need not be exactly perpendicular.
inline Quat alignFrames(Vec3 fromDirection,Vec3 fromNormal,Vec3 toDirection,Vec3 toNormal){
 const auto first=motion::alignDirections(fromDirection,toDirection);
 const auto d=unit(toDirection,{0,1,0});
 auto a=rotate(first,fromNormal);a=a-d*dot(a,d);const auto b=toNormal-d*dot(toNormal,d);
 if(length(a)<1e-4f||length(b)<1e-4f)return normalized(first);
 return normalized(multiply(motion::alignDirections(a,b),first));
}
// Half of the part of q that turns about axis (unit): shares a wrist's twist with the forearm.
inline Quat halfTwist(Quat q,Vec3 axis){
 const float d=q.x*axis.x+q.y*axis.y+q.z*axis.z,n=std::sqrt(d*d+q.w*q.w);
 if(n<1e-6f)return {0,0,0,1};
 Quat t{axis.x*d/n,axis.y*d/n,axis.z*d/n,q.w/n};if(t.w<0)t={-t.x,-t.y,-t.z,-t.w};
 return normalized({t.x,t.y,t.z,t.w+1});
}
inline Quat levelHeading(Vec3 forward,Quat fallback){
 const Vec3 flat{forward.x,0,forward.z};return length(flat)<.05f?fallback:motion::alignDirections({0,0,1},flat);
}
inline std::vector<Pose> globals(const std::vector<Joint>& joints,const std::vector<uint16_t>& parents){
 if(joints.size()!=parents.size()||joints.empty())throw std::invalid_argument("body skeleton size");
 std::vector<Pose> out(joints.size());
 for(size_t i=0;i<joints.size();i++){if(parents[i]!=UINT16_MAX&&parents[i]>=i)throw std::invalid_argument("body skeleton order");
  out[i]=parents[i]==UINT16_MAX?pose(joints[i]):compose(out[parents[i]],pose(joints[i]));}
 return out;
}
// The animation's legs now, from the hips and without the animation's heading (what Targets::legsHeld takes).
inline std::vector<Pose> legsNow(const std::vector<Joint>& animated,const std::vector<Joint>& rest,const std::vector<uint16_t>& parents,const Skeleton& k={}){
 const auto R=globals(rest,parents),A=globals(animated,parents);
 if(k.hips>=A.size())throw std::invalid_argument("body ik joint");
 const auto un=inverse(levelHeading(rotate(multiply(A[k.hips].orientation,inverse(R[k.hips].orientation)),{0,0,1}),{0,0,0,1}));
 std::vector<Pose> out(A.size());
 for(size_t j=0;j<A.size();j++)out[j]={normalized(multiply(un,A[j].orientation)),rotate(un,A[j].position-A[k.hips].position)};
 return out;
}
inline std::vector<Joint> solve(const std::vector<Joint>& animated,const std::vector<Joint>& rest,const std::vector<uint16_t>& parents,
                                const Targets& t,const Skeleton& k={},const Settings& s={},Report* report=nullptr){
 if(animated.size()!=rest.size()||!finite(t.head.position)||(t.hand[0]&&!finite(t.socket[0].position))||(t.hand[1]&&!finite(t.socket[1].position)))throw std::invalid_argument("body ik input");
 const auto R=globals(rest,parents),A=globals(animated,parents);const size_t count=rest.size();
 const auto known=[&](uint16_t j){if(j>=count)throw std::invalid_argument("body ik joint");};
 known(k.hips);known(k.chest);known(k.neck);known(k.head);for(auto j:k.spine)known(j);for(auto j:k.upper)known(j);
 for(const auto* side:{&k.shoulder,&k.upperTwist,&k.elbow,&k.foreTwist,&k.wrist,&k.socket,&k.elbowHelper,&k.thigh,&k.knee,&k.ankle,&k.kneeHelper})for(auto j:*side)known(j);
 std::vector<char> set(count,0);std::vector<Pose> wanted(count);
 auto put=[&](uint16_t j,Pose p){if(j>=count||!finite(p.position))throw std::invalid_argument("body ik joint");wanted[j]={normalized(p.orientation),p.position};set[j]=1;};
 Report r;
 // 1. Heading: the headset's right vector stays level through any pitch; rolled onto its side, the view direction is used.
 const auto animatedYaw=levelHeading(rotate(multiply(A[k.hips].orientation,inverse(R[k.hips].orientation)),{0,0,1}),{0,0,0,1});
 const auto right=rotate(t.head.orientation,{1,0,0});
 const auto yaw=length(Vec3{right.x,0,right.z})>.3f?levelHeading({-right.z,0,right.x},animatedYaw):levelHeading(rotate(t.head.orientation,{0,0,1}),animatedYaw);
 const auto turned=[&](Vec3 v){return rotate(yaw,v);};
 // 2. Head joint from the headset; the chest block hangs rigidly from the neck pivot, the neck points at the head.
 const Pose head{multiply(t.head.orientation,R[k.head].orientation),t.head.position-rotate(t.head.orientation,s.eyeInHead)};
 const auto pivot=t.head.position-rotate(t.head.orientation,s.eyeFromPivot),restPivot=R[k.head].position+s.eyeInHead-s.eyeFromPivot;
 const auto chest=pivot+turned(R[k.chest].position-restPivot);
 const auto onChest=[&](uint16_t j){return Pose{multiply(yaw,R[j].orientation),chest+turned(R[j].position-R[k.chest].position)};};
 put(k.head,head);put(k.chest,onChest(k.chest));for(auto j:k.upper)put(j,onChest(j));
 {auto neck=onChest(k.neck);const auto was=turned(R[k.head].position-R[k.neck].position),now=head.position-neck.position;
  if(length(was)>1e-4f&&length(now)>1e-4f)neck.orientation=multiply(motion::alignDirections(was,now),neck.orientation);put(k.neck,neck);}
 // 3. Hips under the neck pivot at the animated height, as far as the spine allows; the spine joints spread between.
 const float torso=R[k.chest].position.y-R[k.hips].position.y;if(!(torso>.05f))throw std::invalid_argument("body ik torso");
 auto hips=pivot+turned(R[k.hips].position-restPivot);
 hips.y=std::clamp(A[k.hips].position.y,chest.y-torso,chest.y-torso*(1-std::clamp(s.torsoCompress,0.f,.9f)));
 // In the water the animated pelvis (and with it the legs) is turned to the body's heading about the model's own
 // vertical, as the legs are on land: the heading is the headset's. The swim animation lies along the model's +Z
 // whatever its hips do (they face down), so the turn is the heading itself. Run game-20261014-004300 (the character
 // steered by keyboard, the view by the script, up to 160 degrees apart): the body swam where the keys said and its
 // legs, level behind the hips, came round in front of the eyes - "pieces of the armour at the lower edge".
 if(t.swim){const auto reach=turned(A[k.hips].position)-chest;
  hips=chest+unit(reach,{0,-1,0})*std::clamp(length(reach),torso*(1-std::clamp(s.torsoCompress,0.f,.9f)),torso);}
 if(t.hipsGiven){if(!finite(t.hips.position))throw std::invalid_argument("body ik hips");hips=t.hips.position;}
 r.hipsBelowAnimated=A[k.hips].position.y-hips.y;r.torso=t.swim?length(chest-hips)/torso:(chest.y-hips.y)/torso;r.pivot=pivot;
 const bool rigid=t.swim||t.hipsGiven;
 const Pose rigidHips{t.hipsGiven?normalized(t.hips.orientation):multiply(yaw,A[k.hips].orientation),hips};
 const auto onHips=[&](uint16_t j){return rigid?compose(rigidHips,compose(inverse(R[k.hips]),R[j])):Pose{multiply(yaw,R[j].orientation),hips+turned(R[j].position-R[k.hips].position)};};
 put(k.hips,onHips(k.hips));
 for(auto j:k.spine){const float f=std::clamp((R[j].position.y-R[k.hips].position.y)/torso,0.f,1.f);const auto low=onHips(j),high=onChest(j);
  put(j,{t.swim?blend(low.orientation,high.orientation,f):low.orientation,low.position+(high.position-low.position)*f});}
 // 4. Arms: shoulder -> elbow -> wrist, the elbow down, a little back and outward. The hand socket lands on the grip;
 //    beyond reach the shoulder first goes toward the hand, then the forearm stretches rather than leaving the hand
 //    behind. Half of the wrist's twist goes to the forearm.
 for(unsigned h=0;h<2;h++){
  if(!t.hand[h])continue;
  auto S=onChest(k.shoulder[h]).position;const float side=h?1.f:-1.f;
  const auto W=compose(t.socket[h],inverse(compose(inverse(R[k.wrist[h]]),R[k.socket[h]])));
  const float a=length(R[k.elbow[h]].position-R[k.shoulder[h]].position),b=length(R[k.wrist[h]].position-R[k.elbow[h]].position);
  S=S+unit(W.position-S,{0,0,1})*std::clamp(length(W.position-S)-(a+b)+1e-3f,0.f,std::max(s.shoulderReach,0.f));
  const auto pole=turned({side*.35f,-1,-.6f});
  const auto E=twoBoneMiddle(S,W.position,a,b,pole);
  const auto u0=turned(unit(R[k.elbow[h]].position-R[k.shoulder[h]].position,{side,0,0})),f0=turned(unit(R[k.wrist[h]].position-R[k.elbow[h]].position,{side,0,0}));
  const auto u1=unit(E-S,u0),f1=unit(W.position-E,f0);
  auto n0=cross(u0,f0);if(length(n0)<1e-3f)n0=cross(turned({0,0,-1}),u0);
  auto n1=cross(u1,f1);if(length(n1)<1e-3f)n1=cross(pole,unit(W.position-S,u1));
  const auto upper=alignFrames(u0,n0,u1,n1),fore=alignFrames(f0,n0,f1,n1);
  const auto rested=[&](uint16_t j,Quat q){return multiply(q,multiply(yaw,R[j].orientation));};
  put(k.shoulder[h],{rested(k.shoulder[h],upper),S});
  put(k.upperTwist[h],{rested(k.upperTwist[h],upper),S+u1*length(R[k.upperTwist[h]].position-R[k.shoulder[h]].position)});
  put(k.elbow[h],{rested(k.elbow[h],fore),E});put(k.elbowHelper[h],{rested(k.elbowHelper[h],fore),E});
  const auto twist=halfTwist(multiply(W.orientation,inverse(rested(k.wrist[h],fore))),f1);
  put(k.foreTwist[h],{multiply(twist,rested(k.foreTwist[h],fore)),E+f1*length(R[k.foreTwist[h]].position-R[k.elbow[h]].position)});
  put(k.wrist[h],W);put(k.socket[h],t.socket[h]);
  r.armStretch[h]=std::max(0.f,length(W.position-S)-(a+b));
 }
 // 5. Legs: the animated leg, turned to the body's heading, under the level hips; each foot keeps its animated height.
 //    legsRest: toward the rest pose's leg, hanging from the hips (its foot that far below them).
 const auto yawDelta=normalized(multiply(yaw,inverse(animatedYaw)));
 const float hang=std::clamp(t.legsRest,0.f,1.f);
 const float swing=std::isfinite(t.legsBack)?std::clamp(t.legsBack,0.f,1.6f):0.f;
 const Quat back{std::sin(swing*.5f),0,0,std::cos(swing*.5f)},backNow{std::sin(swing*hang*.5f),0,0,std::cos(swing*hang*.5f)};   // about the body's right: down turns to behind
 const float hw=t.legsHeld&&t.legsHeld->size()==count&&std::isfinite(t.heldWeight)?std::clamp(t.heldWeight,0.f,1.f):0.f;
 const auto restLeg=[&](uint16_t j){const auto r=rotate(back,R[j].position-R[k.hips].position);return hw>0?r+((*t.legsHeld)[j].position-r)*hw:r;};
 const auto restTurn=[&](uint16_t j){const auto r=multiply(back,R[j].orientation);return hw>0?blend(r,(*t.legsHeld)[j].orientation,hw):r;};
 const auto under=[&](uint16_t j){const auto a=rotate(yawDelta,A[j].position-A[k.hips].position);
  return hang>0?a+(turned(restLeg(j))-a)*hang:a;};
 for(unsigned leg=0;leg<2&&!t.swim;leg++){
  const auto T0=under(k.thigh[leg]),K0=under(k.knee[leg]),F0=under(k.ankle[leg]);
  const float a=length(R[k.knee[leg]].position-R[k.thigh[leg]].position),b=length(R[k.ankle[leg]].position-R[k.knee[leg]].position); // rest lengths: an animation may stretch a bone
  const auto T=onHips(k.thigh[leg]).position;
  Vec3 target{hips.x+F0.x,A[k.ankle[leg]].position.y,hips.z+F0.z};
  if(hang>0)target.y+=(hips.y+restLeg(k.ankle[leg]).y-target.y)*hang;
  // Hips above the animation (it crouches or lunges, the player stands): the foot stays on the ground and comes in
  // under the hip as far as the leg needs; only a leg too short even straight down leaves the ground.
  const float reach=a+b-1e-3f,down=T.y-target.y;
  if(length(target-T)>reach){const Vec3 out{target.x-T.x,0,target.z-T.z};
   if(down>0&&down<reach)target=Vec3{T.x,target.y,T.z}+unit(out,{0,0,1})*std::sqrt(reach*reach-down*down);
   else{r.legShort[leg]=std::max(down-reach,0.f);target=T+Vec3{0,-reach,0};}}
  const auto bend=K0-(T0+F0)*.5f;                                    // a straight animated leg gives no direction: forward then
  const auto pole=bend+turned(rotate(backNow,{0,0,s.kneeForward}))*std::clamp(1-length(bend)/.03f,0.f,1.f);
  const auto knee=twoBoneMiddle(T,target,a,b,pole);
  const auto u0=unit(K0-T0,{0,-1,0}),f0=unit(F0-K0,{0,-1,0}),u1=unit(knee-T,u0),f1=unit(target-knee,f0);
  auto n0=cross(u0,f0);if(length(n0)<1e-3f)n0=cross(pole,unit(F0-T0,u0));
  auto n1=cross(u1,f1);if(length(n1)<1e-3f)n1=cross(pole,unit(target-T,u1));
  const auto from=[&](uint16_t j,Quat q){const auto animatedBase=multiply(yawDelta,A[j].orientation);
   return multiply(q,hang>0?blend(animatedBase,multiply(yaw,restTurn(j)),hang):animatedBase);};
  const auto shin=alignFrames(f0,n0,f1,n1);
  put(k.thigh[leg],{from(k.thigh[leg],alignFrames(u0,n0,u1,n1)),T});
  put(k.knee[leg],{from(k.knee[leg],shin),knee});put(k.kneeHelper[leg],{from(k.kneeHelper[leg],{0,0,0,1}),knee});
  put(k.ankle[leg],{from(k.ankle[leg],{0,0,0,1}),target});
 }
 // 6. Back to parent-relative joints; everything not set keeps its animated local pose under its new parent.
 auto out=animated;std::vector<Pose> finalG(count);
 for(size_t i=0;i<count;i++){
  const auto p=parents[i];
  if(set[i]){const auto local=p==UINT16_MAX?wanted[i]:compose(inverse(finalG[p]),wanted[i]);out[i].orientation=normalized(local.orientation);out[i].position=local.position;finalG[i]=wanted[i];}
  else finalG[i]=p==UINT16_MAX?pose(out[i]):compose(finalG[p],pose(out[i]));
 }
 for(unsigned h=0;h<2;h++)if(t.hand[h])r.socketError[h]=length(finalG[k.socket[h]].position-t.socket[h].position);
 r.hips=finalG[k.hips];
 if(report)*report=r;
 return out;
}
} // namespace evr::bodyik
