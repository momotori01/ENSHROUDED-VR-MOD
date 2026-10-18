// Protocol and injected-failure tests. No-op eye/UI completions here are NOT GPU image evidence.
#include <evr/openxr_client.hpp>
#include <iostream>
#include <filesystem>
#include <limits>
#include <set>
using namespace evr;
namespace {
PFN_xrEndFrame realEnd{};
PFN_xrWaitSwapchainImage realWait{};
PFN_xrReleaseSwapchainImage realRelease{};
PFN_xrCreateSwapchain realCreate{};
PFN_xrDestroySwapchain realDestroy{};
PFN_xrEnumerateSwapchainImages realImages{};
PFN_xrGetSystemProperties realProperties{};
XrSwapchain ui{};
int waitFault=0,releaseFault=0,layerFault=0;
bool createFault=false,imagesFault=false,limitLayers=false;
std::set<XrSwapchain> alive;
unsigned ended=0,quads=0,empty=0,checks=0;
void require(bool value,const char* what){if(!value)throw std::runtime_error(what);++checks;std::cout<<"PASS "<<what<<'\n';}
template<class F> void rejects(F fn,const char* what){bool rejected=false;try{fn();}catch(const std::exception&){rejected=true;}require(rejected,what);}
XrResult XRAPI_CALL create(XrSession s,const XrSwapchainCreateInfo* i,XrSwapchain* h){
    if(createFault){createFault=false;return XR_ERROR_RUNTIME_FAILURE;}
    auto r=realCreate(s,i,h);if(XR_SUCCEEDED(r))alive.insert(*h);return r;
}
XrResult XRAPI_CALL destroy(XrSwapchain h){auto r=realDestroy(h);if(XR_SUCCEEDED(r))alive.erase(h);return r;}
XrResult XRAPI_CALL images(XrSwapchain h,uint32_t c,uint32_t* n,XrSwapchainImageBaseHeader* i){
    if(imagesFault){imagesFault=false;return XR_ERROR_RUNTIME_FAILURE;}return realImages(h,c,n,i);
}
XrResult XRAPI_CALL properties(XrInstance i,XrSystemId s,XrSystemProperties* p){auto r=realProperties(i,s,p);if(limitLayers)p->graphicsProperties.maxLayerCount=1;return r;}
XrResult XRAPI_CALL wait(XrSwapchain h,const XrSwapchainImageWaitInfo* i){
    if(h==ui && waitFault){if(waitFault>0)--waitFault;return XR_TIMEOUT_EXPIRED;}return realWait(h,i);
}
XrResult XRAPI_CALL release(XrSwapchain h,const XrSwapchainImageReleaseInfo* i){
    if(h==ui && releaseFault){--releaseFault;return XR_ERROR_RUNTIME_FAILURE;}return realRelease(h,i);
}
XrResult XRAPI_CALL end(XrSession s,const XrFrameEndInfo* i){
    ++ended;if(!i->layerCount)++empty;
    if(i->layerCount!=2)return realEnd(s,i);
    ++quads;
    auto quad=*reinterpret_cast<const XrCompositionLayerQuad*>(i->layers[1]);
    require(quad.type==XR_TYPE_COMPOSITION_LAYER_QUAD && quad.eyeVisibility==XR_EYE_VISIBILITY_BOTH,"quad is standard both-eye layer");
    require(quad.layerFlags==XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT,"premultiplied alpha flag retained");
    require(quad.subImage.swapchain==ui && quad.pose.position.z<0,"dedicated UI swapchain faces viewer");
    auto info=*i;std::array<const XrCompositionLayerBaseHeader*,3> layers{i->layers[0],reinterpret_cast<const XrCompositionLayerBaseHeader*>(&quad),nullptr};info.layers=layers.data();
    switch(layerFault){
    case 1:quad.space=XR_NULL_HANDLE;break;
    case 2:quad.size.width=0;break;
    case 3:quad.size.height=std::numeric_limits<float>::quiet_NaN();break;
    case 4:quad.pose.orientation.w=0;break;
    case 5:quad.pose.position.x=std::numeric_limits<float>::infinity();break;
    case 6:quad.subImage.imageRect.offset.x=-1;break;
    case 7:quad.subImage.imageRect.extent.width=INT32_MAX;break;
    case 8:quad.subImage.imageArrayIndex=1;break;
    case 9:quad.eyeVisibility=static_cast<XrEyeVisibility>(55);break;
    case 10:quad.layerFlags=XrCompositionLayerFlags(1)<<63;break;
    case 11:quad.type=XR_TYPE_COMPOSITION_LAYER_PROJECTION;break;
    case 12:quad.subImage.swapchain=reinterpret_cast<const XrCompositionLayerProjection*>(i->layers[0])->views[0].subImage.swapchain;break;
    case 13:info.layerCount=3;break;
    case 14:{uint32_t index;XrSwapchainImageAcquireInfo a{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};xrCheck(xrAcquireSwapchainImage(ui,&a,&index),"negative acquire");break;}
    }
    return realEnd(s,&info);
}
XrResult XRAPI_CALL get(XrInstance i,const char* n,PFN_xrVoidFunction* out){
    auto r=xrGetInstanceProcAddr(i,n,out);if(XR_FAILED(r))return r;
#define WRAP(name,field,fn) if(std::strcmp(n,#name)==0){field=reinterpret_cast<PFN_##name>(*out);*out=reinterpret_cast<PFN_xrVoidFunction>(&fn);}
    WRAP(xrEndFrame,realEnd,end) WRAP(xrWaitSwapchainImage,realWait,wait) WRAP(xrReleaseSwapchainImage,realRelease,release)
    WRAP(xrCreateSwapchain,realCreate,create) WRAP(xrDestroySwapchain,realDestroy,destroy)
    WRAP(xrEnumerateSwapchainImages,realImages,images) WRAP(xrGetSystemProperties,realProperties,properties)
#undef WRAP
    return r;
}
uint64_t begin(OpenXrClient& xr){require(xr.beginFrame(),"frame begins");return xr.pendingFrame().frame;}
void eyes(OpenXrClient& xr){auto frame=xr.pendingFrame().frame;xr.markEyeRendered(0,frame);xr.markEyeRendered(1,frame);}
void finish(OpenXrClient& xr,bool withUi){auto f=begin(xr);if(withUi){xr.acquireUi(f);xr.markUiRendered(f);}eyes(xr);xr.finishFrame();}
}
int main(){
    try{
        std::filesystem::create_directories("ui-api-validation");Log log("ui-api-validation/client.jsonl",true);
        {
            OpenXrClient xr(log,&get);xr.init(1);xr.poll();
            rejects([&]{xr.acquireUi(1);},"UI acquisition outside a frame rejected");
            rejects([&]{xr.configureUi(0,200,VK_FORMAT_R8G8B8A8_SRGB);},"zero UI extent rejected");
            rejects([&]{xr.configureUi(4097,200,VK_FORMAT_R8G8B8A8_SRGB);},"Runtime extent limit enforced");
            rejects([&]{xr.configureUi(400,200,VK_FORMAT_R16G16B16A16_SFLOAT);},"unsupported UI format rejected");
            xr.configureUi(400,200,VK_FORMAT_R8G8B8A8_SRGB);ui=xr.uiSwapchain().handle;
            require(alive.size()==3,"two eye swapchains and one UI owned");
            xr.configureUi(400,200,VK_FORMAT_R8G8B8A8_SRGB);require(ui==xr.uiSwapchain().handle,"unchanged UI configuration reuses target");
            auto f=begin(xr);rejects([&]{xr.acquireUi(f-1);},"stale UI frame refused");
            xr.acquireUi(f);rejects([&]{xr.acquireUi(f);},"duplicate UI acquire refused");
            rejects([&]{xr.configureUi(300,200,VK_FORMAT_R8G8B8A8_SRGB);},"resize while acquired refused");
            rejects([&]{xr.markUiRendered(f-1);},"stale UI completion refused");
            rejects([&]{xr.markUiRendered(f,{0,2});},"invalid UI distance refused");
            rejects([&]{xr.markUiRendered(f,{1,std::numeric_limits<float>::infinity()});},"infinite UI size refused");
            xr.markUiRendered(f);rejects([&]{xr.markUiRendered(f);},"duplicate UI completion refused");
            eyes(xr);xr.finishFrame();require(!xr.hasPendingFrame(),"UI and eyes complete in one frame");
            const auto q=quads;finish(xr,false);require(quads==q,"next frame never reuses stale UI");
            for(int i=0;i<7;i++)finish(xr,true);
            xr.configureUi(320,180,VK_FORMAT_R8G8B8A8_UNORM);ui=xr.uiSwapchain().handle;
            require(alive.size()==3 && xr.uiSwapchain().width==320 && xr.uiSwapchainFormat()==VK_FORMAT_R8G8B8A8_UNORM,"resize replaces old swapchain and explicit format");
            createFault=true;rejects([&]{xr.configureUi(420,200,VK_FORMAT_R8G8B8A8_UNORM);},"create failure propagated");
            require(alive.size()==3 && ui==xr.uiSwapchain().handle,"failed create preserves previous UI");
            imagesFault=true;rejects([&]{xr.configureUi(420,200,VK_FORMAT_R8G8B8A8_UNORM);},"enumeration failure propagated");
            require(alive.size()==3 && ui==xr.uiSwapchain().handle,"failed enumeration destroys provisional swapchain");
            f=begin(xr);xr.acquireUi(f);eyes(xr);rejects([&]{xr.finishFrame();},"unfinished UI prevents submission");
            require(!xr.hasPendingFrame(),"unfinished UI releases pending acquisitions");finish(xr,true);
            f=begin(xr);xr.acquireUi(f);xr.skipFrame("fixture_skip");require(!xr.hasPendingFrame(),"skip releases acquired UI");finish(xr,true);
            f=begin(xr);waitFault=1;rejects([&]{xr.acquireUi(f);},"positive timeout is not mistaken for success");
            require(!xr.hasPendingFrame(),"transient UI timeout aborts frame and retries cleanup wait");finish(xr,true);
            f=begin(xr);xr.acquireUi(f);xr.markUiRendered(f);eyes(xr);releaseFault=1;
            rejects([&]{xr.finishFrame();},"release failure aborts frame");require(!xr.hasPendingFrame(),"release retry succeeds during abort");finish(xr,true);
            for(layerFault=1;layerFault<=14;++layerFault){
                f=begin(xr);xr.acquireUi(f);xr.markUiRendered(f);eyes(xr);auto e=ended;
                rejects([&]{xr.finishFrame();},"Mock rejects malformed or unreleased quad");
                require(ended==e+1 && !xr.hasPendingFrame(),"failed xrEndFrame never submitted twice");
                if(layerFault==14){XrSwapchainImageWaitInfo w{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};w.timeout=1000000;xrCheck(realWait(ui,&w),"negative cleanup wait");XrSwapchainImageReleaseInfo r{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};xrCheck(realRelease(ui,&r),"negative cleanup release");}
            }
            layerFault=0;finish(xr,true);
            f=begin(xr);waitFault=-1;rejects([&]{xr.acquireUi(f);},"persistent UI wait timeout propagated");
            rejects([&]{xr.beginFrame();},"unresolved UI acquisition blocks reuse");
            xr.shutdown();waitFault=0;require(alive.empty(),"shutdown destroys persistently acquired UI after GPU idle");
        }
        {
            OpenXrClient xr(log,&get);limitLayers=true;xr.init(.5f);xr.poll();limitLayers=false;
            rejects([&]{xr.configureUi(100,100,VK_FORMAT_R8G8B8A8_UNORM);},"Runtime maxLayerCount enforced");
            finish(xr,false);xr.shutdown();require(alive.empty(),"projection-only fallback and session recreation clean");
        }
        {
            OpenXrClient xr(log,&get);xr.init(1);xr.poll();xr.configureUi(200,100,VK_FORMAT_R8G8B8A8_SRGB);ui=xr.uiSwapchain().handle;
            auto f=begin(xr);xr.acquireUi(f);xr.shutdown();require(alive.empty(),"pending UI shutdown releases all ownership");
        }
        std::cout<<"RESULT "<<checks<<" PASS 0 FAIL (protocol only)\n";return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
