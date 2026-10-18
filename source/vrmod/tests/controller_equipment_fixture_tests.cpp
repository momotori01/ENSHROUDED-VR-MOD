#include <evr/controller_equipment_fixture.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::game;
int main(){unsigned n=0;try{
 auto require=[&](bool b){++n;if(!b)throw std::runtime_error("equipment fixture invariant "+std::to_string(n));};
 using P=ControllerEquipmentFixture::Phase;EquipmentObservation o{true,17,20,30,40};
 for(unsigned scenario=0;scenario<9;scenario++){
  ControllerEquipmentFixture f(20);bool prior=false,next=false,other=false,complete=false,aborted=false;unsigned presses=0;uint16_t last=0;
  for(uint64_t t=1;t<=13001;t+=10){
   auto sample=o;bool connected=t>100,hud=true;
   if(t>=1201&&t<3001)sample.item=21;
   if(t>=3001){sample.child=31;sample.handle=41;}
   if(scenario==1)sample.item=20; // Unrecognized button mapping: neutral timeout, no retry.
   if(scenario==2&&t>=1801)hud=false;
   if(scenario==3&&t>=1801)sample.root=18;
   if(scenario==4&&t>=1801)sample.valid=false;
   if(scenario==5&&t>=1801)connected=false;
   if(scenario==6)sample.item=21;
   if(scenario==7&&t>=3001)sample.item=22; // Failed return cannot complete.
   if(scenario==8&&t>=1201&&t<1401){sample.item=sample.child=sample.handle=0;} // Streaming gap in wait phase.
   auto c=f.update(t,connected,hud,sample);require(c.buttons==0||c.buttons==4||c.buttons==8);
   if(c.buttons&&c.buttons!=last)++presses;last=c.buttons;
   prior|=c.buttons==4;next|=c.buttons==8;other|=c.phase==P::ObserveOther;
   if(c.phase==P::Complete||c.phase==P::Aborted){require(!c.buttons);complete=c.phase==P::Complete;aborted=!complete;
    require(f.update(t+10,true,true,o).phase==c.phase);break;}
  }
  if(scenario==0||scenario==8){require(complete&&!aborted&&prior&&next&&other&&presses==2&&f.otherItem()==21);}
  else {require(aborted&&!complete);require(presses<=2);if(scenario==1)require(prior&&!next&&presses==1);}
 }
 ControllerEquipmentFixture invalid;require(invalid.update(1,true,true,o).phase==P::Aborted);
 ControllerEquipmentFixture clock(20);clock.update(100,true,true,o);require(clock.update(99,true,true,o).phase==P::Aborted);
 std::cout<<"PASS "<<n<<" bounded hotbar cases; actual mapping requires native observation\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
