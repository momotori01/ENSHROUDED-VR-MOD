#include <evr/bow_aim.hpp>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <string>
#include <limits>
using namespace evr;using namespace evr::motion;
std::string bytes(const void* p,size_t n){std::ostringstream s;s<<std::hex<<std::setfill('0');for(size_t i=0;i<n;++i)s<<std::setw(2)<<unsigned(static_cast<const uint8_t*>(p)[i]);return s.str();}
void writeVec(Vec3 v){std::cout<<'['<<v.x<<','<<v.y<<','<<v.z<<']';}
void emit(WorldPositionQ32 p,Quat q,Vec3 scale,Vec3 nock,const BowSpawnEvent& event){
 const auto source=event;const auto aim=knownBowWorldAim(p,q,scale,nock);const auto camera=bowAimCamera(aim,event);
 if(event!=source)throw std::runtime_error("event changed");
 std::cout<<std::setprecision(10)<<"{\"model_q32\":["<<p.x<<','<<p.y<<','<<p.z<<"],\"model_q\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"],\"scale\":";writeVec(scale);
 std::cout<<",\"nock\":";writeVec(nock);std::cout<<",\"camera\":\""<<bytes(camera.data(),camera.size())<<"\",\"event\":\""<<bytes(event.data(),event.size())<<"\",\"aim_q\":["<<aim.orientation.x<<','<<aim.orientation.y<<','<<aim.orientation.z<<','<<aim.orientation.w<<"]}\n";
}
int main(int argc,char** argv){try{
 if(argc==2&&std::string(argv[1])=="--stdin"){
  WorldPositionQ32 p;Quat q;Vec3 scale,nock;std::string hex;
  while(std::cin>>p.x>>p.y>>p.z>>q.x>>q.y>>q.z>>q.w>>scale.x>>scale.y>>scale.z>>nock.x>>nock.y>>nock.z>>hex){
   if(hex.size()!=672)throw std::invalid_argument("event size");BowSpawnEvent event{};for(size_t i=0;i<event.size();i++)event[i]=static_cast<uint8_t>(std::stoul(hex.substr(i*2,2),nullptr,16));emit(p,q,scale,nock,event);
  }if(!std::cin.eof())throw std::invalid_argument("bad input row");return 0;
 }
 const bool vectors=argc==2&&std::string(argv[1])=="--vectors";unsigned checks=0;auto ck=[&](bool ok){++checks;if(!ok)throw std::runtime_error("check "+std::to_string(checks));};
 for(unsigned i=0;i<128;i++){
  const WorldPositionQ32 p{16061959037952ll+int64_t(i)*1234567,3628173623194ll-int64_t(i)*734561,6131030293504ll};
  const auto q=normalized(euler(-3.1f+float(i)*.05f,.8f*std::sin(float(i)),.7f*std::cos(float(i))));
  const Vec3 scale{.8f+.3f*float(i%7)/6,1.1f-.2f*float(i%5)/4,1};
  const Vec3 nock{.08f*std::sin(float(i)),.06f*std::cos(float(i)),.24f+.5f*float(i%13)/12};
  BowSpawnEvent e{};e[0x90]=3;e[0x148]=1;const Vec3 local{.015f*float(i%4),-.1999f,.007f},world{0,.003f*float(i%3),0};const auto offset=euler(.01f*float(i%3),-2*pi/180,.015f*float(i%4));
  std::memcpy(e.data()+0xc0,&local,12);std::memcpy(e.data()+0xcc,&world,12);std::memcpy(e.data()+0xd8,&offset,16);
  const auto aim=knownBowWorldAim(p,q,scale,nock);const auto c=bowAimCamera(aim,e);const auto source=e;
  Quat cq;WorldPositionQ32 cp;std::memcpy(&cp,c.data(),24);std::memcpy(&cq,c.data()+24,16);
  const auto out=cameraPosition(cp,rotate(cq,local)+world,1);const auto forward=rotate(multiply(cq,offset),{0,0,1});
  ck(length(forward-aim.direction)<1e-6f);ck(std::abs(relativeQ32(out.x,aim.origin.x))<2e-7&&std::abs(relativeQ32(out.y,aim.origin.y))<2e-7&&std::abs(relativeQ32(out.z,aim.origin.z))<2e-7);ck(e==source);
  if(vectors)emit(p,q,scale,nock,e);
 }
 for(Vec3 bad:std::array<Vec3,3>{{{0,0,0},{0,0,-.5f},{0,std::numeric_limits<float>::quiet_NaN(),.5f}}}){bool refused=false;try{knownBowWorldAim({}, {0,0,0,1},{1,1,1},bad);}catch(const std::invalid_argument&){refused=true;}ck(refused);}
 bool refused=false;try{bowAimCamera({},{});}catch(const std::invalid_argument&){refused=true;}ck(refused);
 if(!vectors)std::cout<<checks<<" PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
