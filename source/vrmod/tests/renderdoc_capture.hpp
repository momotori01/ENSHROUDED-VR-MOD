#pragma once
#include <windows.h>
#include <renderdoc_app.h>
#include <cstdlib>
#include <stdexcept>

// Test-only explicit capture around a complete stereo pair; no DLL injection or registration.
class FixtureRenderDoc {
    RENDERDOC_API_1_6_0* api=nullptr;
    void* device=nullptr;
    bool capturing=false;
public:
    template<class Instance> explicit FixtureRenderDoc(Instance instance) {
        const char* output=std::getenv("EVR_RENDERDOC_CAPTURE");
        if(!output||!*output)return;
        HMODULE mod=GetModuleHandleW(L"renderdoc.dll");
        auto get=mod?reinterpret_cast<pRENDERDOC_GetAPI>(GetProcAddress(mod,"RENDERDOC_GetAPI")):nullptr;
        if(!get||!get(eRENDERDOC_API_Version_1_6_0,reinterpret_cast<void**>(&api)))throw std::runtime_error("RenderDoc explicitly requested but API unavailable");
        device=RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance);
        api->SetCaptureFilePathTemplate(output);
        api->MaskOverlayBits(0,0);
        api->SetCaptureKeys(nullptr,0);
        api->SetFocusToggleKeys(nullptr,0);
    }
    void begin(int frame) {if(api&&frame==4){api->StartFrameCapture(device,nullptr);capturing=true;}}
    void end() {if(capturing){capturing=false;if(!api->EndFrameCapture(device,nullptr))throw std::runtime_error("RenderDoc EndFrameCapture failed");}}
    ~FixtureRenderDoc(){if(capturing)api->DiscardFrameCapture(device,nullptr);}
};
