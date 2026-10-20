#include <evr/vr_settings_panel.hpp>
#include <cwchar>
#include <iostream>
#include <stdexcept>
using namespace evr::vrpanel;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 // The threshold: 10 .. 50 %, any whole percent; 30 % down enters under 0.7 of the standing eye height.
 {Body b;ck(b.realCrouch&&b.crouchDrop==30&&enterRatio(30)>.699f&&enterRatio(30)<.701f&&enterRatio(70)>.499f&&enterRatio(70)<.501f&&enterRatio(5)>.899f);
  b=apply(b,Hit::CrouchOff);ck(!b.realCrouch&&b.crouchDrop==30);b=apply(b,Hit::CrouchOn);ck(b.realCrouch);
  ck(apply(b,Hit::Settings).crouchDrop==30&&apply(b,Hit::BugReport).realCrouch&&apply(b,Hit::DropSlider).crouchDrop==30);
  // swimming by the arms and the stroke's speed (2026-10-20): on, 100 %
  ck(b.armSwim&&b.swingSpeed==100&&!apply(b,Hit::SwimOff).armSwim&&apply(apply(b,Hit::SwimOff),Hit::SwimOn).armSwim&&apply(b,Hit::SwingSlider).swingSpeed==100);
  ck(isSlider(Hit::DropSlider)&&isSlider(Hit::SwingSlider)&&!isSlider(Hit::SwimOn)&&!isSlider(Hit::None));
  // the hit stop (17:40): on unless switched off
  ck(b.hitStop&&!apply(b,Hit::HitStopOff).hitStop&&apply(apply(b,Hit::HitStopOff),Hit::HitStopOn).hitStop&&!isSlider(Hit::HitStopOn));
  ck(hitNamed("hit_stop_on")==Hit::HitStopOn&&hitNamed("hit_stop_off")==Hit::HitStopOff);}
 // The slider: the track's ends are 10 and 50 %, past them the same; every percent between can be reached and comes back.
 {ck(sliderDrop(trackX0)==10&&sliderDrop(trackX1)==50&&sliderDrop(dropSlider.x0)==10&&sliderDrop(dropSlider.x1)==50&&sliderDrop(-500)==10&&sliderDrop(5000)==50);
  ck(sliderDrop((trackX0+trackX1)/2)==30);
  for(int d=dropLow;d<=dropHigh;++d)ck(sliderDrop(sliderX(d))==d&&sliderX(d)>=trackX0&&sliderX(d)<=trackX1);
  ck(sliderX(5)==trackX0&&sliderX(99)==trackX1&&trackX0>dropSlider.x0&&trackX1<dropSlider.x1);
  // the stroke's speed: 50 .. 150 % in steps of 5, the middle 100 %
  ck(sliderSwing(trackStart(swingSlider))==50&&sliderSwing(trackEnd(swingSlider))==150&&sliderSwing(-9)==50&&sliderSwing(9999)==150&&sliderSwing((trackStart(swingSlider)+trackEnd(swingSlider))/2)==100);
  for(int v=50;v<=150;v+=5)ck(sliderSwing(swingX(v))==v);
  ck(sliderSwing(swingX(52))==50&&swingX(33)==trackStart(swingSlider));}
 // Written into the settings file: only [Body]'s two lines change; read back as the loader reads them.
 {const std::string ini="[Mode]\r\nVR=1\r\n\r\n[Log]\r\nKeep=8\r\n";
  const auto out=store(ini,{false,27});ck(out.substr(0,ini.size())==ini&&out.find("[Body]\r\nRealCrouch=0\r\nCrouchDrop=27\r\nArmSwim=1\r\n")!=std::string::npos&&out.find("[Melee]\r\nSwingSpeed=100\r\n")!=std::string::npos);
  const auto v=evr::settings::read(out);ck(!v.realCrouch&&v.crouchDrop==27&&v.vr&&v.keepLogs==8&&out.find("LockOnMovement")==std::string::npos);
  const auto again=store(out,{true,43});ck(again.size()==out.size()&&evr::settings::read(again).realCrouch&&evr::settings::read(again).crouchDrop==43);
  const auto swim=store(again,{true,43,80,false});const auto w=evr::settings::read(swim);ck(w.swingSpeed==80&&!w.armSwim&&w.crouchDrop==43&&swim.size()==again.size()-1);
  ck(evr::settings::read(store(again,{true,43,147,true})).swingSpeed==145&&evr::settings::read(store(again,{true,43,999,true})).swingSpeed==150);
  ck(hitNamed("lock_off")==Hit::None);
  // the hit stop into [Melee] HitStop and back (on unless set)
  {Body off{true,43};off.hitStop=false;const auto h=store(again,off);ck(!evr::settings::read(h).hitStop&&h.find("HitStop=0")!=std::string::npos&&evr::settings::read(again).hitStop&&evr::settings::read(ini).hitStop);}}   // the lock-on row went with the option (2026-10-19 18:10)
 // The places: the two buttons always, the rows only while the settings are open.
 {ck(hitAt(10,10,false)==Hit::BugReport&&hitAt(700,40,false)==Hit::Settings&&hitAt(650,40,true)==Hit::None);
  // the slim rows (2026-10-20): crouch on / off, its depth, the stroke's speed, swimming on / off; the labels' side is no button
  ck(hitAt(1000,rowY(0)+26,false)==Hit::None&&hitAt(1000,rowY(0)+26,true)==Hit::CrouchOn&&hitAt(1100,rowY(0)+26,true)==Hit::CrouchOff&&hitAt(1068,rowY(0)+26,true)==Hit::None);
  ck(hitAt(720,rowY(1)+26,true)==Hit::DropSlider&&hitAt(1160,rowY(1)+26,true)==Hit::DropSlider&&hitAt(900,rowY(2)+26,true)==Hit::SwingSlider);
  ck(hitAt(1000,rowY(3)+26,true)==Hit::SwimOn&&hitAt(1100,rowY(3)+26,true)==Hit::SwimOff&&hitAt(300,rowY(2)+26,true)==Hit::None&&hitAt(1000,rowY(5)+26,true)==Hit::None);
  // the hit stop on / off (17:40), the fifth row
  ck(hitAt(1000,rowY(4)+26,true)==Hit::HitStopOn&&hitAt(1100,rowY(4)+26,true)==Hit::HitStopOff&&hitAt(1000,rowY(4)+26,false)==Hit::None);
  ck(settingsButton.y1-settingsButton.y0<buttonHeight&&settingsButton.x1-settingsButton.x0<bugButton.x1-bugButton.x0);   // only the bug report's button stands out
  {const auto all=boxes(true);for(size_t i=0;i<all.size();++i)for(size_t j=i+1;j<all.size();++j){const auto& a=all[i];const auto& b=all[j];ck(a.x1<=b.x0||b.x1<=a.x0||a.y1<=b.y0||b.y1<=a.y0);}}   // no two overlap
  for(const auto& b:boxes(true))ck(b.y1<=noteTop);
  ck(noteTop+60<=height);
  for(const auto& b:boxes(true))ck(b.x0>=0&&b.y0>=0&&b.x1<=width&&b.y1<=height&&b.x0<b.x1&&b.y0<b.y1);}
 // Every language of the bug report's button has its own texts (2026-10-19), each with the depth's one number.
 for(const char* l:{"japanese","german","french","spanish","latam","italian","polish","brazilian","portuguese","russian","ukrainian","schinese","tchinese","koreana","turkish","english","klingon"}){
  const auto x=texts(l);ck(x.button&&x.crouch&&x.on&&x.off&&x.drop&&x.note&&x.unsaved&&x.swing&&x.swim&&x.hitStop&&std::wcschr(x.hitStop,L'\n')&&std::wcschr(x.crouch,L'\n')&&std::wcschr(x.swing,L'\n')&&std::wcschr(x.swim,L'\n'));
  wchar_t b[256]{};ck(std::swprintf(b,256,x.drop,42)>0&&std::wcsstr(b,L"42")!=nullptr);
  wchar_t c[256]{};ck(std::swprintf(c,256,x.swing,135)>0&&std::wcsstr(c,L"135")!=nullptr&&std::wcschr(c,L'%')!=nullptr);
  if(std::string(l)!="english"&&std::string(l)!="klingon")ck(std::wcscmp(x.button,texts("english").button)!=0);}
 ck(hitNamed("borrow_fog")==Hit::None);   // the borrowing's switches are gone (2026-10-18 12:40)
 // The Mock's presses (the slider with its value).
 {const auto p=testPresses("900:slider=42; 400:settings;bad;0:settings;500:crouch_off;600:nothing;700:slider;800:slider=x");
  ck(p.size()==3&&p[0].frame==400&&p[0].hit==Hit::Settings&&p[1].hit==Hit::CrouchOff&&p[2].frame==900&&p[2].hit==Hit::DropSlider&&p[2].value==42);
  ck(testPresses("").empty()&&hitNamed("crouch_on")==Hit::CrouchOn&&std::string(name(Hit::DropSlider))=="slider");
  const auto q=testPresses("10:swing_slider=80;20:swing_slider;30:swim_off;40:swim_on");
  ck(q.size()==3&&q[0].hit==Hit::SwingSlider&&q[0].value==80&&q[1].hit==Hit::SwimOff&&q[2].hit==Hit::SwimOn);}
 ck(std::wstring(texts("japanese").button)==L"VR設定"&&std::wstring(texts("german").button)==L"VR-Einstellungen"&&std::wstring(texts("klingon").button)==L"VR settings");
 std::cout<<"vr_settings_panel "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"vr_settings_panel FAIL "<<e.what()<<"\n";return 1;}}
