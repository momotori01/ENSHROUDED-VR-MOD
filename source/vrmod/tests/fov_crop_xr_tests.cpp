// Visibility-mask field of view crop through the OpenXR client and the mock runtime
// (EVR_MOCK_VISIBILITY_MASK=0.85 from CTest). Protocol evidence only, not HMD or GPU evidence.
#include <evr/openxr_client.hpp>
#include <filesystem>
#include <iostream>
using namespace evr;
namespace {
PFN_xrEndFrame realEnd{};
PFN_xrLocateViews realLocate{};
std::array<XrView,2> located{};
XrResult XRAPI_CALL locate(XrSession s,const XrViewLocateInfo* i,XrViewState* state,uint32_t cap,uint32_t* count,XrView* views){
    const auto result=realLocate(s,i,state,cap,count,views);
    if(XR_SUCCEEDED(result)&&cap>=2&&*count==2)for(unsigned eye=0;eye<2;++eye)located[eye]=views[eye];
    return result;
}
std::array<XrCompositionLayerProjectionView, 2> submitted{};
unsigned checks = 0;
void require(bool value, const char *what)
{
    if (!value) throw std::runtime_error(what);
    ++checks;
    std::cout << "PASS " << what << '\n';
}
XrResult XRAPI_CALL end(XrSession s, const XrFrameEndInfo *i)
{
    if (i->layerCount && i->layers[0]->type == XR_TYPE_COMPOSITION_LAYER_PROJECTION)
    {
        const auto &p = *reinterpret_cast<const XrCompositionLayerProjection *>(i->layers[0]);
        for (uint32_t eye = 0; eye < 2; ++eye) submitted[eye] = p.views[eye];
    }
    return realEnd(s, i);
}
XrResult XRAPI_CALL get(XrInstance i, const char *n, PFN_xrVoidFunction *out)
{
    auto r = xrGetInstanceProcAddr(i, n, out);
    if (XR_SUCCEEDED(r) && !std::strcmp(n, "xrEndFrame"))
    {
        realEnd = reinterpret_cast<PFN_xrEndFrame>(*out);
        *out = reinterpret_cast<PFN_xrVoidFunction>(&end);
    }
    if(XR_SUCCEEDED(r)&&!std::strcmp(n,"xrLocateViews")){
        realLocate=reinterpret_cast<PFN_xrLocateViews>(*out);*out=reinterpret_cast<PFN_xrVoidFunction>(&locate);
    }
    return r;
}
bool recordedFov=false;
XrFovf mockFov(int eye) {
    if(recordedFov) return eye ? XrFovf{-.741855681f,.950275421f,.759112358f,-.933196187f} : XrFovf{-.958523333f,.735325396f,.769307733f,-.925051451f};
    return eye ? XrFovf{-.82f, .88f, .8f, -.85f} : XrFovf{-.88f, .82f, .8f, -.85f}; }
struct Run
{
    std::array<uint32_t, 2> width{}, height{}, fullWidth{}, fullHeight{};
    std::array<XrFovf, 2> fov{};
};
Run run(const char *logName, bool turned = false)
{
    Log log(std::string("fov-crop-validation/") + logName, true);
    OpenXrClient xr(log, &get);
    xr.init(1.f);
    Run result;
    for (int eye = 0; eye < 2; ++eye) { result.fullWidth[eye] = xr.targets[eye].width; result.fullHeight[eye] = xr.targets[eye].height; }
    for (int frame = 0; frame < 3; ++frame)
    {
        xr.poll();
        require(xr.beginFrame(), "frame begins");
        const auto f = xr.pendingFrame();
        require(f.shouldRender, "frame renders");
        xr.markEyeRendered(0, f.frame);
        xr.markEyeRendered(1, f.frame);
        xr.finishFrame();
        for (int eye = 0; eye < 2; ++eye)
        {
            if(!turned)require(std::memcmp(&submitted[eye].pose,&located[eye].pose,sizeof(XrPosef))==0,"submitted eye pose and IPD remain exactly the runtime pose");
            else{   // turned toward the middle of the runtime fov (cantedEnvelope): same position, the turn added
                const auto want=normalized(multiply(located[eye].pose.orientation,cantedEnvelope(located[eye].fov).turn));const auto& q=submitted[eye].pose.orientation;
                require(std::memcmp(&submitted[eye].pose.position,&located[eye].pose.position,sizeof(XrVector3f))==0&&
                        std::abs(q.x-want.x)<1e-6f&&std::abs(q.y-want.y)<1e-6f&&std::abs(q.z-want.z)<1e-6f&&std::abs(q.w-want.w)<1e-6f,"turned eye keeps its position (IPD) and adds the turn");}
            require(std::memcmp(&submitted[eye].fov, &f.views[eye].fov, sizeof(XrFovf)) == 0, "submitted fov is the fov the eye was rendered with");
            require(submitted[eye].subImage.imageRect.extent.width == int32_t(xr.targets[eye].width) &&
                        submitted[eye].subImage.imageRect.extent.height == int32_t(xr.targets[eye].height) &&
                        submitted[eye].subImage.imageRect.offset.x == 0 && submitted[eye].subImage.imageRect.offset.y == 0,
                    "submitted image rect is the eye extent at the image origin");
            result.width[eye] = xr.targets[eye].width; result.height[eye] = xr.targets[eye].height; result.fov[eye] = f.views[eye].fov;
        }
    }
    return result;
}
} // namespace
int main(int argc,char** argv)
{
    try
    {
        recordedFov=argc==2 && std::strcmp(argv[1],"recorded")==0;
        std::filesystem::create_directories("fov-crop-validation");
        // Mask at 85 % of the tangent extents: crop applies.
        const auto cropped = run("cropped.jsonl");
        for (int eye = 0; eye < 2; ++eye)
        {
            const auto full = mockFov(eye), f = cropped.fov[eye];
            require(f.angleLeft > full.angleLeft && f.angleRight < full.angleRight && f.angleUp < full.angleUp && f.angleDown > full.angleDown,
                    "cropped fov lies inside the runtime fov");
            const float expectedRight = std::atan(std::tan(full.angleRight) * .85f) + 3.14159265f / 180;
            require(std::abs(f.angleRight - expectedRight) < 1e-4f, "crop is the mask box plus a one degree margin");
            require(cropped.width[eye] < cropped.fullWidth[eye] && cropped.height[eye] < cropped.fullHeight[eye], "eye extent shrinks");
            const double ratio = (std::tan(f.angleRight) - std::tan(f.angleLeft)) / (std::tan(full.angleRight) - std::tan(full.angleLeft));
            require(std::abs(double(cropped.width[eye]) / cropped.fullWidth[eye] - ratio) < 2.0 / cropped.fullWidth[eye], "pixels per tangent are kept");
        }
        const double saving = 1.0 - double(cropped.width[0]) * cropped.height[0] / (double(cropped.fullWidth[0]) * cropped.fullHeight[0]);
        std::cout << "pixel saving per eye " << saving << '\n';
        require(saving > .15 && saving < .30, "85 % mask saves 15..30 % of the eye pixels");
        // Opt-out: the extension is not enabled and nothing changes.
        _putenv_s("EVR_FOV_CROP", "0");
        const auto off = run("disabled.jsonl");
        for (int eye = 0; eye < 2; ++eye)
        {
            const auto runtimeFov = mockFov(eye);
            require(off.width[eye] == off.fullWidth[eye] && std::memcmp(&off.fov[eye], &runtimeFov, sizeof(XrFovf)) == 0, "EVR_FOV_CROP=0 keeps the runtime fov and extent");
        }
        _putenv_s("EVR_FOV_CROP", "");
        // A mask that already fills the fov: under 2 % saving, not applied.
        _putenv_s("EVR_MOCK_VISIBILITY_MASK", "1");
        const auto full = run("full-mask.jsonl");
        for (int eye = 0; eye < 2; ++eye)
        {
            const auto runtimeFov = mockFov(eye);
            require(full.width[eye] == full.fullWidth[eye] && std::memcmp(&full.fov[eye], &runtimeFov, sizeof(XrFovf)) == 0, "full mask leaves the runtime fov");
        }
        _putenv_s("EVR_SYMMETRIC_FOV","1");_putenv_s("EVR_CANTED_FOV","0");
        const auto symmetric=run("symmetric.jsonl");
        _putenv_s("EVR_SYMMETRIC_FOV","");_putenv_s("EVR_CANTED_FOV","");
        for(unsigned eye=0;eye<2;++eye){
            const auto expected=symmetricEnvelope(mockFov(eye));
            require(std::memcmp(&symmetric.fov[eye],&expected,sizeof(XrFovf))==0,"engine and submitted layer receive the symmetric runtime envelope");
            require(symmetric.width[eye]==symmetric.fullWidth[eye]&&symmetric.height[eye]==symmetric.fullHeight[eye],"symmetric compatibility retains allocated eye extent");
        }
        // Turned toward the middle (2026-10-19, the default with EVR_SYMMETRIC_FOV): with a lens mask touching the fov's
        // sides (an ellipse at 1.0) and without one (the runtime rectangle's corners).
        for(const char* mask:{"1",""}){
            _putenv_s("EVR_SYMMETRIC_FOV","1");_putenv_s("EVR_MOCK_VISIBILITY_MASK",*mask?mask:"0");
            const auto turned=run(*mask?"canted-mask.jsonl":"canted-corners.jsonl",true);
            _putenv_s("EVR_SYMMETRIC_FOV","");
            for(unsigned eye=0;eye<2;++eye){
                const auto& f=turned.fov[eye];
                require(f.angleLeft==-f.angleRight&&f.angleDown==-f.angleUp,"turned render frustum is centred");
                require(turned.width[eye]<=turned.fullWidth[eye]&&turned.height[eye]<=turned.fullHeight[eye],"turned extent never grows");
            }
            require(turned.width[0]==turned.width[1]&&turned.height[0]==turned.height[1],"both eyes keep one extent");
            const double saving=1.0-double(turned.width[0])*turned.height[0]/(double(turned.fullWidth[0])*turned.fullHeight[0]);
            std::cout<<"turned pixel saving "<<(*mask?"(mask) ":"(corners) ")<<saving<<'\n';
            if(recordedFov)require(*mask?saving>.2:saving>.1,*mask?"the user's headset with a lens mask saves over 20 %":"the user's headset without a mask saves over 10 %");
        }
        // The visible area kept for the next start (fovCache): a start with the mask writes it, a start without uses it.
        {
            std::filesystem::remove("fov-crop-validation/fov-cache.txt");
            _putenv_s("EVR_FOV_CACHE_FILE","fov-crop-validation/fov-cache.txt");_putenv_s("EVR_SYMMETRIC_FOV","1");
            _putenv_s("EVR_MOCK_VISIBILITY_MASK","1");const auto measured=run("canted-cache-write.jsonl",true);
            require(std::filesystem::exists("fov-crop-validation/fov-cache.txt"),"a start with the lens mask keeps it for the next start");
            _putenv_s("EVR_MOCK_VISIBILITY_MASK","0");const auto reused=run("canted-cache-read.jsonl",true);
            require(reused.width[0]==measured.width[0]&&reused.height[0]==measured.height[0],"a start without the mask takes the kept one: the same extent");
            _putenv_s("EVR_FOV_CACHE_FILE","");_putenv_s("EVR_SYMMETRIC_FOV","");
        }
        _putenv_s("EVR_MOCK_VISIBILITY_MASK","0.85");
        std::cout << checks << " checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL " << e.what() << '\n';
        return 1;
    }
}
