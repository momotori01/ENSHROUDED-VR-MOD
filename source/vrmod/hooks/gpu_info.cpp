// The graphics card for the recommended graphics (include/evr/gpu_info.hpp). Alone in this translation unit (dxgi.h).
#include <evr/gpu_info.hpp>
#include <windows.h>
#include <dxgi.h>
#pragma comment(lib,"dxgi.lib")
namespace evr::recommend {
Machine detectMachine(){
    Machine best;IDXGIFactory1* factory=nullptr;
    if(FAILED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory)))||!factory)return best;
    IDXGIAdapter1* adapter=nullptr;
    for(UINT i=0;factory->EnumAdapters1(i,&adapter)!=DXGI_ERROR_NOT_FOUND;++i){
        DXGI_ADAPTER_DESC1 d{};
        if(adapter&&SUCCEEDED(adapter->GetDesc1(&d))&&!(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)&&d.VendorId!=0x1414&&uint64_t(d.DedicatedVideoMemory)>best.videoMemory){
            char name[256]{};WideCharToMultiByte(CP_UTF8,0,d.Description,-1,name,sizeof name-1,nullptr,nullptr);
            best.gpu=name;best.vendor=d.VendorId;best.videoMemory=uint64_t(d.DedicatedVideoMemory);best.known=true;}
        if(adapter){adapter->Release();adapter=nullptr;}
    }
    factory->Release();return best;
}
}
