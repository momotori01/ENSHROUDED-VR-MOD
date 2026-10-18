#include <evr/hand_rig.hpp>
#include <cstring>
#include <iostream>
#include <string>
#include <limits>
using namespace evr;
int main(){try{
 unsigned checks=0;auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("hand rig invariant "+std::to_string(checks));};
 std::vector<hand::Joint> neutral(11);std::vector<uint16_t> parents{UINT16_MAX,0,1,2,2,4,0,6,7,7,9};
 neutral[0].orientation=euler(.4f,-.3f,.2f);neutral[0].position={3,2,-4};
 neutral[1].position={-.6f,1.2f,0};neutral[6].position={.6f,1.2f,0};
 neutral[1].orientation=euler(.3f,.2f,-.9f);neutral[6].orientation=euler(-.5f,.1f,.8f);
 neutral[2].position=neutral[7].position={.13f,0,0};neutral[3].position=neutral[8].position={.08f,0,0};
 neutral[4].position={.1f,-.03f,-.02f};neutral[9].position={.1f,.03f,-.02f};
 neutral[4].orientation=euler(pi/2,0,0);neutral[9].orientation=euler(-pi/2,0,pi);
 std::vector<uint16_t> skin{1,2,3,6,7,8};std::array<hand::Branch,2> branches{{{1,2,4},{6,7,9}}};
 hand::Rig rig(neutral,parents,skin,branches);auto initial=rig.globals(neutral);
 auto close=[&](Pose a,Pose b){check(length(a.position-b.position)<2e-5f);check(length(rotate(a.orientation,{0,0,1})-rotate(b.orientation,{0,0,1}))<2e-5f);check(length(rotate(a.orientation,{0,1,0})-rotate(b.orientation,{0,1,0}))<2e-5f);};
 // Independently chosen target socket poses, with both sides rotating/translating
 // in opposite directions. Three minutes at90Hz, no native pointers or assets.
 for(unsigned n=0;n<16200;n++){
  const float t=float(n)/90;
  const std::array<Pose,2> target{{{euler(t*.8f,.5f*std::sin(t),.7f*std::cos(t)),{-.3f+.1f*std::sin(t),-.2f,.3f}},
                               {euler(-t*.6f,.4f*std::cos(t),-.6f*std::sin(t)),{.3f,-.2f+.1f*std::cos(t),.3f+.1f*std::sin(t)}}}};
  auto joints=rig.locate(target);auto world=rig.globals(joints);close(world[4],target[0]);close(world[9],target[1]);
  for(size_t i=0;i<joints.size();i++)if(i!=1&&i!=6){check(length(joints[i].position-neutral[i].position)==0);check(length(rotate(joints[i].orientation,{1,0,0})-rotate(neutral[i].orientation,{1,0,0}))<1e-6f);}
  // Native parent root (and therefore body/world transform) remains untouched.
  close(world[0],initial[0]);
  check(std::abs(length(world[3].position-world[2].position)-.08f)<2e-5f);
  check(std::abs(length(world[8].position-world[7].position)-.08f)<2e-5f);
 }
 auto reject=[&](auto fn){bool caught=false;try{fn();}catch(const std::exception&){caught=true;}check(caught);};
 reject([&]{auto bad=parents;bad[2]=3;hand::Rig x(neutral,bad,skin,branches);});
 reject([&]{auto bad=neutral;bad[3].scale.x=2;hand::Rig x(bad,parents,skin,branches);});
 reject([&]{auto bad=skin;bad.push_back(0);hand::Rig x(neutral,parents,bad,branches);});
 reject([&]{auto bad=branches;bad[1]=bad[0];hand::Rig x(neutral,parents,skin,bad);});
 reject([&]{auto bad=branches;bad[0].socket=9;hand::Rig x(neutral,parents,skin,bad);});
 reject([&]{std::array<Pose,2> p{identityPose(),identityPose()};p[0].position.x=std::numeric_limits<float>::quiet_NaN();rig.locate(p);});
 reject([&]{std::array<Pose,2> p{identityPose(),identityPose()};p[1].orientation={0,0,0,0};rig.locate(p);});
 std::cout<<"PASS "<<checks<<" hand rig invariants;3minute sequence and7 rejection cases\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
