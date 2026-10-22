#include <evr/body_ik.hpp>
#include "player_skeleton_fixture.hpp"
#include <iostream>
#include <string>
using namespace evr;using namespace evr::bodyik;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 auto near=[](Vec3 a,Vec3 b,float e=1e-4f){return length(a-b)<=e;};
 auto angle=[](Quat a,Quat b){a=normalized(a);b=normalized(b);return 2*std::acos(std::min(1.f,std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w)))*180/pi;};
 auto turnY=[](float degrees){const float h=degrees*pi/360;return Quat{0,std::sin(h),0,std::cos(h)};};    // +Z toward +X
 auto pitchDown=[](float degrees){const float h=degrees*pi/360;return Quat{std::sin(h),0,0,std::cos(h)};};
 std::vector<hand::Joint> rest;std::vector<uint16_t> parents;fixture::playerSkeleton(rest,parents);
 const auto R=globals(rest,parents);const Skeleton k;const Settings s;
 const float upperArm=length(R[42].position-R[40].position),foreArm=length(R[44].position-R[42].position);
 const Vec3 eye=R[17].position+s.eyeInHead;ck(near(eye,{0,1.7233f,.0892f},2e-3f));  // the rest pose looks along +Z
 const Targets atRest{{{0,0,0,1},eye},{R[64],R[94]}};
 auto below=[&](size_t j,uint16_t root){for(auto p=uint16_t(j);p!=UINT16_MAX;p=parents[p])if(p==root)return true;return false;};

 // The middle joint: both lengths hold, it bends toward the pole, and a target out of reach gives the straight chain.
 {const auto m=twoBoneMiddle({0,1,0},{0,.4f,0},.4f,.3f,{0,0,1});ck(std::abs(length(m-Vec3{0,1,0})-.4f)<1e-5f&&std::abs(length(m-Vec3{0,.4f,0})-.3f)<1e-5f&&m.z>.1f&&std::abs(m.x)<1e-5f);
  const auto far=twoBoneMiddle({0,1,0},{0,-1,0},.4f,.3f,{0,0,1});ck(near(far,{0,.6f,0},2e-2f));
  bool threw=false;try{twoBoneMiddle({0,1,0},{0,0,0},0,.3f,{0,0,1});}catch(const std::invalid_argument&){threw=true;}ck(threw);}

 // Rest animation, headset at the rest eyes, hands at the rest sockets: the body stands as in the rest pose.
 Report report;const auto standing=solve(rest,rest,parents,atRest,k,s,&report);const auto G=globals(standing,parents);
 ck(report.socketError[0]<1e-4f&&report.socketError[1]<1e-4f&&report.armStretch[0]==0&&report.armStretch[1]==0);
 ck(std::abs(report.torso-1)<1e-4f&&std::abs(report.hipsBelowAnimated)<1e-4f&&report.legShort[0]==0&&report.legShort[1]==0);
 for(auto j:{1,10,11,12,13,14,15,16,17,39,69,40,70,44,74,2,3,4,5,6,7,8,9,20})ck(near(G[size_t(j)].position,R[size_t(j)].position,2e-4f));
 for(auto j:{1,12,15,16,17,2,3,4,6,7,8,44,74})ck(angle(G[size_t(j)].orientation,R[size_t(j)].orientation)<.05f);
 for(unsigned h=0;h<2;h++){ck(std::abs(length(G[k.elbow[h]].position-G[k.shoulder[h]].position)-upperArm)<1e-4f);
  ck(std::abs(length(G[k.wrist[h]].position-G[k.elbow[h]].position)-foreArm)<1e-4f);ck(near(G[k.elbowHelper[h]].position,G[k.elbow[h]].position));}
 // Fingers, face and toes are the animation's: their parent-relative joints are untouched.
 for(size_t j=0;j<rest.size();j++)if((j>=45&&j<=68&&j!=64)||(j>=75&&j<=97&&j!=94)||(j>=18&&j<=38)||j==5||j==9||j==0)
  ck(near(standing[j].position,rest[j].position,0)&&angle(standing[j].orientation,rest[j].orientation)==0);

 // The game's own camera height (1.65 m, below the model's eyes): the spine takes it, hips and legs stay.
 {Targets t=atRest;t.head.position.y=1.65f;Report r;const auto g=globals(solve(rest,rest,parents,t,k,s,&r),parents);
  ck(near(g[1].position,R[1].position)&&near(g[3].position,R[3].position,2e-4f)&&near(g[8].position,R[8].position,2e-4f));
  ck(r.torso>.78f&&r.torso<.83f&&std::abs(g[17].position.y-(1.65f-s.eyeInHead.y))<1e-5f);
  ck(g[12].position.y<R[12].position.y&&g[12].position.y>g[11].position.y&&g[15].position.y>g[14].position.y);
  ck(near(g[40].position-g[17].position,R[40].position-R[17].position));}

 // Crouching in the room: the hips go down once the spine is at its shortest, the feet stay, the knees go forward.
 {Targets t=atRest;t.head.position.y-=.4f;for(auto& hand:t.socket)hand.position.y-=.4f;Report r;const auto g=globals(solve(rest,rest,parents,t,k,s,&r),parents);
  ck(std::abs(r.torso-.75f)<1e-4f&&r.hipsBelowAnimated>.3f&&r.hipsBelowAnimated<.31f&&r.legShort[0]==0);
  for(unsigned leg=0;leg<2;leg++){
   ck(near(g[k.ankle[leg]].position,R[k.ankle[leg]].position,2e-4f)&&g[k.knee[leg]].position.z>R[k.knee[leg]].position.z+.15f);
   ck(std::abs(length(g[k.knee[leg]].position-g[k.thigh[leg]].position)-length(R[k.knee[leg]].position-R[k.thigh[leg]].position))<1e-4f);
   ck(std::abs(length(g[k.ankle[leg]].position-g[k.knee[leg]].position)-length(R[k.ankle[leg]].position-R[k.knee[leg]].position))<1e-4f);
   ck(angle(g[k.ankle[leg]].orientation,R[k.ankle[leg]].orientation)<.05f&&near(g[k.kneeHelper[leg]].position,g[k.knee[leg]].position));}
  ck(r.socketError[0]<1e-4f&&r.socketError[1]<1e-4f);}

 // Headset and hands turned together about the vertical: the whole body turns with them.
 for(float degrees:{90.f,-135.f,180.f}){const auto q=turnY(degrees);Targets t{{q,rotate(q,eye)},{Pose{multiply(q,R[64].orientation),rotate(q,R[64].position)},Pose{multiply(q,R[94].orientation),rotate(q,R[94].position)}}};
  const auto g=globals(solve(rest,rest,parents,t),parents);
  for(size_t j=1;j<rest.size();j++)if(below(j,1)){ck(near(g[j].position,rotate(q,G[j].position),5e-4f));ck(angle(g[j].orientation,multiply(q,G[j].orientation))<.1f);}}

 // Looking down or straight down does not turn the body; the head joint pitches about the eyes' position.
 for(float degrees:{60.f,90.f,-80.f}){Targets t=atRest;t.head.orientation=pitchDown(degrees);const auto g=globals(solve(rest,rest,parents,t),parents);
  ck(angle(g[1].orientation,R[1].orientation)<.05f&&near(g[69].position-g[39].position,R[69].position-R[39].position)&&angle(g[39].orientation,R[39].orientation)<.05f);
  ck(near(g[17].position,eye-rotate(pitchDown(degrees),s.eyeInHead))&&angle(g[17].orientation,multiply(pitchDown(degrees),R[17].orientation))<.05f);
  // The chest hangs from the neck pivot, not from the head joint; the neck stays on the chest and points at the head.
  const auto pivot=eye-rotate(pitchDown(degrees),s.eyeFromPivot),restPivot=eye-s.eyeFromPivot;
  for(auto j:{15,16,39,69,1})ck(near(g[size_t(j)].position,pivot+(R[size_t(j)].position-restPivot)));
  ck(dot(unit(g[17].position-g[16].position,{}),rotate(multiply(g[16].orientation,inverse(R[16].orientation)),unit(R[17].position-R[16].position,{})))>.9999f);}
 // Looking down about the neck, as a person does (the eyes go forward and down): chest, shoulders and hips stay.
 for(float degrees:{30.f,70.f,-40.f}){Targets t=atRest;t.head.orientation=pitchDown(degrees);t.head.position=eye-s.eyeFromPivot+rotate(pitchDown(degrees),s.eyeFromPivot);
  Report r;const auto g=globals(solve(rest,rest,parents,t,k,s,&r),parents);
  for(auto j:{1,10,12,15,16,39,69,2,3,4,6,7,8})ck(near(g[size_t(j)].position,G[size_t(j)].position,2e-4f));
  ck(near(r.pivot,eye-s.eyeFromPivot)&&r.legShort[0]==0&&std::abs(r.torso-1)<1e-4f&&!near(g[17].position,G[17].position,.02f));}

 // A hand in front of the chest: the elbow hangs below and outside the line from shoulder to wrist.
 {Targets t=atRest;t.socket[1].position={.2f,1.3f,.35f};t.socket[0].position={-.2f,1.3f,.35f};Report r;const auto g=globals(solve(rest,rest,parents,t,k,s,&r),parents);
  for(unsigned h=0;h<2;h++){const auto S=g[k.shoulder[h]].position,W=g[k.wrist[h]].position,E=g[k.elbow[h]].position;const auto line=unit(W-S,{0,0,1});
   const auto off=E-(S+line*dot(E-S,line));ck(off.y<-.03f&&(h?off.x>0:off.x<0));
   ck(std::abs(length(E-S)-upperArm)<1e-4f&&std::abs(length(W-E)-foreArm)<1e-4f&&r.socketError[h]<1e-4f&&r.armStretch[h]==0);
   ck(near(g[k.upperTwist[h]].position,S+unit(E-S,{0,0,1})*length(R[k.upperTwist[h]].position-R[k.shoulder[h]].position)));
   ck(near(g[k.foreTwist[h]].position,E+unit(W-E,{0,0,1})*length(R[k.foreTwist[h]].position-R[k.elbow[h]].position)));}
  ck(near(g[40].position,G[40].position)&&near(g[1].position,G[1].position));}                // hands do not pull the body

 // Beyond reach the hand still arrives: the arm is straight and the forearm takes the stretch.
 {Targets t=atRest;t.socket[1].position={.3f,1.4f,2};Report r;const auto g=globals(solve(rest,rest,parents,t,k,s,&r),parents);
  ck(r.socketError[1]<1e-4f&&r.armStretch[1]>1&&r.armStretch[0]==0&&std::abs(length(g[72].position-g[70].position)-upperArm)<1e-4f);
  ck(dot(unit(g[72].position-g[70].position,{}),unit(g[74].position-g[70].position,{}))>.999f);
  // The shoulder goes toward the hand first (6 cm), and only that one.
  ck(std::abs(length(g[70].position-G[70].position)-s.shoulderReach)<1e-4f&&dot(unit(g[70].position-G[70].position,{}),unit(g[74].position-G[70].position,{}))>.99f&&near(g[40].position,G[40].position));
  // Just past the arm's length: the shoulder takes it all, nothing stretches.
  const auto way=unit(Vec3{.3f,-.1f,1},{});Targets edge=atRest;const Pose wrist{G[74].orientation,G[70].position+way*(upperArm+foreArm+.02f)};edge.socket[1]=compose(wrist,compose(inverse(R[74]),R[94]));
  Report e;const auto ge=globals(solve(rest,rest,parents,edge,k,s,&e),parents);
  ck(e.armStretch[1]==0&&e.socketError[1]<1e-4f&&std::abs(length(ge[74].position-ge[72].position)-foreArm)<1e-4f&&length(ge[70].position-G[70].position)>.02f&&length(ge[70].position-G[70].position)<.045f);}

 // Half of a turn of the hand about the forearm goes to the forearm joint.
 {const auto f=unit(G[74].position-G[72].position,{});const float h=45*pi/180;const Quat quarter{f.x*std::sin(h),f.y*std::sin(h),f.z*std::sin(h),std::cos(h)};
  const Pose wrist{multiply(quarter,G[74].orientation),G[74].position};Targets t=atRest;t.socket[1]=compose(wrist,compose(inverse(R[74]),R[94]));
  Report r;const auto g=globals(solve(rest,rest,parents,t,k,s,&r),parents);
  ck(r.socketError[1]<1e-4f&&near(g[72].position,G[72].position)&&angle(g[72].orientation,G[72].orientation)<.05f);
  ck(std::abs(angle(g[73].orientation,G[73].orientation)-45)<.5f&&std::abs(angle(g[74].orientation,G[74].orientation)-90)<.1f);}

 // The walk animation's sway does not reach the tracked upper body; the hips keep its height while the spine allows.
 {auto walk=rest;const auto lean=normalized(Quat{.1f,.05f,.12f,1});
  walk[1].orientation=normalized(multiply(lean,rest[1].orientation));walk[1].position.y+=.03f;
  walk[12].orientation=normalized(multiply(normalized(Quat{.15f,0,.1f,1}),rest[12].orientation));
  walk[40].orientation=normalized(multiply(normalized(Quat{0,.3f,.2f,1}),rest[40].orientation));
  walk[94].position=walk[94].position+Vec3{.02f,.03f,-.01f};walk[94].orientation=normalized(multiply(normalized(Quat{.2f,0,.1f,1}),rest[94].orientation)); // the animation moves the socket in the hand
  const auto W=globals(walk,parents);Report r;const auto g=globals(solve(walk,rest,parents,atRest,k,s,&r),parents);
  for(auto j:{15,16,17,39,69,40,70,42,72,44,74,64,94})ck(near(g[size_t(j)].position,G[size_t(j)].position,2e-4f));
  ck(std::abs(g[1].position.y-(R[1].position.y+.03f))<1e-5f&&near({g[1].position.x,0,g[1].position.z},{R[1].position.x,0,R[1].position.z}));
  ck(angle(g[1].orientation,multiply(levelHeading(rotate(lean,{0,0,1}),{0,0,0,1}),R[1].orientation))>1); // not the animated heading: the headset's
  ck(angle(g[1].orientation,R[1].orientation)<.05f&&std::abs(r.hipsBelowAnimated)<1e-5f);
  for(unsigned leg=0;leg<2;leg++)ck(std::abs(g[k.ankle[leg]].position.y-W[k.ankle[leg]].position.y)<2e-3f||r.legShort[leg]>0);
  // Animated hips below what the spine can reach (it never lengthens): the hips stay up.
  ck(r.socketError[0]<1e-4f&&r.socketError[1]<1e-4f&&angle(g[94].orientation,R[94].orientation)<.05f);
  walk[1].position.y=rest[1].position.y-.2f;Report low;const auto d=globals(solve(walk,rest,parents,atRest,k,s,&low),parents);
  ck(std::abs(d[1].position.y-R[1].position.y)<1e-5f&&std::abs(low.torso-1)<1e-5f&&low.hipsBelowAnimated<-.19f);}

 // The animation lunges (hips low, feet far apart) while the player stands: the feet stay on the ground and come in.
 {auto lunge=rest;lunge[1].position.y-=.3f;                                   // the whole animated body 30 cm down ...
  auto L=globals(lunge,parents);
  for(unsigned leg=0;leg<2;leg++){                                              // ... and its feet back up on the ground, 45 cm out
   const Vec3 foot{R[k.ankle[leg]].position.x+(leg?.45f:-.45f),R[k.ankle[leg]].position.y,R[k.ankle[leg]].position.z};
   const auto local=compose(inverse(L[k.knee[leg]]),Pose{L[k.ankle[leg]].orientation,foot});lunge[k.ankle[leg]].position=local.position;}
  L=globals(lunge,parents);Report r;const auto g=globals(solve(lunge,rest,parents,atRest,k,s,&r),parents);
  ck(std::abs(g[1].position.y-R[1].position.y)<1e-5f&&r.legShort[0]==0&&r.legShort[1]==0);
  for(unsigned leg=0;leg<2;leg++){const auto foot=g[k.ankle[leg]].position,hip=g[k.thigh[leg]].position;
   ck(std::abs(foot.y-L[k.ankle[leg]].position.y)<1e-4f&&std::abs(foot.x)<std::abs(L[k.ankle[leg]].position.x)-.1f&&(leg?foot.x>hip.x:foot.x<hip.x));
   ck(length(foot-hip)<length(R[k.knee[leg]].position-R[k.thigh[leg]].position)+length(R[k.ankle[leg]].position-R[k.knee[leg]].position));}
  // Standing on a box in the room (headset 40 cm up): no leg is that long, the feet leave the ground.
  Targets high=atRest;high.head.position.y+=.4f;Report up;const auto u=globals(solve(rest,rest,parents,high,k,s,&up),parents);
  ck(up.legShort[0]>.3f&&up.legShort[1]>.3f&&u[4].position.y>R[4].position.y+.3f&&std::abs(u[4].position.x-u[2].position.x)<1e-4f);}

 // A hand that is not tracked: that arm is the animation's, hanging from the tracked chest; the other arm is solved.
 {Targets t=atRest;t.hand[0]=false;t.socket[0].position={NAN,NAN,NAN};t.socket[1].position={.2f,1.3f,.35f};Report r;const auto out=solve(rest,rest,parents,t,k,s,&r);const auto g=globals(out,parents);
  for(auto j:{40,41,42,43,44,64})ck(near(out[size_t(j)].position,rest[size_t(j)].position,0)&&angle(out[size_t(j)].orientation,rest[size_t(j)].orientation)==0);
  ck(near(g[44].position,R[44].position,2e-4f)&&r.socketError[0]==0&&r.socketError[1]<1e-4f&&near(g[94].position,{.2f,1.3f,.35f}));}

 // A leaning model (the game pitches the whole character while it swims): the turn about +Y and the rest come apart
 // and go together again, and the animation of a model leaned by L, stood upright, solved and leaned back is the
 // upright solution as the leaning model shows it.
 {const auto L=normalized(multiply(pitchDown(55),Quat{0,0,std::sin(.1f),std::cos(.1f)}));
  for(float degrees:{0.f,40.f,-130.f,179.f}){const auto q=normalized(multiply(turnY(degrees),L));
   ck(angle(multiply(uprightPart(q),leanPart(q)),q)<.02f&&std::abs(uprightPart(q).x)+std::abs(uprightPart(q).z)==0&&std::abs(leanPart(q).y)<1e-4f);
   ck(angle(uprightPart(turnY(degrees)),turnY(degrees))<.02f&&leanAngle(turnY(degrees))<1e-3f&&std::abs(leanAngle(q)*180/pi-angle(L,{0,0,0,1}))<1);}
  auto model=rest;lean(model,parents,inverse(L));                  // what the game holds: the rest pose in a model leaned by L
  ck(!near(globals(model,parents)[17].position,R[17].position,.2f));
  auto animated=model;lean(animated,parents,L);                    // stood upright: the rest pose again
  for(size_t j=0;j<rest.size();j++)ck(near(globals(animated,parents)[j].position,R[j].position,2e-4f));
  auto out=solve(animated,rest,parents,atRest,k,s);lean(out,parents,inverse(L));
  const auto shown=globals(out,parents);                           // the model's transform turns these by L
  for(auto j:{1,12,15,17,40,42,44,64,94,2,3,4,6,7,8})ck(near(rotate(L,shown[size_t(j)].position),G[size_t(j)].position,3e-4f)&&angle(multiply(L,shown[size_t(j)].orientation),G[size_t(j)].orientation)<.06f);
  bool threw=false;auto few=parents;few.pop_back();try{lean(model,few,L);}catch(const std::invalid_argument&){threw=true;}ck(threw);}

 // Swimming: the animation lies face down, hips low and behind. Head, chest and arms are the player's as on land; the
 // hips go toward the animated hips at the spine's length and take the animated orientation; the legs are not solved.
 {auto swim=rest;swim[1].orientation=normalized(multiply(pitchDown(80),rest[1].orientation));swim[1].position={.05f,.75f,-.35f};
  swim[3].orientation=normalized(multiply(pitchDown(-25),rest[3].orientation));
  const auto W=globals(swim,parents);Targets t=atRest;t.swim=true;Report r;const auto out=solve(swim,rest,parents,t,k,s,&r);const auto g=globals(out,parents);
  const float torso=R[15].position.y-R[1].position.y;
  for(auto j:{15,16,17,39,69,40,70,42,72,44,74,64,94})ck(near(g[size_t(j)].position,G[size_t(j)].position,2e-4f));
  ck(r.socketError[0]<1e-4f&&r.socketError[1]<1e-4f);
  const float spine=length(g[1].position-g[15].position);
  ck(spine<=torso+1e-4f&&spine>=torso*(1-s.torsoCompress)-1e-4f&&std::abs(r.torso-spine/torso)<1e-4f&&dot(unit(g[1].position-g[15].position,{}),unit(W[1].position-g[15].position,{}))>.9999f);
  ck(angle(g[1].orientation,W[1].orientation)<.05f);
  for(auto j:{2,3,4,5,6,7,8,9,100,101})ck(near(out[size_t(j)].position,swim[size_t(j)].position,0)&&angle(out[size_t(j)].orientation,swim[size_t(j)].orientation)==0);
  for(auto j:{2,3,4,6,7,8})ck(angle(g[size_t(j)].orientation,W[size_t(j)].orientation)<.05f&&near(g[size_t(j)].position-g[1].position,W[size_t(j)].position-W[1].position,2e-4f));
  for(auto j:k.spine){const auto p=g[j].position;ck(finite(p)&&length(p-g[1].position)<=spine+.05f&&length(p-g[15].position)<=spine+.25f);}
  // The player looks another way than the character swims: the body is the same about the headset - the animated
  // pelvis and legs turn with the heading, and the legs are as far along it as the animation has them along +Z.
  for(float degrees:{90.f,-135.f,180.f}){const auto q=turnY(degrees);Targets turnedView{{q,rotate(q,eye)},{Pose{multiply(q,R[64].orientation),rotate(q,R[64].position)},Pose{multiply(q,R[94].orientation),rotate(q,R[94].position)}}};
   turnedView.swim=true;const auto gq=globals(solve(swim,rest,parents,turnedView),parents);
   for(size_t j=1;j<rest.size();j++)if(below(j,1)){ck(near(gq[j].position,rotate(q,g[j].position),5e-4f));ck(angle(gq[j].orientation,multiply(q,g[j].orientation))<.1f);}
   for(auto j:{3,4,7,8})ck(std::abs(dot(gq[size_t(j)].position-gq[1].position,rotate(q,{0,0,1}))-(W[size_t(j)].position.z-W[1].position.z))<2e-3f);}
  // Animated hips the spine reaches: the hips are exactly the animation's.
  auto close=swim;const Vec3 within=G[15].position+unit(Vec3{.1f,-1,-.3f},{})*(torso*.9f);
  close[1].position=compose(inverse(globals(close,parents)[parents[1]]),Pose{{0,0,0,1},within}).position;
  Report c;const auto gc=globals(solve(close,rest,parents,t,k,s,&c),parents);
  ck(near(globals(close,parents)[1].position,within,1e-5f)&&near(gc[1].position,within,1e-4f)&&std::abs(c.torso-.9f)<1e-3f);
  // On land the same animation is solved the standing way: upright hips, legs solved.
  Targets land=atRest;const auto solved=solve(swim,rest,parents,land,k,s);const auto gl=globals(solved,parents);
  ck(std::abs(gl[1].position.x-G[1].position.x)<1e-4f&&std::abs(gl[1].position.z-G[1].position.z)<1e-4f&&angle(gl[1].orientation,R[1].orientation)<.05f&&angle(solved[2].orientation,swim[2].orientation)>1);}

 // Another player's body: head, hands and the pelvis its owner's solve gave. The same animation gives the same body
 // again, on land and in the water; another animation (the walk cycle is at another place) changes the legs only.
 {auto walk=rest;walk[1].orientation=normalized(multiply(normalized(Quat{.1f,.05f,.12f,1}),rest[1].orientation));walk[1].position.y-=.04f;
  walk[3].orientation=normalized(multiply(pitchDown(-20),rest[3].orientation));
  Targets t=atRest;t.head.orientation=turnY(35);t.head.position.y-=.2f;t.socket[1].position={.25f,1.2f,.3f};
  auto swim=rest;swim[1].orientation=normalized(multiply(pitchDown(80),rest[1].orientation));swim[1].position={.05f,.75f,-.35f};
  for(bool water:{false,true}){t.swim=water;const auto& animated=water?swim:walk;
   Report mine;const auto own=globals(solve(animated,rest,parents,t,k,s,&mine),parents);ck(near(mine.hips.position,own[1].position,0)&&angle(mine.hips.orientation,own[1].orientation)==0);
   Targets other=t;other.hipsGiven=true;other.hips=mine.hips;Report theirs;const auto seen=globals(solve(animated,rest,parents,other,k,s,&theirs),parents);
   for(size_t j=1;j<rest.size();j++)if(below(j,1)){ck(near(seen[j].position,own[j].position,3e-4f));ck(angle(seen[j].orientation,own[j].orientation)<.1f);}
   ck(near(theirs.hips.position,mine.hips.position,1e-5f));
   // The receiver's animation differs: the tracked upper body and the pelvis are the owner's all the same.
   const auto elsewhere=globals(solve(rest,rest,parents,other,k,s),parents);
   for(auto j:{1,15,16,17,39,69,40,70,42,72,44,74,64,94})ck(near(elsewhere[size_t(j)].position,own[size_t(j)].position,3e-4f));
   ck(angle(elsewhere[1].orientation,own[1].orientation)<.1f);}
  Targets bad=atRest;bad.hipsGiven=true;bad.hips.position.x=NAN;bool threw=false;try{solve(rest,rest,parents,bad);}catch(const std::invalid_argument&){threw=true;}ck(threw);}

 // Bad input is refused.
 // The double jump's flip (2026-10-18): the animation tucks the legs - the thighs forward and up, the feet near the hips' height.
 // legsRest 1: the legs hang under the hips as at rest; 0: the animation's tuck; between: the feet between the two.
 {auto tuck=rest;for(unsigned leg=0;leg<2;leg++){const auto j=k.thigh[leg];   // the thigh turned forward and up, about the model's X
   tuck[j].orientation=normalized(multiply(inverse(R[parents[j]].orientation),multiply(pitchDown(-90),R[j].orientation)));}
  const auto T=globals(tuck,parents);ck(T[4].position.y>R[4].position.y+.4f);   // the fixture's tuck lifts the feet
  Targets t=atRest;const auto animatedLegs=globals(solve(tuck,rest,parents,t,k,s),parents);
  t.legsRest=1;const auto hanging=globals(solve(tuck,rest,parents,t,k,s),parents);
  t.legsRest=.5f;const auto half=globals(solve(tuck,rest,parents,t,k,s),parents);
  for(unsigned leg=0;leg<2;leg++){const auto a=k.ankle[leg];
   ck(animatedLegs[a].position.y>R[a].position.y+.3f);                                   // as before: the feet up
   ck(near(hanging[a].position,R[a].position,2e-3f)&&near(hanging[k.knee[leg]].position,R[k.knee[leg]].position,2e-3f));
   ck(angle(hanging[a].orientation,R[a].orientation)<1.f&&angle(hanging[k.thigh[leg]].orientation,R[k.thigh[leg]].orientation)<1.f);
   ck(half[a].position.y<animatedLegs[a].position.y-.05f&&half[a].position.y>hanging[a].position.y+.05f);
   ck(std::abs(length(half[k.knee[leg]].position-half[k.thigh[leg]].position)-length(R[k.knee[leg]].position-R[k.thigh[leg]].position))<1e-4f);}
  // legsBack (the glider): the hanging legs swing back about the body's right - the feet behind the hips and higher, the
  // bones as long as at rest, the knees facing down-forward; 0 is the straight hang above.
  t.legsRest=1;t.legsBack=70*pi/180;const auto swung=globals(solve(tuck,rest,parents,t,k,s),parents);t.legsBack=0;
  for(unsigned leg=0;leg<2;leg++){const auto a=k.ankle[leg],th=k.thigh[leg],kn=k.knee[leg];
   const auto leg0=R[a].position-R[th].position,leg1=swung[a].position-swung[th].position;
   ck(leg1.z<hanging[a].position.z-hanging[th].position.z-.3f&&swung[a].position.y>hanging[a].position.y+.3f);   // behind, up
   ck(std::abs(length(swung[kn].position-swung[th].position)-length(R[kn].position-R[th].position))<1e-4f&&
      std::abs(length(swung[a].position-swung[kn].position)-length(R[a].position-R[kn].position))<1e-4f);
   const float pitch=std::atan2(-leg1.z,-leg1.y)-std::atan2(-leg0.z,-leg0.y);ck(std::abs(pitch*180/pi-70)<3);       // swung by the angle
   ck(angle(swung[a].orientation,normalized(multiply(Quat{std::sin(35*pi/180),0,0,std::cos(35*pi/180)},R[a].orientation)))<2.f);}
  // legsHeld: the hanging legs are the kept ones (the first jump's) - the tuck kept gives the tuck's legs again, as the
  // animation gave them; half weight lies between them and the straight hang.
  {const auto kept=legsNow(tuck,rest,parents,k);t.legsRest=1;t.legsHeld=&kept;t.heldWeight=1;const auto heldLegs=globals(solve(tuck,rest,parents,t,k,s),parents);
   t.heldWeight=.5f;const auto halfHeld=globals(solve(tuck,rest,parents,t,k,s),parents);t.legsHeld=nullptr;t.heldWeight=0;
   for(unsigned leg=0;leg<2;leg++){const auto a=k.ankle[leg];
    ck(near(heldLegs[a].position,animatedLegs[a].position,1e-2f)&&near(heldLegs[k.knee[leg]].position,animatedLegs[k.knee[leg]].position,1e-2f));
    ck(halfHeld[a].position.y>hanging[a].position.y+.05f&&halfHeld[a].position.y<heldLegs[a].position.y-.05f);}}
  for(auto j:{1,15,17,44,74})ck(near(hanging[size_t(j)].position,animatedLegs[size_t(j)].position,1e-4f));}   // the rest of the body as it was
 {Targets t=atRest;t.head.position.x=NAN;bool threw=false;try{solve(rest,rest,parents,t);}catch(const std::invalid_argument&){threw=true;}ck(threw);
  auto shorter=rest;shorter.pop_back();threw=false;try{solve(shorter,rest,parents,atRest);}catch(const std::invalid_argument&){threw=true;}ck(threw);
  Skeleton bad;bad.wrist[0]=400;threw=false;try{solve(rest,rest,parents,atRest,bad);}catch(const std::exception&){threw=true;}ck(threw);}
 std::cout<<"body ik tests passed ("<<n<<")\n";return 0;
}catch(const std::exception& e){std::cerr<<"body ik test failed: "<<e.what()<<"\n";return 1;}}
