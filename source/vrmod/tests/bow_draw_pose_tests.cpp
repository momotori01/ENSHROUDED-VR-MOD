#include <evr/bow_draw_pose.hpp>
#include <iostream>
#include <string>
#include <cstring>
#include <limits>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool x){++n;if(!x)throw std::runtime_error(std::to_string(n));};
 const auto rest=knownBowDrawPose(0);const auto end=knownBowDrawPose(10);
 ck(!memcmp(rest.data(),knownBowDrawKeys.front().joints.data(),sizeof(rest)));
 ck(!memcmp(end.data(),knownBowDrawKeys.back().joints.data(),sizeof(end)));
 for(unsigned i=0;i<=555;i++){
  const float pull=float(i)/1000;const auto p=knownBowDrawPose(pull);
  ck(!memcmp(p.data(),rest.data(),2*sizeof(hand::Joint)));
  ck(std::abs(rest[5].position.z-p[5].position.z-pull)<2e-6f);
  for(auto j:p){auto q=j.orientation;ck(std::abs(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w-1)<.002f&&finite(j.position)&&finite(j.scale));}
 }
 for(unsigned i=0;i<=200;i++){
  const float pull=.555f*i/200.f;const Vec3 target{.09f*std::sin(float(i)),.07f*std::cos(float(i)),.24005109f+pull};
  const auto curve=knownBowDrawPose(pull),p=knownBowNockPose(pull,target);
  ck(!memcmp(p.data(),curve.data(),4*sizeof(hand::Joint)));
  const auto root=compose(Pose{normalized(p[0].orientation),p[0].position},Pose{normalized(p[1].orientation),p[1].position});
  const auto endpoint=root.position+rotate(root.orientation,p[5].position);
  ck(length(endpoint-Vec3{target.x,target.y,-target.z})<1e-6f);
  ck(length((p[4].position-p[5].position)-(curve[4].position-curve[5].position))<1e-6f);
 }
 bool badNock=false;try{knownBowNockPose(.2f,{0,std::numeric_limits<float>::infinity(),0});}catch(const std::invalid_argument&){badNock=true;}ck(badNock);
 bool bad=false;try{knownBowDrawPose(std::numeric_limits<float>::quiet_NaN());}catch(const std::invalid_argument&){bad=true;}ck(bad);
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
