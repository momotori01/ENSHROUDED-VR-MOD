// OpenXR screen protocol tests. Completion markers here are NOT GPU/video evidence.
#include <evr/openxr_client.hpp>
#include <iostream>
#include <filesystem>
#include <limits>
using namespace evr;
namespace {
PFN_xrWaitFrame realWaitFrame{};PFN_xrLocateViews realLocate{};bool invisible=false,untracked=false;
PFN_xrEndFrame realEnd{};PFN_xrAcquireSwapchainImage realAcquire{};PFN_xrWaitSwapchainImage realWait{};
std::array<XrSwapchain,2> eyes{};XrSwapchain ui{};XrSpace local{};
unsigned checks=0,eyeAcquires=0,uiAcquires=0,worldFrames=0,screenFrames=0,emptyFrames=0;
int waitFault=0,layerFault=0;bool checkLocal=false;Pose expected{};
void require(bool value,const char* what){if(!value)throw std::runtime_error(what);++checks;std::cout<<"PASS "<<what<<'\n';}
template<class F>void rejects(F f,const char* what){bool no=false;try{f();}catch(const std::exception&){no=true;}require(no,what);}
XrResult XRAPI_CALL acquire(XrSwapchain h,const XrSwapchainImageAcquireInfo* i,uint32_t* n){
 if(h==eyes[0]||h==eyes[1])++eyeAcquires;else if(h==ui)++uiAcquires;return realAcquire(h,i,n);
}
XrResult XRAPI_CALL wait(XrSwapchain h,const XrSwapchainImageWaitInfo* i){if(h==ui&&waitFault){--waitFault;return XR_TIMEOUT_EXPIRED;}return realWait(h,i);}
XrResult XRAPI_CALL waitFrame(XrSession s,const XrFrameWaitInfo* i,XrFrameState* o){auto r=realWaitFrame(s,i,o);if(invisible)o->shouldRender=XR_FALSE;return r;}
XrResult XRAPI_CALL locate(XrSession s,const XrViewLocateInfo* i,XrViewState* v,uint32_t c,uint32_t* n,XrView* o){auto r=realLocate(s,i,v,c,n,o);if(untracked)v->viewStateFlags=0;return r;}
XrResult XRAPI_CALL end(XrSession s,const XrFrameEndInfo* i){
 if(!i->layerCount){++emptyFrames;return realEnd(s,i);}
 if(i->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_PROJECTION){++worldFrames;return realEnd(s,i);}
 require(i->layerCount==1&&i->layers[0]->type==XR_TYPE_COMPOSITION_LAYER_QUAD,"screen submits only one quad without a world projection");
 const auto& q=*reinterpret_cast<const XrCompositionLayerQuad*>(i->layers[0]);
 require(q.subImage.swapchain==ui&&q.eyeVisibility==XR_EYE_VISIBILITY_BOTH,"screen references current dedicated image for both eyes");
 if(checkLocal){
  require(q.space==local,"screen is anchored in LOCAL tracking space");
  require(length(q.pose.position-expected.position)<1e-6f&&length(rotate(q.pose.orientation,{0,0,-1})-rotate(expected.orientation,{0,0,-1}))<1e-6f,"frozen screen origin survives subsequent HMD poses");
 }
 ++screenFrames;auto changed=q;auto info=*i;const XrCompositionLayerBaseHeader* layer=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&changed);info.layers=&layer;
 if(layerFault==1)changed.space=XR_NULL_HANDLE;
 if(layerFault==2)changed.size.width=0;
 if(layerFault==3)changed.subImage.imageRect.extent.width=INT32_MAX;
 return realEnd(s,&info);
}
XrResult XRAPI_CALL get(XrInstance i,const char* n,PFN_xrVoidFunction* out){
 auto r=xrGetInstanceProcAddr(i,n,out);if(XR_FAILED(r))return r;
#define W(name,field,fn) if(!std::strcmp(n,#name)){field=reinterpret_cast<PFN_##name>(*out);*out=reinterpret_cast<PFN_xrVoidFunction>(&fn);}
 W(xrWaitFrame,realWaitFrame,waitFrame) W(xrLocateViews,realLocate,locate)
 W(xrEndFrame,realEnd,end) W(xrAcquireSwapchainImage,realAcquire,acquire) W(xrWaitSwapchainImage,realWait,wait)
#undef W
 return r;
}
uint64_t screen(OpenXrClient& xr){require(xr.beginScreenFrame(),"screen frame begins");auto f=xr.pendingFrame();require(f.screenOnly&&f.shouldRender,"explicit screen frame retains valid tracking");return f.frame;}
void world(OpenXrClient& xr){require(xr.beginFrame(),"world frame resumes");auto f=xr.pendingFrame();require(!f.screenOnly,"default is native stereo, not screen");xr.markEyeRendered(0,f.frame);xr.markEyeRendered(1,f.frame);xr.finishFrame();}
}
int main(){
 try{
  std::filesystem::create_directories("screen-api-validation");Log log("screen-api-validation/client.jsonl",true);
  OpenXrClient xr(log,&get);xr.init(.25f);xr.poll();eyes={xr.targets[0].handle,xr.targets[1].handle};local=xr.local;
  xr.configureUi(320,180,VK_FORMAT_R8G8B8A8_SRGB);ui=xr.uiSwapchain().handle;
  world(xr);const auto originalSession=xr.session;const auto draws=xr.draws[0];const auto beforeEyes=eyeAcquires;
  UiQuadPlacement placement;placement.distance=2;placement.width=3;placement.localSpace=true;placement.origin={euler(.5f,0,0),{1,1.6f,2}};
  expected=compose(placement.origin,Pose{{0,0,0,1},{0,0,-2}});checkLocal=true;
  const auto firstHead=xr.headLocation.pose;
  for(unsigned n=0;n<20;++n){auto f=screen(xr);xr.acquireUi(f);xr.markUiRendered(f,placement);xr.finishFrame();}
  require(length(xr.headLocation.pose.position-firstHead.position)>1e-6f||length(rotate(xr.headLocation.pose.orientation,{0,0,-1})-rotate(firstHead.orientation,{0,0,-1}))>1e-6f,"test actually changes tracked HMD pose while screen stays fixed");
  require(eyeAcquires==beforeEyes&&xr.draws[0]==draws&&xr.draws[1]==draws,"screen sequence acquires/renders zero world eye images");
  require(xr.session==originalSession&&xr.local==local,"world-screen transition preserves session and tracking space");
  auto f=screen(xr);rejects([&]{xr.markEyeRendered(0,f);},"fake screen eye completion rejected");
  rejects([&]{xr.finishFrame();},"screen without new content rejected rather than reusing an old image");require(!xr.hasPendingFrame(),"missing screen content aborts cleanly");
  f=screen(xr);xr.acquireUi(f);rejects([&]{xr.finishFrame();},"unfinished screen GPU completion rejected");
  f=screen(xr);xr.acquireUi(f);auto invalid=placement;invalid.origin.position.y=std::numeric_limits<float>::quiet_NaN();
  rejects([&]{xr.markUiRendered(f,invalid);},"nonfinite local screen origin rejected");invalid=placement;invalid.origin.orientation={0,0,0,0};
  rejects([&]{xr.markUiRendered(f,invalid);},"invalid local screen rotation rejected");invalid=placement;invalid.distance=std::numeric_limits<float>::max();invalid.origin.position.z=-std::numeric_limits<float>::max();invalid.origin.orientation={0,0,0,1};
  rejects([&]{xr.markUiRendered(f,invalid);},"finite inputs whose composed screen position overflows are rejected");xr.markUiRendered(f,placement);xr.finishFrame();
  for(int condition=0;condition<2;++condition){
   invisible=condition==0;untracked=condition==1;const auto prevUi=uiAcquires,prevEyes=eyeAcquires,prevEmpty=emptyFrames;
   require(xr.beginScreenFrame(),"hidden/tracking-lost screen frame begins");require(!xr.pendingFrame().shouldRender,"hidden/tracking-lost screen submits no content");
   xr.finishFrame();require(uiAcquires==prevUi&&eyeAcquires==prevEyes&&emptyFrames==prevEmpty+1,"empty screen balances frame without any swapchain acquisition");
  }invisible=untracked=false;
  f=screen(xr);waitFault=1;rejects([&]{xr.acquireUi(f);},"screen wait timeout fails and releases during abort");require(!xr.hasPendingFrame(),"screen timeout leaves no pending frame");
  f=screen(xr);xr.acquireUi(f);xr.skipFrame("screen_not_ready");require(!xr.hasPendingFrame(),"screen skip ends an empty frame without stale content");
  for(layerFault=1;layerFault<=3;++layerFault){f=screen(xr);xr.acquireUi(f);xr.markUiRendered(f,placement);rejects([&]{xr.finishFrame();},"Mock rejects invalid screen-only layers");}layerFault=0;checkLocal=false;
  require(xr.beginFrame(),"negative world frame begins");f=xr.pendingFrame().frame;xr.acquireUi(f);xr.markUiRendered(f);
  rejects([&]{xr.finishFrame();},"UI cannot disguise a missing stereo world pair");
  world(xr);require(worldFrames==2&&xr.draws[0]==draws+1&&xr.draws[1]==draws+1,"return from screen draws and submits both eyes again");
  require(xr.session==originalSession,"screen return keeps the same session");
  xr.shutdown();require(!xr.uiSwapchain().handle&&!xr.hasPendingFrame(),"screen/session teardown releases ownership");
  std::cout<<"RESULT "<<checks<<" PASS 0 FAIL (protocol only; no game media)\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
