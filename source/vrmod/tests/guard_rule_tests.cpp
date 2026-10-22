#include <evr/guard_rule.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::guard;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 const double pi=3.14159265358979;
 auto at=[&](double degrees,double metres=2){return std::make_pair(std::sin(degrees*pi/180)*metres,std::cos(degrees*pi/180)*metres);};
 // front +Z: an attacker straight ahead, 45 and 79 degrees to either side is blocked; 81, 90, behind are not
 for(const double d:{0.,45.,-45.,79.,-79.}){const auto a=at(d);ck(blocks(0,1,a.first,a.second));}
 for(const double d:{81.,-81.,90.,-90.,135.,180.}){const auto a=at(d);ck(!blocks(0,1,a.first,a.second));}
 // another front (the shield turned 90 degrees to the right, +X): ahead of it is blocked, ahead of the body is at the edge
 {const auto a=at(90);ck(blocks(1,0,a.first,a.second));}{const auto a=at(-90);ck(!blocks(1,0,a.first,a.second));}
 {const auto a=at(0);ck(std::abs(angleFromFront(1,0,a.first,a.second)-pi/2)<1e-9&&!blocks(1,0,a.first,a.second));}
 // an attacker on the target, a front not known: 0 (blocked by the front test alone)
 ck(angleFromFront(0,1,.01,.01)==0);ck(angleFromFront(0,0,1,0)==0);
 // the stamina: round(0.2 x damage x mod - block), 0 at the least; the health change's sign does not matter
 ck(staminaCost(100,1,0)==20);ck(staminaCost(-100,1,0)==20);ck(staminaCost(100,1,5)==15);ck(staminaCost(100,1.5,0)==30);
 ck(staminaCost(12,1,0)==2);   // 2.4
 ck(staminaCost(13,1,0)==3);   // 2.6
 ck(staminaCost(25,1,0)==5);ck(staminaCost(10,1,5)==0);ck(staminaCost(10,1,2)==0);ck(staminaCost(0,1,0)==0);
 std::cout<<"guard_rule: "<<n<<" checks passed\n";return 0;}
 catch(const std::exception& e){std::cerr<<"guard_rule check failed: "<<e.what()<<'\n';return 1;}}
