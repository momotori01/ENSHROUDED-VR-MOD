#include <evr/vr_settings.hpp>
#include <iostream>
#include <stdexcept>
using namespace evr::settings;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 const std::string ini="; Enshrouded VR\r\n[Mode]\r\nVR=1   ; 1 = headset, 0 = monitor\r\n\r\n[Online]\r\nSync = 0\r\nMenu=0\r\n\r\n[Picture]\r\nRenderScale=0.8\r\nDlss=Quality\r\nSharpness=0.2\r\n[Session]\r\nMinutes=240\r\n";
 // Reading: sections and keys in any letter case, a comment behind a value is not part of it.
 ck(get(ini,"Mode","VR")=="1"&&get(ini,"mode","vr")=="1"&&get(ini,"Online","Sync")=="0"&&get(ini,"Picture","Dlss")=="Quality"&&get(ini,"Mode","Nothing","x")=="x"&&get(ini,"Nowhere","VR").empty());
 const auto v=read(ini);ck(v.vr&&!v.online&&v.renderScale==.8&&v.dlss=="Quality"&&v.sharpness==.2&&v.keepLogs==8);
 // Changing one value: its line alone changes, the comment behind it and every other byte stay.
 {const auto off=set(ini,"Mode","VR","0");ck(get(off,"Mode","VR")=="0"&&off.find("VR=0   ; 1 = headset, 0 = monitor\r\n")!=std::string::npos&&off.size()==ini.size()&&!read(off).vr);
  ck(set(off,"Mode","VR","1")==ini);
  const auto on=set(ini,"online","sync","1");ck(on.find("Sync =1\r\n")!=std::string::npos&&read(on).online&&on.substr(0,on.find("Sync"))==ini.substr(0,ini.find("Sync")));}
 // A key that is not there goes to its section's end, a section that is not there to the file's end.
 {const auto more=set(ini,"Online","Extra","7");ck(get(more,"Online","Extra")=="7"&&more.find("Menu=0\r\nExtra=7\r\n\r\n[Picture]")!=std::string::npos);
  const auto added=set(ini,"New","Key","v");ck(added.substr(0,ini.size())==ini&&added.substr(ini.size())=="[New]\r\nKey=v\r\n"&&get(added,"New","Key")=="v");
  ck(set("","Mode","VR","1")=="[Mode]\r\nVR=1\r\n"&&set("[Mode]","Mode","VR","0")=="[Mode]\r\nVR=0\r\n"&&set("[A]\nx=1\n","A","x","2")=="[A]\nx=2\n"&&set("[A]\nx=1","B","y","2")=="[A]\nx=1\n[B]\ny=2\n");}
 // What the loader takes is held to what the mod accepts; anything else gives the value it came with.
 // Without a file: VR off, the other players' VR bodies on (user, 2026-10-14).
 {auto bad=set(set(set(set(ini,"Picture","RenderScale","9"),"Picture","Dlss","Best"),"Log","Keep","-1"),"Mode","VR","yes");const auto b=read(bad);
  ck(!b.vr&&b.renderScale==.8&&b.dlss=="Quality"&&b.keepLogs==8);
  const auto ok=read(set(set(set(ini,"Picture","RenderScale","1.0"),"Picture","Dlss","performance"),"Log","Keep","3"));ck(ok.vr&&ok.renderScale==1.&&ok.dlss=="Performance"&&ok.keepLogs==3);
  ck(!read("").vr&&read("").online&&read("").keepLogs==8&&read("garbage\n=\n[").dlss=="Quality"&&read(set(ini,"Log","Keep","0")).keepLogs==0);
  // One eye a frame: off unless [Picture] AlternateEyes=1.
  ck(!read(ini).alternateEyes&&read(set(ini,"Picture","AlternateEyes","1")).alternateEyes&&!read(set(ini,"Picture","AlternateEyes","2")).alternateEyes);
  // The GI quality of VR: High unless [Picture] GiQuality names another known one (2026-10-15).
  ck(read(ini).giQuality=="High"&&read(set(ini,"Picture","GiQuality","ultra")).giQuality=="Ultra"&&read(set(ini,"Picture","GiQuality","Keep")).giQuality=="Keep"&&read(set(ini,"Picture","GiQuality","Disabled")).giQuality=="High");
  // The warp of the eye left out: off unless [Picture] EyeWarp=1 (2026-10-15).
  ck(read(ini).eyeWarp==0&&read(set(ini,"Picture","EyeWarp","1")).eyeWarp==1&&read(set(ini,"Picture","EyeWarp","2")).eyeWarp==2&&read(set(ini,"Picture","EyeWarp","3")).eyeWarp==0);}
 // Sneaking by crouching: on at 30 % unless [Body] says otherwise; a threshold out of 10 .. 50 % gives 30 (2026-10-18).
 ck(read(ini).realCrouch&&read(ini).crouchDrop==30&&!read(set(ini,"Body","RealCrouch","0")).realCrouch&&read(set(ini,"Body","CrouchDrop","25")).crouchDrop==25&&
    read(set(ini,"Body","CrouchDrop","60")).crouchDrop==30&&read(set(ini,"Body","CrouchDrop","5")).crouchDrop==30&&read(set(ini,"Body","RealCrouch","x")).realCrouch);
 // The stroke's speed (50 .. 150 %, steps of 5) and swimming by the arms: 100 % and on unless set (2026-10-20).
 ck(read(ini).swingSpeed==100&&read(ini).armSwim&&read(set(ini,"Melee","SwingSpeed","80")).swingSpeed==80&&read(set(ini,"Melee","SwingSpeed","83")).swingSpeed==85&&
    read(set(ini,"Melee","SwingSpeed","200")).swingSpeed==100&&read(set(ini,"Melee","SwingSpeed","45")).swingSpeed==100&&read(set(ini,"Melee","SwingSpeed","x")).swingSpeed==100&&
    !read(set(ini,"Body","ArmSwim","0")).armSwim&&clampSwingSpeed(149)==150&&clampSwingSpeed(10)==50);
 // A whole section, in the file's order ([Environment] of the loader).
 {const auto e=entries("[Mode]\r\nVR=1\r\n[Environment]\r\n; a comment\r\nEVR_GRIP_TRIM=\r\nXR_RUNTIME_JSON = C:\\x y\\mock.json   ; test\r\n[Other]\r\nA=1\r\n","environment");
  ck(e.size()==2&&e[0].first=="EVR_GRIP_TRIM"&&e[0].second.empty()&&e[1].first=="XR_RUNTIME_JSON"&&e[1].second=="C:\\x y\\mock.json"&&entries(ini,"Nothing").empty());}
 ck(text(.8)=="0.8"&&text(1.)=="1.0"&&text(.25)=="0.25"&&text(.7)=="0.7");
 std::cout<<"vr_settings "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"vr_settings FAIL "<<e.what()<<"\n";return 1;}}
