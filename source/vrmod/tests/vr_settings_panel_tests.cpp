#include <evr/vr_settings_panel.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::vrpanel;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 // The threshold: 10 .. 50 %, any whole percent; 30 % down enters under 0.7 of the standing eye height.
 {Body b;ck(b.realCrouch&&b.crouchDrop==30&&enterRatio(30)>.699f&&enterRatio(30)<.701f&&enterRatio(70)>.499f&&enterRatio(70)<.501f&&enterRatio(5)>.899f);
  b=apply(b,Hit::CrouchOff);ck(!b.realCrouch&&b.crouchDrop==30);b=apply(b,Hit::CrouchOn);ck(b.realCrouch);
  ck(apply(b,Hit::Settings).crouchDrop==30&&apply(b,Hit::BugReport).realCrouch&&apply(b,Hit::DropSlider).crouchDrop==30);}
 // The slider: the track's ends are 10 and 50 %, past them the same; every percent between can be reached and comes back.
 {ck(sliderDrop(trackX0)==10&&sliderDrop(trackX1)==50&&sliderDrop(dropSlider.x0)==10&&sliderDrop(dropSlider.x1)==50&&sliderDrop(-500)==10&&sliderDrop(5000)==50);
  ck(sliderDrop((trackX0+trackX1)/2)==30);
  for(int d=dropLow;d<=dropHigh;++d)ck(sliderDrop(sliderX(d))==d&&sliderX(d)>=trackX0&&sliderX(d)<=trackX1);
  ck(sliderX(5)==trackX0&&sliderX(99)==trackX1&&trackX0>dropSlider.x0&&trackX1<dropSlider.x1);}
 // Written into the settings file: only [Body]'s two lines change; read back as the loader reads them.
 {const std::string ini="[Mode]\r\nVR=1\r\n\r\n[Log]\r\nKeep=8\r\n";
  const auto out=store(ini,{false,27});ck(out.substr(0,ini.size())==ini&&out.substr(ini.size())=="[Body]\r\nRealCrouch=0\r\nCrouchDrop=27\r\n");
  const auto v=evr::settings::read(out);ck(!v.realCrouch&&v.crouchDrop==27&&v.vr&&v.keepLogs==8);
  const auto again=store(out,{true,43});ck(again.size()==out.size()&&evr::settings::read(again).realCrouch&&evr::settings::read(again).crouchDrop==43);}
 // The places: the two buttons always, the rows only while the settings are open.
 {ck(hitAt(10,10,false)==Hit::BugReport&&hitAt(700,40,false)==Hit::Settings&&hitAt(650,40,true)==Hit::None);
  ck(hitAt(720,150,false)==Hit::None&&hitAt(720,150,true)==Hit::CrouchOn&&hitAt(900,150,true)==Hit::CrouchOff&&hitAt(720,220,true)==Hit::DropSlider&&hitAt(1100,220,true)==Hit::DropSlider);
  ck(hitAt(760,300,true)==Hit::None&&hitAt(1050,300,true)==Hit::None);   // nothing under the slider
  {const auto all=boxes(true);for(size_t i=0;i<all.size();++i)for(size_t j=i+1;j<all.size();++j){const auto& a=all[i];const auto& b=all[j];ck(a.x1<=b.x0||b.x1<=a.x0||a.y1<=b.y0||b.y1<=a.y0);}}   // no two overlap
  for(const auto& b:boxes(true))ck(b.y1<=noteTop);
  for(const auto& b:boxes(true))ck(b.x0>=0&&b.y0>=0&&b.x1<=width&&b.y1<=height&&b.x0<b.x1&&b.y0<b.y1);}
 ck(hitNamed("borrow_fog")==Hit::None);   // the borrowing's switches are gone (2026-10-18 12:40)
 // The Mock's presses (the slider with its value).
 {const auto p=testPresses("900:slider=42; 400:settings;bad;0:settings;500:crouch_off;600:nothing;700:slider;800:slider=x");
  ck(p.size()==3&&p[0].frame==400&&p[0].hit==Hit::Settings&&p[1].hit==Hit::CrouchOff&&p[2].frame==900&&p[2].hit==Hit::DropSlider&&p[2].value==42);
  ck(testPresses("").empty()&&hitNamed("crouch_on")==Hit::CrouchOn&&std::string(name(Hit::DropSlider))=="slider");}
 ck(std::wstring(texts("japanese").button)==L"VR設定"&&std::wstring(texts("german").button)==L"VR settings");
 std::cout<<"vr_settings_panel "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"vr_settings_panel FAIL "<<e.what()<<"\n";return 1;}}
