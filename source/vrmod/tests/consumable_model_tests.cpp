#include <evr/consumable_model.hpp>
#include <cmath>
#include <iostream>
#include <string>
// The held consumable's model in the right hand (include/evr/consumable_model.hpp): scaled to the hand's size, never
// enlarged, held along the fist as a weapon is, the middle of its bounds in the grip.
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 using namespace evr;using namespace evr::motion;
 const auto near=[](Vec3 a,Vec3 b){return length(a-b)<1e-5f;};
 Pose grip=identityPose();grip.position={.2f,1.1f,-.3f};grip.orientation=normalized(Quat{.1f,.3f,-.2f,.9f});
 // the small healing potion's icon model (resources_manmade_potion_healing_small_01): 15.5 x 31.1 x 15.5 cm, base at 0
 const Vec3 lo{-.077f,.002f,-.077f},hi{.077f,.313f,.077f};
 auto m=mountConsumable(grip,lo,hi,.13f);
 ck(std::abs(m.scale-.13f/.311f)<1e-5f);
 const auto held=mountLiveWeapon(grip);ck(near(rotate(m.pose.orientation,{1,0,0}),rotate(held.orientation,{1,0,0})));   // held as a weapon
 // the middle of the scaled bounds is in the grip
 const auto middle=m.pose.position+rotate(m.pose.orientation,(lo+hi)*(.5f*m.scale));ck(near(middle,grip.position));
 // its +Y (the bottle's neck) goes the way a blade goes out of the fist
 ck(near(rotate(m.pose.orientation,{0,1,0}),rotate(held.orientation,{0,1,0})));
 // a model smaller than the hand's size keeps its own size
 m=mountConsumable(grip,{-.03f,0,-.03f},{.03f,.08f,.03f},.13f);ck(m.scale==1.f);
 // bounds that are not a box are refused
 bool refused=false;try{mountConsumable(grip,hi,lo,.13f);}catch(const std::invalid_argument&){refused=true;}ck(refused);
 // a rotation from its axes
 {const Quat r=normalized(Quat{.3f,-.2f,.5f,.8f});const auto q=quatFromAxes(rotate(r,{1,0,0}),rotate(r,{0,1,0}),rotate(r,{0,0,1}));
  ck(near(rotate(q,{.3f,.7f,-.2f}),rotate(r,{.3f,.7f,-.2f})));
  const Quat r2=normalized(Quat{.9f,.1f,-.2f,.05f});const auto q2=quatFromAxes(rotate(r2,{1,0,0}),rotate(r2,{0,1,0}),rotate(r2,{0,0,1}));ck(near(rotate(q2,{1,2,3}),rotate(r2,{1,2,3})));}
 // a right hand lying palm down, fingers along -Z, the thumb to -X: out of the palm is down
 {const Vec3 wrist{0,0,0},index{-.03f,0,-.09f},middle{0,0,-.095f},little{.04f,0,-.08f};
  const auto palm=palmFrame(wrist,index,middle,little,true,.015f);
  ck(near(rotate(palm.orientation,{0,1,0}),{0,-1,0}));ck(near(rotate(palm.orientation,{0,0,1}),{0,0,1}));ck(near(palm.position,{0,-.015f,-.0475f}));
  // the left hand (thumb to +X) palm down: down as well
  const auto left=palmFrame(wrist,{.03f,0,-.09f},middle,{-.04f,0,-.08f},false,.015f);ck(near(rotate(left.orientation,{0,1,0}),{0,-1,0}));
  // The frame made with right=false (as for the right hand in the game's mirrored space, motion_hand_visual.hpp palmInSocket):
  // +X toward the little finger (motion_consumable_visual.hpp: a heap 2 cm that way); with right=true toward the index.
  ck(rotate(left.orientation,{1,0,0}).x<-.9f);ck(rotate(palm.orientation,{1,0,0}).x<-.9f);
  // popcorn on that palm: its bottom on the skin, its middle over the palm's
  const Vec3 plo{-.141f,-.005f,-.154f},phi{.141f,.178f,.157f};const auto on=mountOnPalm(palm,plo,phi,.13f);
  const auto base=on.pose.position+rotate(on.pose.orientation,Vec3{0,plo.y,(plo.z+phi.z)*.5f}*on.scale);ck(near(base,palm.position));
  ck(near(rotate(on.pose.orientation,{0,1,0}),{0,-1,0}));}
 // the longest side along the fist / the fingers
 {const Vec3 lo{-.43f,-.13f,-.12f},hi{.44f,.14f,.13f};   // a scroll lying along X
  const auto r=longestAlong(lo,hi,ConsumableHold::Fist);Vec3 a,b;turnedBounds(r,lo,hi,a,b);ck(b.y-a.y>.86f&&b.x-a.x<.28f);
  const Vec3 zlo{-.13f,0,-.38f},zhi{.13f,.3f,.38f};const auto rz=longestAlong(zlo,zhi,ConsumableHold::Fist);turnedBounds(rz,zlo,zhi,a,b);ck(b.y-a.y>.75f);
  const auto rp=longestAlong(lo,hi,ConsumableHold::Palm);turnedBounds(rp,lo,hi,a,b);ck(b.z-a.z>.86f&&std::abs((b.y-a.y)-.27f)<1e-4f);
  ck(near(rotate(longestAlong({-.1f,0,-.1f},{.1f,.4f,.1f},ConsumableHold::Fist),{0,1,0}),{0,1,0}));}
 // a fish by its tail: the tail base in the grip, the head out of the fist's other side (the held blade's way turned round)
 {const Vec3 lo{-.08f,-.1f,-.6f},hi{.08f,.2f,.7f};const auto held=mountLiveWeapon(grip);
  const auto m=mountByTail(grip,lo,hi,.26f);ck(std::abs(m.scale-.2f)<1e-5f);
  const Vec3 tailBase{0,.05f,-.6f+.2f*1.3f};ck(near(m.pose.position+rotate(m.pose.orientation,tailBase*m.scale),grip.position));
  const Vec3 head{0,.05f,.7f};const auto toHead=rotate(m.pose.orientation,head*m.scale)-rotate(m.pose.orientation,tailBase*m.scale);
  ck(dot(toHead*(1/length(toHead)),rotate(held.orientation,{0,-1,0}))>.999f);}
 // size 0: the fish as large as its model (the caught one)
 {const auto m=mountByTail(grip,{-.08f,-.1f,-.6f},{.08f,.2f,.7f},0);ck(m.scale==1.f);}
 std::cout<<"consumable model tests passed: "<<n<<"\n";return 0;}
 catch(const std::exception& e){std::cerr<<"consumable model test failed: "<<e.what()<<"\n";return 1;}}
