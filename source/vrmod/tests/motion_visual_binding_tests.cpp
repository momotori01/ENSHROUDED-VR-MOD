#include <evr/motion_visual_binding.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::motion;
int main(){unsigned n=0;try{
 auto require=[&](bool b){++n;if(!b)throw std::runtime_error("visual binding invariant "+std::to_string(n));};
 const std::array<uint32_t,4> guid{1,2,3,4};const VisualIdentity original{20,0x1000050,5,0x100000,guid};auto selected=original;
 auto decide=[&](const VisualIdentity& s,bool valid=true,bool owned=true){return visualBinding(s,original,20,guid,valid,owned);};
 require(decide(selected,true,false)==VisualBinding::Create);require(decide(selected)==VisualBinding::Update);
 selected.source+=0x1000000;selected.child++;require(decide(selected)==VisualBinding::Rebind); // Same pool index, newer generation.
 require(original.source==0x1000050&&original.child==5); // No input mutation.
 for(unsigned k=0;k<8;k++){
  auto bad=selected;switch(k){case 0:bad.item++;break;case 1:bad.resource++;break;case 2:bad.guid[0]++;break;
   case 3:bad.source=0;break;case 4:bad.child=0;break;case 5:bad.resource=0;break;case 6:bad.guid={};break;case 7:break;}
  require(decide(bad,k!=7)==VisualBinding::Suspend);
 }
 require(visualBinding(original,original,0,guid,true,true)==VisualBinding::Suspend);
 require(visualBinding(original,original,20,{},true,true)==VisualBinding::Suspend);
 for(unsigned cycle=0;cycle<200;cycle++){
  auto bow=original;bow.item=21;bow.guid[0]=3;require(decide(bow)==VisualBinding::Suspend);
  auto back=original;back.source+=0x1000000*(1+cycle%10);back.child+=cycle+1;require(decide(back)==VisualBinding::Rebind);
  auto missing=back;missing.source=0;require(decide(missing)==VisualBinding::Suspend);
 }
 std::array<RetainedVisual,retainedVisualCapacity> cache{};
 auto pick=selectRetainedVisual(cache,original,true);require(pick.valid&&pick.create&&pick.slot==0);
 cache[0]={original,0x1000100};auto second=original;second.item++;second.resource+=4096;second.guid[1]++;
 pick=selectRetainedVisual(cache,second,true);require(pick.valid&&pick.create&&pick.slot==1);cache[1]={second,0x1000101};
 for(unsigned cycle=0;cycle<200;++cycle){
  auto back=cycle%2?original:second;back.source+=cycle*0x1000000;back.child+=cycle;
  pick=selectRetainedVisual(cache,back,true);require(pick.valid&&!pick.create&&pick.slot==(cycle%2?0:1));
 }
 auto recycled=original;recycled.resource++;pick=selectRetainedVisual(cache,recycled,true);require(pick.valid&&pick.create&&pick.slot==2);
 auto differentGuid=original;differentGuid.guid[0]++;pick=selectRetainedVisual(cache,differentGuid,true);require(pick.valid&&pick.create&&pick.slot==2);
 for(size_t i=2;i<cache.size();++i){auto key=original;key.item+=uint32_t(i)+100;cache[i]={key,uint32_t(i)+1000};}
 require(!selectRetainedVisual(cache,recycled,true).valid); // Capacity never authorizes eviction.
 pick=selectRetainedVisual(cache,original,true);require(pick.valid&&!pick.create&&pick.slot==0);
 require(!selectRetainedVisual(cache,original,false).valid);
 for(unsigned k=0;k<5;k++){auto missing=original;switch(k){case 0:missing.item=0;break;case 1:missing.source=0;break;case 2:missing.child=0;break;case 3:missing.resource=0;break;case 4:missing.guid={};break;}require(!selectRetainedVisual(cache,missing,true).valid);}
 std::cout<<"PASS "<<n<<" render-only equipment identity cases; no native resource lifetime claim\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
