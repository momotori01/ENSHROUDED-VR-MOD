#pragma once
// Diagnostic only (2026-10-23, the right A as the game's own A with the ray on the plate - STATUS 10-23 21:05 (a)): copies of
// the game's memory around its UI, taken on the game's thread inside the frontend's update (input_ui_observer.hpp gate), for
// finding offline where the UI keeps the thing under the mouse (the hover) and the thing the pad has chosen (the focus).
// What is copied: the exe's .data, and 32 MB around each
// of the frontend, its state, the UI controller, the UI object and the two device objects. Nothing is written to the game.
// EVR_UI_SCAN_FRAMES=<XR frame>:<label>,... and EVR_UI_SCAN_DIR=<folder>; without both nothing happens.
// File: "EVRS", u32 region count, u64 exe base, then per region u64 base, u64 size, u32 name length, name, the bytes (pages
// that cannot be read are zeros).
#include <filesystem>
#include <fstream>
namespace uiScan {
struct Step {uint64_t frame=0;std::string label;};
inline std::vector<Step> steps;inline size_t next=0;inline std::filesystem::path folder;inline bool armed=false;
inline void install(){
    char frames[2048]{},dir[1024]{};
    const auto n=GetEnvironmentVariableA("EVR_UI_SCAN_FRAMES",frames,sizeof frames),m=GetEnvironmentVariableA("EVR_UI_SCAN_DIR",dir,sizeof dir);
    if(!n||n>=sizeof frames||!m||m>=sizeof dir)return;
    std::string text(frames);size_t at=0;
    while(at<text.size()){
        auto comma=text.find(',',at);if(comma==std::string::npos)comma=text.size();
        const auto item=text.substr(at,comma-at);const auto colon=item.find(':');
        if(colon!=std::string::npos)steps.push_back({std::strtoull(item.substr(0,colon).c_str(),nullptr,10),item.substr(colon+1)});
        at=comma+1;
    }
    folder=std::filesystem::path(dir);std::error_code ec;std::filesystem::create_directories(folder,ec);
    armed=!steps.empty();
    logFile->event("ui_scan_armed","\"steps\":"+std::to_string(steps.size()));
}
struct Region {uintptr_t base=0;size_t size=0;std::string name;};
// 8 MB before to 24 MB after the object, inside its allocation (the game's allocator reserves arenas of many GB: the
// first copies took the arena's first 128 MB, far from the objects). Pages that are not committed are zeros.
inline void addAllocation(std::vector<Region>& out,uintptr_t inside,const char* name){
    if(!inside)return;
    MEMORY_BASIC_INFORMATION mbi{};if(!VirtualQuery(reinterpret_cast<void*>(inside),&mbi,sizeof mbi)||!mbi.AllocationBase)return;
    const auto allocation=reinterpret_cast<uintptr_t>(mbi.AllocationBase);
    uintptr_t base=std::max<uintptr_t>(allocation,(inside-(8ull<<20))&~uintptr_t(4095));const uintptr_t end=base+(32ull<<20);
    for(auto& r:out)if(base<r.base+r.size&&r.base<end){   // overlapping: one region from the lower start to the higher end
        const auto lo=std::min(base,r.base),hi=std::max<uintptr_t>(end,r.base+r.size);r.base=lo;r.size=size_t(hi-lo);r.name+="+"+std::string(name);return;}
    out.push_back({base,size_t(end-base),name});
}
inline bool readable(const MEMORY_BASIC_INFORMATION& q){
    if(q.State!=MEM_COMMIT||(q.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    return (q.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))!=0;
}
// Called from the gate with the objects it has just read.
inline void maybe(uintptr_t owner,uintptr_t state,uintptr_t controller,uintptr_t ui,uintptr_t device,uintptr_t localDevice){
    if(!armed||next>=steps.size()||liveFrame.frame<steps[next].frame)return;
    const auto step=steps[next++];
    try{
        std::vector<Region> regions;
        {const auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(baseAddress);const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(baseAddress+dos->e_lfanew);
         auto sec=IMAGE_FIRST_SECTION(nt);
         for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i,++sec)
            if((sec->Characteristics&IMAGE_SCN_MEM_WRITE)&&sec->Misc.VirtualSize)regions.push_back({baseAddress+sec->VirtualAddress,sec->Misc.VirtualSize,"exe_data"});}
        addAllocation(regions,owner,"frontend");addAllocation(regions,state,"state");addAllocation(regions,controller,"controller");
        addAllocation(regions,ui,"ui");addAllocation(regions,device,"device");addAllocation(regions,localDevice,"local_device");
        const auto path=folder/(std::to_string(step.frame)+"-"+step.label+".evrs");
        std::ofstream o(path,std::ios::binary|std::ios::trunc);
        o.write("EVRS",4);const uint32_t count=uint32_t(regions.size());o.write(reinterpret_cast<const char*>(&count),4);
        const uint64_t exe=baseAddress;o.write(reinterpret_cast<const char*>(&exe),8);
        std::vector<char> page(4096);uint64_t total=0;
        for(const auto& r:regions){
            const uint64_t b=r.base,s=r.size;const uint32_t nl=uint32_t(r.name.size());
            o.write(reinterpret_cast<const char*>(&b),8);o.write(reinterpret_cast<const char*>(&s),8);o.write(reinterpret_cast<const char*>(&nl),4);o.write(r.name.data(),nl);
            uintptr_t at=r.base;const uintptr_t end=r.base+r.size;
            while(at<end){
                MEMORY_BASIC_INFORMATION q{};if(!VirtualQuery(reinterpret_cast<void*>(at),&q,sizeof q))break;
                const uintptr_t stop=std::min<uintptr_t>(end,reinterpret_cast<uintptr_t>(q.BaseAddress)+q.RegionSize);
                if(readable(q)){
                    for(uintptr_t p=at;p<stop;p+=4096){const size_t len=size_t(std::min<uintptr_t>(4096,stop-p));SIZE_T got=0;
                        if(!ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(p),page.data(),len,&got)||got!=len)std::fill(page.begin(),page.begin()+len,0);
                        o.write(page.data(),len);}
                }else{std::fill(page.begin(),page.end(),0);for(uintptr_t p=at;p<stop;p+=4096)o.write(page.data(),size_t(std::min<uintptr_t>(4096,stop-p)));}
                at=stop;
            }
            total+=s;
        }
        std::ostringstream names;for(const auto& r:regions)names<<(names.tellp()>0?",":"")<<"[\""<<r.name<<"\","<<hex(r.base)<<","<<r.size<<"]";
        logFile->event("ui_scan_taken","\"frame\":"+std::to_string(liveFrame.frame)+",\"label\":"+jsonString(step.label)+",\"bytes\":"+std::to_string(total)+
            ",\"objects\":{\"frontend\":"+hex(owner)+",\"state\":"+hex(state)+",\"controller\":"+hex(controller)+",\"ui\":"+hex(ui)+",\"device\":"+hex(device)+",\"local_device\":"+hex(localDevice)+"},\"regions\":["+names.str()+"]");
    }catch(const std::exception& e){logFile->event("ui_scan_failure","\"reason\":"+jsonString(e.what()));}
}
}
