#include <evr/controller_menu_fixture.hpp>
#include <iostream>
#include <vector>
#include <array>
using F=evr::game::ControllerMenuFixture;using P=F::Phase;
int main(){unsigned passed=0,failed=0;auto check=[&](bool v,const char* n){std::cout<<(v?"PASS ":"FAIL ")<<n<<'\n';v?++passed:++failed;};
 F f;check(f.update(0,false,1,0,true).buttons==0,"wait for native connection");
 check(f.update(10,true,1,0,true).phase==P::Settle,"connected controller settles in HUD");
 check(f.update(509,true,1,0,true).buttons==0,"settle threshold preserves neutral input");
 check(f.update(510,true,1,0,true).buttons==0x10,"only START emitted to open menu");
 check(f.update(600,true,4,0,true).buttons==0x10,"early menu appearance does not create another edge");
 check(f.update(630,true,4,0,true).buttons==0,"first START released");
 check(f.update(640,true,4,0,true).phase==P::ObserveOpen,"actual menu appearance required");
 check(f.update(1339,true,4,0,true).captureTag==0&&f.update(1340,true,4,0,true).captureTag==1&&f.update(1350,true,4,0,true).captureTag==0,"capture waits for settled menu and fires once");
 check(f.update(2140,true,4,0,true).buttons==0x10,"second START emitted only after observed menu");
 check(f.update(2260,true,1,0,true).buttons==0,"second START released");
 check(f.update(2270,true,1,0,true).phase==P::ObserveClosed,"actual return to HUD required");
 check(f.update(2570,true,1,0,true).captureTag==2&&f.update(2580,true,1,0,true).captureTag==0,"return capture waits for HUD and fires once");
 check(f.update(3070,true,1,0,true).phase==P::Complete&&f.update(20000,true,1,0,true).buttons==0,"completed one-shot cannot repeat");
 for(unsigned scenario=0;scenario<6;scenario++){
  F g;g.update(100,true,1,0,true);auto r=g.update(scenario==0?99:scenario==1?10000:200,scenario!=2,scenario==3?9:1,scenario==4?1:0,scenario!=5);
  check(r.phase==P::Aborted&&r.buttons==0&&g.update(300,true,1,0,true).phase==P::Aborted,"clock/timeout/disconnect/unknown UI/overlay/owner changes abort and release");
 }
 F noMenu;noMenu.update(0,true,1,0,true);noMenu.update(500,true,1,0,true);noMenu.update(620,true,1,0,true);
 check(noMenu.update(1500,true,1,0,true).buttons==0&&noMenu.update(9001,true,1,0,true).phase==P::Aborted,"no close pulse if opening was not observed");
 F alreadyOpen;check(alreadyOpen.update(0,true,4,0,true).phase==P::Aborted,"cannot start from an existing menu");
 for(uint64_t step:{10,100,300}){
  F nav(true);uint8_t screen=1;uint16_t lastButtons=0;std::vector<uint16_t> pressed;std::vector<uint32_t> captures;bool safe=true;
  for(uint64_t now=0;now<19000;now+=step){
   const auto r=nav.update(now,true,screen,0,true);
   safe &= r.buttons==0||r.buttons==0x10||r.buttons==8||r.buttons==0x200||r.buttons==0x100;
   if(r.buttons&&r.buttons!=lastButtons)pressed.push_back(r.buttons);lastButtons=r.buttons;
   if(r.captureTag)captures.push_back(r.captureTag);
   if(r.phase==P::PressOpen)screen=4;
   if(r.phase==P::PressClose)screen=1;
   if(r.phase==P::Complete)break;
  }
  check(safe&&nav.phase()==P::Complete&&pressed==std::vector<uint16_t>{16,8,512,256,16}&&captures==std::vector<uint32_t>{1,3,4,5,2},"navigation has exactly five safe presses and all five settled captures across frame rates");
 }
 // Menu tour (2026-10-06): LB x3 then RB x5 inside the menu, one capture per tab, tabs may report another menu screen.
 for(uint64_t step:{10,100,300}){
  F tour(false,false,true);uint8_t screen=1;uint16_t lastButtons=0;std::vector<uint16_t> pressed;std::vector<uint32_t> captures;bool safe=true;
  for(uint64_t now=0;now<32000;now+=step){
   const auto r=tour.update(now,true,screen,0,true);
   safe &= r.buttons==0||r.buttons==0x10||r.buttons==0x200||r.buttons==0x100;
   if(r.buttons&&r.buttons!=lastButtons)pressed.push_back(r.buttons);lastButtons=r.buttons;
   if(r.captureTag)captures.push_back(r.captureTag);
   if(r.phase==P::PressOpen)screen=4;
   if(r.phase==P::TourObserve)screen=tour.tourStep()==2?7:4; // e.g. a map screen on the first tab
   if(r.phase==P::PressClose)screen=1;
   if(r.phase==P::Complete)break;
  }
  check(safe&&tour.phase()==P::Complete&&pressed==std::vector<uint16_t>{16,256,256,256,512,512,512,512,512,16}&&
        captures==std::vector<uint32_t>{1,3,3,3,3,3,3,3,3,2},"menu tour visits every tab once with one capture each and closes");
 }
 {F tour(false,false,true);uint64_t now=0;uint8_t screen=1;P last=P::WaitConnection;
  for(;now<32000&&last!=P::TourObserve;now+=10){auto r=tour.update(now,true,screen,0,true);last=r.phase;if(r.phase==P::PressOpen)screen=4;}
  check(tour.update(now,true,1,0,true).phase==P::Aborted,"menu tour aborts if gameplay returns mid tour");}
 for(P target:{P::PressSelectNext,P::ObserveSelection,P::PressNextTab,P::ObserveNextTab,P::PressPreviousTab,P::ObservePreviousTab}){
  F nav(true);uint8_t screen=1;bool aborted=false;
  for(uint64_t now=0;now<19000;now+=20){auto r=nav.update(now,true,screen,0,true);if(r.phase==P::PressOpen)screen=4;
   if(r.phase==target){r=nav.update(now+1,true,1,0,true);aborted=r.phase==P::Aborted&&r.buttons==0;break;}}
  check(aborted,"leaving native menu during any selection/tab phase aborts before gameplay button mapping");
 }
 for(uint64_t step:{10,100,300}){
  F altar(false,true);uint8_t screen=1;uint16_t last=0;std::vector<uint16_t> presses;bool released=true;
  for(uint64_t now=0;now<9000;now+=step){const auto r=altar.update(now,true,screen,0,true);
   if(r.buttons&&last!=r.buttons)presses.push_back(r.buttons);last=r.buttons;
   released &= !(screen==12&&r.buttons==0x8000)&&!(screen==1&&r.buttons==0x2000);
   if(r.phase==P::PressOpen)screen=12;if(r.phase==P::PressClose)screen=1;if(r.phase==P::Complete)break;
  }
  check(released&&altar.phase()==P::Complete&&presses==std::vector<uint16_t>{0x8000,0x2000},"altar only interacts and cancels, releases across screen transition");
 }
 for(uint8_t screen:{2,4,13,14,15,16,17}){F altar(false,true);altar.update(0,true,1,0,true);altar.update(500,true,1,0,true);auto r=altar.update(600,true,screen,0,true);check(r.phase==P::Aborted&&r.buttons==0,"unknown/upgrade/destructive altar pages abort without confirmation");}
 std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}
