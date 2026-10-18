#include <evr/hand_rig.hpp>
#include <iostream>
#include <limits>
#include <string>
using namespace evr;
// Finger curl (2026-10-09, EDF6VR-style) on a synthetic two-hand skeleton shaped like the native one:
// branch root -> wrist -> {thumb(3), index(4), middle(4), ring(4), little(4), socket}.
int main(){try{
 unsigned checks=0;auto check=[&](bool ok){++checks;if(!ok)throw std::runtime_error("finger curl invariant "+std::to_string(checks));};
 std::vector<hand::Joint> neutral;std::vector<uint16_t> parents;std::vector<uint16_t> skin;
 auto add=[&](uint16_t parent,Vec3 p){hand::Joint j;j.position=p;neutral.push_back(j);parents.push_back(parent);return uint16_t(neutral.size()-1);};
 const auto body=add(UINT16_MAX,{0,0,0});
 std::array<hand::Branch,2> branches{};std::array<std::array<uint16_t,3>,2> anatomy{};std::array<std::array<uint16_t,hand::FingerCount>,2> tops{};
 for(unsigned h=0;h<2;h++){
  const auto root=add(body,{h?.3f:-.3f,1,0}),wrist=add(root,{0,0,.25f});skin.push_back(wrist);
  const unsigned lengths[hand::FingerCount]{3,4,4,4,4};
  for(unsigned f=0;f<hand::FingerCount;f++){
   uint16_t at=wrist;
   for(unsigned k=0;k<lengths[f];k++){at=add(at,{.02f*f,0,.03f});skin.push_back(at);if(!k)tops[h][f]=at;}
  }
  const auto socket=add(wrist,{0,-.02f,.06f});add(socket,{0,0,.01f});
  add(wrist,{0,.01f,.02f}); // single helper bone beside the socket, as the native left wrist (66)
  branches[h]={root,wrist,socket};
  anatomy[h]={uint16_t(tops[h][hand::Index]+1),uint16_t(tops[h][hand::Middle]+1),uint16_t(tops[h][hand::Little]+1)};
 }
 hand::Rig rig(neutral,parents,skin,branches);
 auto closed=neutral;for(auto& j:closed)j.orientation=euler(0,1.2f,0);
 check(!rig.curlEnabled());
 rig.enableCurl(closed,anatomy);check(rig.curlEnabled());
 for(unsigned h=0;h<2;h++)for(unsigned f=0;f<hand::FingerCount;f++){
  const auto& chain=rig.fingerJoints(h)[f];check(!chain.empty()&&chain.front()==tops[h][f]&&chain.size()==(f==hand::Thumb?3u:4u));}
 const std::array<Pose,2> grips{{{euler(.2f,0,0),{-.3f,1,.4f}},{euler(-.2f,0,0),{.3f,1,.4f}}}};
 auto angle=[](Quat a,Quat b){const float d=std::abs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);return 2*std::acos(std::min(1.f,d));};
 for(float t:{0.f,.5f,1.f}){
  std::array<hand::FingerCurl,2> curl{};for(auto& c:curl)c.amount.fill(t);
  const auto out=rig.locate(grips,&curl);
  for(unsigned h=0;h<2;h++)for(unsigned f=0;f<hand::FingerCount;f++)for(auto j:rig.fingerJoints(h)[f]){
   check(std::abs(angle(out[j].orientation,neutral[j].orientation)-1.2f*t)<2e-3f);check(length(out[j].position-neutral[j].position)==0);}
 }
 {std::array<hand::FingerCurl,2> curl{};curl[1].amount[hand::Index]=1;const auto out=rig.locate(grips,&curl);
  for(unsigned h=0;h<2;h++)for(unsigned f=0;f<hand::FingerCount;f++)for(auto j:rig.fingerJoints(h)[f])
   check((h==1&&f==hand::Index)?angle(out[j].orientation,closed[j].orientation)<1e-4f:angle(out[j].orientation,neutral[j].orientation)<1e-4f);}
 {const auto plain=rig.locate(grips),none=rig.locate(grips,nullptr);check(plain.size()==none.size());
  for(size_t i=0;i<plain.size();i++)check(angle(plain[i].orientation,neutral[i].orientation)<1e-6f||i==branches[0].root||i==branches[1].root);}
 // Control mapping: relaxed .15, trigger dead zone .05 then closed at .8 of the rest (fourth real-device run), grip closed at .08, thumb on stick.
 auto c=hand::curlFromControls(0,0,false);check(std::abs(c.amount[hand::Index]-.15f)<1e-6f&&std::abs(c.amount[hand::Middle]-.15f)<1e-6f&&std::abs(c.amount[hand::Thumb]-.15f)<1e-6f);
 check(std::abs(hand::curlFromControls(.05f,0,false).amount[hand::Index]-.15f)<1e-6f);
 check(hand::curlFromControls(.3f,0,false).amount[hand::Index]<.6f); // no longer closed at a third of the pull
 check(hand::curlFromControls(.82f,0,false).amount[hand::Index]==1&&hand::curlFromControls(0,.08f,false).amount[hand::Ring]==1&&hand::curlFromControls(0,0,true).amount[hand::Thumb]==1);
 const float half=hand::curlFromControls(.05f+.4f*.95f,.04f,false).amount[hand::Index];check(std::abs(half-(.15f+.85f*.5f))<1e-4f);
 check(std::abs(hand::curlFromControls(0,.04f,false).amount[hand::Little]-(.15f+.85f*.5f))<1e-4f);
 check(hand::curlFromControls(std::numeric_limits<float>::quiet_NaN(),-1,false).amount[hand::Index]==.15f);
 // Rejections: closed pose shape, anatomy that names the same chain twice.
 {hand::Rig r(neutral,parents,skin,branches);bool rejected=false;try{r.enableCurl(std::vector<hand::Joint>(3),anatomy);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&!r.curlEnabled());}
 {hand::Rig r(neutral,parents,skin,branches);auto bad=anatomy;bad[0][1]=bad[0][0];bool rejected=false;try{r.enableCurl(closed,bad);}catch(const std::invalid_argument&){rejected=true;}check(rejected&&!r.curlEnabled());}
 std::cout<<"PASS "<<checks<<" finger curl checks (synthetic skeleton; native hand pose unverified)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
