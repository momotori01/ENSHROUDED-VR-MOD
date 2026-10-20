#include <evr/live_targets.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 auto close=[](float a,float b){return std::abs(a-b)<1e-4f;};
 // Segment to segment.
 ck(close(segmentDistance({0,0,0},{1,0,0},{0,1,0},{1,1,0}),1));                 // parallel
 ck(close(segmentDistance({-1,0,0},{1,0,0},{0,-1,2},{0,1,2}),2));               // crossing, apart along z
 ck(close(segmentDistance({0,0,0},{1,0,0},{3,0,0},{4,0,0}),2));                 // in line, end to end
 ck(close(segmentDistance({0,0,0},{0,0,0},{3,4,0},{3,4,0}),5));                 // two points
 ck(close(segmentDistance({2,5,0},{2,5,0},{0,0,0},{0,2,0}),std::sqrt(4.f+9.f)));// a point above the end of the axis
 ck(segmentDistance({-1,1,0},{1,1,0},{0,0,0},{0,2,0})<1e-5f);                   // through the axis
 // The blade and a target, in world Q32 (2^32 per metre) far from the origin.
 const auto q=[](double metres){return int64_t(metres*4294967296.0);};
 const std::array<int64_t,3> base{q(3723.5),q(845.25),q(1442.75)};
 const auto at=[&](double x,double y,double z){return std::array<int64_t,3>{base[0]+q(x),base[1]+q(y),base[2]+q(z)};};
 LiveTarget person;person.entity=7;person.position=at(0,0,0);person.character=true;
 LiveTarget tree=person;tree.character=false;
 // A destructible thing without a health (2026-10-20: tables, pots): a smaller reach than a tree's, and a low axis.
 {LiveTarget pot=person;pot.character=false;pot.object=true;LiveTarget treeHere=pot;treeHere.object=false;
  ck(bladeNear(at(.6,.5,-.2),at(.6,.5,.2),pot)&&!bladeNear(at(1,.5,-.2),at(1,.5,.2),pot)&&bladeNear(at(1,.5,-.2),at(1,.5,.2),treeHere));
  ck(!bladeNear(at(0,2,-.2),at(0,2,.2),pot)&&bladeNear(at(0,2,-.2),at(0,2,.2),treeHere));}
 ck(close(bladeDistance(at(.5,1.2,0),at(1.4,1.2,0),person),.5f));               // level blade pointing away, hilt half a metre off
 ck(bladeNear(at(.5,1.2,0),at(1.4,1.2,0),person));ck(bladeNear(at(.8,1.2,0),at(1.7,1.2,0),person));ck(!bladeNear(at(.9,1.2,0),at(1.8,1.2,0),person)); // a stroke reaches 0.85 m
 ck(bladeNear(at(1.2,1.2,0),at(2.1,1.2,0),tree));ck(!bladeNear(at(1.4,1.2,0),at(2.3,1.2,0),tree)); // a trunk is wider
 ck(bladeNear(at(-1,1,.3),at(1,1,.3),person));                                  // sweeping past the body
 ck(!bladeNear(at(0,3.2,0),at(.5,3.4,0),person));                               // above the head: 1 m over the 2.2 m axis
 ck(LiveTargetReach{}.characterRadius<LiveTargetReach{}.characterStrike);        // the aim mark keeps the body's own radius
 ck(bladeNear(at(0,2.5,0),at(.2,2.6,0),person));                                // just over the head
 ck(!bladeNear(at(0,-1,0),at(.3,-1.2,0),person));                               // below the feet
 LiveTargetReach wide;wide.characterStrike=2;ck(bladeNear(at(1.5,1,0),at(1.9,1,0),person,wide));
 // The blade a little later. 0.2 m in 1/30 s is 6 m/s: 0.1 s ahead that is 0.6 m further.
 {const auto metres=[&](const std::array<int64_t,3>& a,const std::array<int64_t,3>& b,int i){return double(a[i]-b[i])/4294967296.0;};
  const auto before=at(2,1.2,0),now=at(1.8,1.2,0);const auto ahead=bladeAhead(now,before,1/30.,.1);
  ck(std::abs(metres(ahead,now,0)+.6)<1e-4&&ahead[1]==now[1]&&ahead[2]==now[2]);
  ck(!bladeNear(now,at(2.7,1.2,0),tree)&&bladeNear(ahead,bladeAhead(at(2.7,1.2,0),at(2.9,1.2,0),1/30.,.1),tree));   // not there yet, there in 0.1 s
  ck(bladeAhead(now,before,1/30.,0)==now&&bladeAhead(now,before,0,.1)==now&&bladeAhead(now,before,.002,.1)==now&&bladeAhead(now,before,.2,.1)==now);  // no lead; no time; a gap
  ck(bladeAhead(now,now,1/30.,.1)==now);                                         // a blade at rest
  const auto jump=bladeAhead(at(0,1.2,0),at(2,1.2,0),1/90.,.1);ck(std::abs(metres(jump,at(0,1.2,0),0)+.8)<1e-4);   // 180 m/s: limited to 0.8 m
  ck(std::abs(metres(bladeAhead(now,before,1/30.,.1,.25),now,0)+.25)<1e-4);}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
