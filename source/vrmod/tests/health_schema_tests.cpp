#include <evr/health_schema.hpp>
#include <iostream>
#include <string>
#include <stdexcept>
using namespace evr::motion;
int main(){unsigned tests=0;try{
 auto check=[&](bool v){++tests;if(!v)throw std::runtime_error("Health schema "+std::to_string(tests));};
 HealthSchema s;s.componentName="keen::ecs::Health";s.attributeName="Health";s.componentId=230;s.componentBytes=68;
 auto u32=[](auto& b,unsigned at,uint32_t v){std::memcpy(b.data()+at,&v,4);};
 u32(s.container,0,0x8eb84995);u32(s.container,8,36);u32(s.container,12,8);s.container[16]=1;u32(s.attribute,0,0x8eb84995);u32(s.attribute,4,0x8eb84995);
 for(uint16_t index=0;index<10;++index)for(uint32_t count=0;count<10;++count){auto x=s;std::memcpy(x.attribute.data()+12,&index,2);u32(x.container,12,count);check(verifiedHealthSchema(x)==(count>0&&count<=8&&index<count));}
 for(auto name:{"keen::ecs::Stamina","keen::ecs::HealthSaved","Health",""}){auto x=s;x.componentName=name;check(!verifiedHealthSchema(x));}
 for(auto name:{"Health_Max","Health_Max_Base","Stamina",""}){auto x=s;x.attributeName=name;check(!verifiedHealthSchema(x));}
 for(unsigned at:{0u,8u,12u}){auto x=s;u32(x.container,at,0);check(!verifiedHealthSchema(x));}
 for(unsigned at:{0u,4u}){auto x=s;u32(x.attribute,at,0);check(!verifiedHealthSchema(x));}
 for(unsigned size:{0u,20u,67u,69u,3600u}){auto x=s;x.componentBytes=size;check(!verifiedHealthSchema(x));}
 for(unsigned id:{0u,230u,639u,640u,1023u}){auto x=s;x.componentId=id;check(verifiedHealthSchema(x)==(id<640));}
 auto inactive=s;inactive.container[16]=0;check(!verifiedHealthSchema(inactive));
 check(verifiedHealthSchema(s));std::cout<<"PASS "<<tests<<" independent Health identity/bounds checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
