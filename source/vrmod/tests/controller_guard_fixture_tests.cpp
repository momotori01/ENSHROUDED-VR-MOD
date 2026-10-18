#include <evr/controller_guard_fixture.hpp>
#include <iostream>
#include <string>
#include <stdexcept>
using namespace evr::game;
int main(){unsigned n=0;try{
 auto check=[&](bool b){++n;if(!b)throw std::runtime_error("guard fixture invariant "+std::to_string(n));};
 for(unsigned fault=0;fault<12;fault++){
  ControllerGuardFixture fixture;GuardObservation o{true,23,41,true,false,0};unsigned previousButtons=0,edges=0;
  for(uint64_t now=0;now<14000;now+=20){
   const bool held=previousButtons!=0;bool connected=now>=40,hud=true;
   o.blocking=held;o.digital=held?((1ull<<30)|(1ull<<4)):0;
   if(now>=1600)switch(fault){case 1:o.root=24;break;case 2:o.item=42;break;case 3:o.valid=false;break;case 4:hud=false;break;
    case 5:connected=false;break;case 6:o.digital|=1;break;case 7:o.blocking=false;break;case 8:o.digital=0;break;case 9:o.blocking=true;break;case 10:o.digital=1ull<<30;break;case 11:o.digital=1ull<<4;break;}
   const auto r=fixture.update(now,connected,hud,o);check(r.buttons==0||r.buttons==0x100);
   if(!previousButtons&&r.buttons)edges++;previousButtons=r.buttons;
   if(r.phase==ControllerGuardFixture::Phase::Complete){check(fault==0&&fixture.holds()==2&&edges==2);break;}
   if(r.phase==ControllerGuardFixture::Phase::Aborted){check(fault!=0&&!r.buttons);for(int i=0;i<4;i++)check(!fixture.update(now+uint64_t(i)*20,true,true,{true,23,41,true,false,0}).buttons);break;}
   if(now>=13000)throw std::runtime_error("fixture failed to terminate");
  }
 }
 // No observed block at all cannot be counted as a successful press.
 {ControllerGuardFixture f;for(uint64_t t=0;t<3000;t+=20)f.update(t,true,true,{true,1,1,true,false,0});check(f.phase()==ControllerGuardFixture::Phase::Aborted);}
 // No acknowledgement of release, backwards clock and connection timeout.
 {ControllerGuardFixture f;f.update(100,true,true,{true,1,1,true,false,0});check(f.update(99,true,true,{true,1,1,true,false,0}).phase==ControllerGuardFixture::Phase::Aborted);}
 {ControllerGuardFixture f;f.update(0,false,true,{true,1,1,true,false,0});check(f.update(12001,false,true,{true,1,1,true,false,0}).phase==ControllerGuardFixture::Phase::Aborted);}
 std::cout<<"PASS "<<n<<" bounded native guard fixture checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
