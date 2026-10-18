#include <evr/native_xr_bridge.hpp>
#include <windows.h>
#include <cstring>
#include <iostream>
#include <vector>
using namespace evr;
struct LegacyHandSample {
 Pose grip=identityPose(),aim=identityPose();XrSpaceLocationFlags gripFlags=0,aimFlags=0;XrPath profile=XR_NULL_PATH;
 float trigger=0;XrVector2f stick{};bool stickActive=false,stickClickActive=false,stickClick=false;
 bool gripActive=false,aimActive=false,triggerActive=false,grabActive=false,grab=false;
};
struct LegacyHandFrame {uint64_t frame=0,sessionGeneration=0,originGeneration=0;XrTime predictedTime=0;bool focused=false;std::array<LegacyHandSample,2> hands;};
struct LegacyNativeXrFrame {
 uint32_t structSize=sizeof(LegacyNativeXrFrame),diagnosticFrames=1;std::array<uint32_t,2> width{},height{};
 uint64_t frame=0;XrTime time=0;XrPosef head{{0,0,0,1},{0,0,0}};
 std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};XrSpaceLocationFlags headFlags=0;uint64_t sessionGeneration=0;LegacyHandFrame hands;
};
int main(int argc,char** argv){unsigned checks=0;HMODULE layer=nullptr;try{
 auto check=[&](bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);};
 if(argc!=2)throw std::runtime_error("layer path required");
 check(sizeof(LegacyNativeXrFrame)!=sizeof(diagnostic::NativeXrFrame),"face fields must change native bridge size");
 _putenv_s("EVR_LIVE_NATIVE_XR","1");layer=LoadLibraryA(argv[1]);if(!layer)throw std::runtime_error("load diagnostic layer");
 auto begin=reinterpret_cast<diagnostic::BeginNativeXr>(GetProcAddress(layer,"evrBeginNativeXr"));check(begin!=nullptr,"begin exported");
 for(uint32_t size:{uint32_t(0),uint32_t(4),uint32_t(sizeof(LegacyNativeXrFrame)),uint32_t(sizeof(diagnostic::NativeXrFrame)+8)}){
  alignas(diagnostic::NativeXrFrame) std::array<unsigned char,sizeof(diagnostic::NativeXrFrame)+64> bytes;
  bytes.fill(0xa5);std::memcpy(bytes.data(),&size,sizeof(size));auto saved=bytes;
  check(begin(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,0,1,reinterpret_cast<diagnostic::NativeXrFrame*>(bytes.data()))==2,"old/invalid size refused");
  check(bytes==saved,"size refusal cannot write caller buffer");
 }
 for(uint64_t version:{uint64_t(0),uint64_t(1),uint64_t(2),diagnostic::nativeXrFrameAbiVersion+1}){
  diagnostic::NativeXrFrame bad;bad.abiVersion=version;bad.frame=123;const auto saved=bad;
  check(begin(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,0,1,&bad)==2,"old/unknown ABI refused");
  check(std::memcmp(&bad,&saved,sizeof(bad))==0,"ABI refusal cannot write caller buffer");
 }
 diagnostic::NativeXrFrame current;current.diagnosticFrames=0;current.frame=123;
 check(begin(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,0,1,&current)==2,"invalid frame limit refused before GPU use");
 check(current.frame==0&&current.structSize==sizeof(current),"current layout passes size gate and is initialized");
 check(begin(VK_NULL_HANDLE,VK_NULL_HANDLE,VK_NULL_HANDLE,0,1,nullptr)==2,"null rejected");
 FreeLibrary(layer);layer=nullptr;_putenv_s("EVR_LIVE_NATIVE_XR","");
 std::cout<<"PASS "<<checks<<" native frame ABI checks; legacy="<<sizeof(LegacyNativeXrFrame)<<" current="<<sizeof(diagnostic::NativeXrFrame)<<"\n";return 0;
}catch(const std::exception& e){if(layer)FreeLibrary(layer);std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
