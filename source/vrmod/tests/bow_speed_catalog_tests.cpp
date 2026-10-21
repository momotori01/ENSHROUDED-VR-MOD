#include <evr/bow_speed_catalog.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
int main(){try{
 auto require=[](bool b,const char* what){if(!b)throw std::runtime_error(what);};
 require(catalogBowSpeeds.size()==62,"62 bows");
 for(size_t i=1;i<catalogBowSpeeds.size();++i)require(catalogBowSpeeds[i-1].item<catalogBowSpeeds[i].item,"sorted, no duplicate");
 unsigned counts[4]{};
 for(const auto& b:catalogBowSpeeds){require(catalogBowSpeed(b.item)==b.speed,"every bow found");
  if(b.speed==75.f)++counts[0];else if(b.speed==80.f)++counts[1];else if(b.speed==90.f)++counts[2];else if(b.speed==100.f)++counts[3];else require(false,"speed 75 / 80 / 90 / 100");}
 require(counts[0]==1&&counts[1]==3&&counts[2]==45&&counts[3]==13,"the data's counts");
 require(catalogBowSpeed(327157552u)==90.f,"the measured forest long bow (89.9 - 90.3 m/s in game)");
 require(catalogBowSpeed(0)==0&&catalogBowSpeed(1)==0&&catalogBowSpeed(0xffffffffu)==0,"not a bow: 0");
 std::cout<<"PASS bow speed catalog: 62 bows sorted, 75 / 80 / 90 / 100 m/s, the measured bow 90, others 0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
