#pragma once
// Diagnostic (2026-11-11, the stick-turn motion vector bursts: in ~40-frame stretches the game draws an eye's motion as if
// its camera had not turned, while a thing moving with the head gets the turn - motion_blur_20261111.md). Where does the
// game keep the camera an eye's motion starts from? Not in the View's 0x1140 bytes nor in the camera arguments.
// EVR_SCAN_PREV_VIEW=<frame>[/<frame>...]: after eye 0's update at those XR frames, every committed readable page of the
// process (not write-combined / uncached) is searched for eye 0's last-frame view matrix (View+0x1000 of the frame before):
// its first row (m0 m1 m2) and its first column (m0 m4 m8), each within 2e-5. Hits are logged (prev_view_scan_hit); the first
// 16 are then read every frame for 700 frames (prev_view_watch) next to this frame's and last frame's rows, to see what
// they hold in a stretch. Nothing is written to the game.
#include <cmath>
namespace prevViewScan {
inline std::vector<uint64_t> frames;inline size_t next=0;inline bool armed=false;
inline std::array<float,16> last{};inline std::array<uint8_t,40> lastCamera{};inline bool have=false;
struct Hit {uintptr_t at=0;int kind=0;};
inline std::vector<Hit> watch;inline uint64_t watchUntil=0;
inline void install(){
    char v[256]{};const auto n=GetEnvironmentVariableA("EVR_SCAN_PREV_VIEW",v,sizeof v);if(!n||n>=sizeof v)return;
    std::string text(v);size_t at=0;
    while(at<text.size()){auto e=text.find_first_of(",/",at);if(e==std::string::npos)e=text.size();
        const auto f=std::strtoull(text.substr(at,e-at).c_str(),nullptr,10);if(f)frames.push_back(f);at=e+1;}
    std::sort(frames.begin(),frames.end());armed=!frames.empty();
}
// Plain types only (SEH): hits of the three floats key[0..2] at stride `stride` bytes in [p, p+n).
static size_t scanRange(const uint8_t* p,size_t n,const float* key,size_t stride,uintptr_t* out,size_t room){
    size_t found=0;
    __try{
        const float tol=2e-5f;
        for(size_t o=0;o+2*stride+4<=n;o+=4){
            const float a=*reinterpret_cast<const float*>(p+o);
            if(!(std::fabs(a-key[0])<=tol))continue;   // NaN fails too
            if(!(std::fabs(*reinterpret_cast<const float*>(p+o+stride)-key[1])<=tol))continue;
            if(!(std::fabs(*reinterpret_cast<const float*>(p+o+2*stride)-key[2])<=tol))continue;
            if(found<room)out[found]=reinterpret_cast<uintptr_t>(p+o);
            ++found;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
    return found;
}
// Plain types only (SEH): exact copies of `bytes` bytes of key at `step` alignment in [p, p+n).
static size_t scanExact(const uint8_t* p,size_t n,const uint8_t* key,size_t bytes,size_t step,uintptr_t* out,size_t room){
    size_t found=0;
    __try{
        uint32_t first;std::memcpy(&first,key,4);
        for(size_t o=0;o+bytes<=n;o+=step){
            if(*reinterpret_cast<const uint32_t*>(p+o)!=first)continue;
            if(std::memcmp(p+o,key,bytes))continue;
            if(found<room)out[found]=reinterpret_cast<uintptr_t>(p+o);
            ++found;
        }
    }__except(EXCEPTION_EXECUTE_HANDLER){}
    return found;
}
static bool readFloats(uintptr_t at,float* out,size_t count){
    __try{for(size_t i=0;i<count;i++)out[i]=reinterpret_cast<const float*>(at)[i];return true;}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
}
// Called after eye 0's View update each frame.
inline void after(uintptr_t view,const uint8_t* args,uint64_t frame){
    if(!armed)return;
    const auto now=read<std::array<float,16>>(view+0x1000);
    std::array<uint8_t,40> camera{};std::memcpy(camera.data(),args+0x38,40);   // Q32 position (24 bytes) + quaternion (16 bytes)
    if(next<frames.size()&&frame>=frames[next]&&have){
        ++next;
        const float row[3]{last[0],last[1],last[2]},col[3]{last[0],last[4],last[8]};
        std::vector<Hit> hits;size_t total=0,regions=0;uint64_t bytes=0;
        const auto start=std::chrono::steady_clock::now();
        uintptr_t a=0x10000;MEMORY_BASIC_INFORMATION q{};
        std::vector<uintptr_t> found(64);
        // this module's own copies (last, lastCamera) are not the game's
        uintptr_t self=0,selfSize=0;{HMODULE m=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,reinterpret_cast<LPCWSTR>(&last),&m);
            if(m){const auto b=reinterpret_cast<uintptr_t>(m);const auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(b+reinterpret_cast<const IMAGE_DOS_HEADER*>(b)->e_lfanew);
                self=b;selfSize=nt->OptionalHeader.SizeOfImage;}}
        // and this thread's stack (the keys, last frame's leftovers)
        ULONG_PTR stackLo=0,stackHi=0;GetCurrentThreadStackLimits(&stackLo,&stackHi);
        while(VirtualQuery(reinterpret_cast<void*>(a),&q,sizeof q)==sizeof q){
            const auto base=reinterpret_cast<uintptr_t>(q.BaseAddress);const auto size=size_t(q.RegionSize);
            if(q.State==MEM_COMMIT&&!(q.Protect&(PAGE_GUARD|PAGE_NOACCESS|PAGE_WRITECOMBINE|PAGE_NOCACHE))&&
               (q.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))){
                ++regions;bytes+=size;
                for(int kind=0;kind<4;kind++){
                    const auto c=kind<2?scanRange(reinterpret_cast<const uint8_t*>(base),size,kind?col:row,kind?16:4,found.data(),found.size()):
                        kind==2?scanExact(reinterpret_cast<const uint8_t*>(base),size,lastCamera.data()+24,16,4,found.data(),found.size()):   // last frame's quaternion
                                scanExact(reinterpret_cast<const uint8_t*>(base),size,lastCamera.data(),24,8,found.data(),found.size());      // last frame's position
                    total+=c;for(size_t i=0;i<std::min(c,found.size());i++)if(hits.size()<64&&!(found[i]>=self&&found[i]<self+selfSize)&&!(found[i]>=stackLo&&found[i]<stackHi))hits.push_back({found[i],kind});
                }
            }
            const auto nextAddress=base+size;if(nextAddress<=a)break;a=nextAddress;if(a>=0x7ffffffeffffull)break;
        }
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();
        logFile->event("prev_view_scan","\"frame\":"+std::to_string(frame)+",\"hits\":"+std::to_string(total)+",\"regions\":"+std::to_string(regions)+
            ",\"mb\":"+std::to_string(bytes>>20)+",\"ms\":"+std::to_string(ms)+",\"view\":"+hex(view));
        for(const auto& h:hits){
            MEMORY_BASIC_INFORMATION r{};VirtualQuery(reinterpret_cast<void*>(h.at),&r,sizeof r);
            float ctx[24]{};readFloats(h.at-32,ctx,24);
            std::ostringstream o;o<<std::setprecision(6)<<"\"frame\":"<<frame<<",\"at\":\""<<std::hex<<h.at<<"\",\"allocation\":\""<<reinterpret_cast<uintptr_t>(r.AllocationBase)
                <<"\",\"offset\":\""<<(h.at-reinterpret_cast<uintptr_t>(r.AllocationBase))<<"\",\"from_view\":\""<<(h.at>=view?h.at-view:0)<<std::dec<<"\",\"kind\":\""<<(h.kind==0?"row":h.kind==1?"column":h.kind==2?"quaternion":"position")
                <<"\",\"type\":"<<r.Type<<",\"protect\":"<<r.Protect<<",\"context\":[";
            for(int i=0;i<24;i++)o<<(i?",":"")<<ctx[i];o<<']';
            logFile->event("prev_view_scan_hit",o.str());
        }
        if(watch.empty()){for(const auto& h:hits)if(watch.size()<16)watch.push_back(h);watchUntil=frame+700;}
    }
    if(!watch.empty()&&frame<=watchUntil){
        std::ostringstream o;o<<std::setprecision(7)<<"\"frame\":"<<frame<<",\"now\":["<<now[0]<<','<<now[1]<<','<<now[2]<<"],\"last\":["<<last[0]<<','<<last[1]<<','<<last[2]<<"],\"places\":[";
        for(size_t i=0;i<watch.size();i++){float f[9]{};const bool ok=readFloats(watch[i].at,f,9);
            if(watch[i].kind==3){int64_t q[3]{};readFloats(watch[i].at,reinterpret_cast<float*>(q),6);o<<(i?",":"")<<'['<<q[0]<<','<<q[1]<<','<<q[2]<<']';continue;}
            if(watch[i].kind==2){o<<(i?",":"")<<'['<<f[0]<<','<<f[1]<<','<<f[2]<<','<<f[3]<<']';continue;}
            const size_t s=watch[i].kind?4:1;   // the column: every 4th float
            o<<(i?",":"")<<'['<<(ok?f[0]:NAN)<<','<<(ok?f[s]:NAN)<<','<<(ok?f[2*s]:NAN)<<']';}
        o<<"],\"quaternion\":[";float qn[4];std::memcpy(qn,camera.data()+24,16);o<<qn[0]<<','<<qn[1]<<','<<qn[2]<<','<<qn[3]<<']';
        o<<",\"last_quaternion\":[";float ql[4];std::memcpy(ql,lastCamera.data()+24,16);o<<ql[0]<<','<<ql[1]<<','<<ql[2]<<','<<ql[3]<<']';
        logFile->event("prev_view_watch",o.str());
    }
    last=now;lastCamera=camera;have=true;
}
} // namespace prevViewScan
