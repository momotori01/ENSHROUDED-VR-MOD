#include <evr/fov_fit.hpp>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::fovfit;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 auto within1=[](int32_t a,int32_t b){return std::abs(a-b)<=1;};
 // the same field: the whole picture
 {const Angles a{-.9f,.9f,.8f,-.8f};const auto r=part(a,a,2000,1800);ck(r.x0==0&&r.y0==0&&r.x1==2000&&r.y1==1800&&r.valid());}
 // the ALVR player's report 20261024-140648: drawn the symmetric -1.0238..1.0238 x 0.8396..-0.8396, the runtime's left eye
 // -1.0204..0.7475 x 0.8368..-0.7475 in 2832 x 2736: 10.7, 7.6, 2215.3, 2505.5 (worked out beside)
 {const Angles drawn{-1.023804426f,1.023804426f,.8395776153f,-.8395776153f};
  const auto l=part(drawn,{-1.02043879f,.7474797368f,.8368179202f,-.7474796176f},2832,2736);
  ck(within1(l.x0,11)&&within1(l.y0,8)&&within1(l.x1,2215)&&within1(l.y1,2506)&&l.valid());
  // the right eye: its inner side on the left
  const auto r=part(drawn,{-.7499f,1.0238f,.8396f,-.7499f},2832,2736);ck(within1(r.x0,613)&&r.y0==0&&within1(r.x1,2832)&&within1(r.y1,2511));}
 // a field wider than the picture is cut at the picture's edge; a picture without a field gives nothing
 {const auto r=part({-.5f,.5f,.5f,-.5f},{-1.f,1.f,1.f,-1.f},100,100);ck(r.x0==0&&r.y0==0&&r.x1==100&&r.y1==100);
  ck(!part({.5f,-.5f,.5f,-.5f},{-.1f,.1f,.1f,-.1f},100,100).valid());ck(!part({-.5f,.5f,.5f,-.5f},{-.1f,.1f,.1f,-.1f},0,100).valid());}
 // the system name: ALVR in any case; the others not
 ck(alvrName("SteamVR/OpenXR : alvr_server")&&alvrName("ALVR")&&alvrName("Quest via Alvr"));
 ck(!alvrName("SteamVR/OpenXR : lighthouse")&&!alvrName("Oculus Quest2")&&!alvrName(""));
 // runtimes (openxr_client.hpp Auto, 2026-10-27): the Vision Pro's ALVR client has its own; Virtual Desktop's and the others are not ALVR
 ck(alvrName("VisionALVR (VDXR)")&&!alvrName("VirtualDesktopXR")&&!alvrName("Oculus")&&!alvrName("SteamVR/OpenXR")&&!alvrName("Windows Mixed Reality Runtime"));
 // the module of a process: this test's own process has kernel32.dll and not ALVR's driver; no process of a made-up name
 {wchar_t self[MAX_PATH]{};GetModuleFileNameW(nullptr,self,MAX_PATH);const wchar_t* name=wcsrchr(self,L'\\');name=name?name+1:self;
  ck(processHasModule(name,L"kernel32.dll")==1);ck(processHasModule(name,L"driver_alvr_server.dll")==0);
  ck(processHasModule(L"no-such-process-evr.exe",L"kernel32.dll")==-1);}
 std::cout<<"RESULT "<<n<<" PASS 0 FAIL\n";return 0;}
 catch(const std::exception& e){std::cout<<"FAIL "<<e.what()<<"\n";return 1;}}
