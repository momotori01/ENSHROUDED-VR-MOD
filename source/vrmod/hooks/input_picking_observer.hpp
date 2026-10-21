// Read-only observation of the existing asynchronous PickingTarget ray request.
// This does not fire weapons, inject controls, or override target selection.
namespace inputPicking {
using Submit=void*(__fastcall*)(void*,void*,uint32_t,const void*,const void*,const void*,float,const void*,bool);
inline Submit original=nullptr;
inline bool enabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},calls{0},samples{0},failures{0};
constexpr uintptr_t rva=0x955010,callerRva=0x477a1d;
constexpr const char* pattern="48 89 5C 24 08 48 89 7C 24 10 55 48 8D 6C 24 B0 48 81 EC 50 01 00 00 8B 85 80 00 00 00 0F 57 C0 0F 11 44 24 70 48 8B";
struct CallbackScope {CallbackScope(){callbacks.fetch_add(1);}~CallbackScope(){callbacks.fetch_sub(1);}};
__declspec(noinline) void* __fastcall submit(void* context,void* output,uint32_t entity,const void* transform,
    const void* filter,const void* direction,float range,const void* flags,bool option){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    const auto sp=reinterpret_cast<uintptr_t>(_AddressOfReturnAddress())+8;
    const CallbackScope callback;
    std::string pending;std::array<uint8_t,64> cameraBefore{};uintptr_t input=0;
    {const ObserverLastError preserve;
     if(accepting.load()&&caller==baseAddress+callerRva&&calls.fetch_add(1)%8==0&&samples.load()<256){
        try{
          [&]{
            rootCamera::Sample local;{std::lock_guard<std::mutex> lock(rootCamera::mutex);local=rootCamera::latest;}
            if(!local.number||GetTickCount64()-local.milliseconds>1000||entity!=local.entity)return;
            if(!context||reinterpret_cast<uintptr_t>(output)!=sp+0xd8||reinterpret_cast<uintptr_t>(transform)!=sp+0x150||
               reinterpret_cast<uintptr_t>(filter)!=sp+0x2e0||reinterpret_cast<uintptr_t>(direction)!=sp+0x90||
               reinterpret_cast<uintptr_t>(flags)!=sp+0x2d8||entity!=read<uint32_t>(sp+0x2e8))
                throw std::runtime_error("picking request caller ABI changed");
            input=read<uintptr_t>(sp+0x58);if(!input)throw std::runtime_error("picking PlayerInput absent");
            cameraBefore=read<std::array<uint8_t,64>>(input+0x28);
            const auto p=read<WorldPositionQ32>(input+0x28),origin=read<WorldPositionQ32>(reinterpret_cast<uintptr_t>(transform));
            const auto q=read<Quat>(input+0x40),tq=read<Quat>(reinterpret_cast<uintptr_t>(transform)+0x18);normalized(q);normalized(tq);
            const auto d=read<Vec3>(reinterpret_cast<uintptr_t>(direction));
            const auto scale=read<Vec3>(reinterpret_cast<uintptr_t>(transform)+0x28);
            const auto endpoints=read<std::array<float,8>>(sp+0x100);
            const auto distance=read<float>(input+0x50);
            if(!finite(d)||!finite(scale)||!std::isfinite(distance)||!std::isfinite(range)||range<=0||
               !std::isfinite(endpoints[0])||!std::isfinite(endpoints[1])||!std::isfinite(endpoints[2])||
               !std::isfinite(endpoints[4])||!std::isfinite(endpoints[5])||!std::isfinite(endpoints[6]))
                throw std::runtime_error("nonfinite picking ray");
            std::ostringstream out;out<<std::setprecision(10)<<"\"sample\":"<<samples.fetch_add(1)+1
                <<",\"entity\":"<<entity<<",\"local_entity\":"<<local.entity<<",\"caller\":"<<hex(callerRva)
                <<",\"thread\":"<<GetCurrentThreadId()<<",\"input_mode\":"<<unsigned(read<uint8_t>(input+game::playerInputMode))
                <<",\"camera_q32\":["<<p.x<<','<<p.y<<','<<p.z<<"],\"camera_orientation\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<']'
                <<",\"camera_distance\":"<<distance<<",\"ray_origin_q32\":["<<origin.x<<','<<origin.y<<','<<origin.z<<']'
                <<",\"ray_transform_q\":["<<tq.x<<','<<tq.y<<','<<tq.z<<','<<tq.w<<']'
                <<",\"range\":"<<range<<",\"filter\":"<<read<uint32_t>(reinterpret_cast<uintptr_t>(filter))
                <<",\"flags\":"<<read<uint16_t>(reinterpret_cast<uintptr_t>(flags))<<",\"option\":"<<(option?"true":"false");
            auto vector=[&](const char* name,Vec3 v){out<<",\""<<name<<"\":["<<v.x<<','<<v.y<<','<<v.z<<']';};
            vector("ray_direction",d);vector("ray_scale",scale);
            vector("float_origin",{endpoints[0],endpoints[1],endpoints[2]});
            vector("float_endpoint",{endpoints[4],endpoints[5],endpoints[6]});
            pending=out.str();
          }();
        }catch(const std::exception& e){if(failures.fetch_add(1)==0)logFile->event("input_picking_failure","\"reason\":"+jsonString(e.what()));}
     }}
    const auto result=original(context,output,entity,transform,filter,direction,range,flags,option);
    {const ObserverLastError preserve;
     if(!pending.empty()){
        try{
            const bool unchanged=cameraBefore==read<std::array<uint8_t,64>>(input+0x28);
            logFile->event("input_picking_sample",pending+",\"request_id\":"+std::to_string(read<uint32_t>(reinterpret_cast<uintptr_t>(output)))+
                ",\"return_is_output\":"+(result==output?"true":"false")+",\"source_camera_unchanged\":"+(unchanged?"true":"false")+",\"read_only\":true");
        }catch(const std::exception& e){if(failures.fetch_add(1)==0)logFile->event("input_picking_failure","\"reason\":"+jsonString(e.what()));}
     }}
    return result;
}
inline void install(){
    inputPipeline::verify(rva,pattern,callerRva);
    // 1.0 (2026-10-17): the caller passes the filter (0x208) and the flags (1) by value, not as pointers into its frame
    // (0x477689: mov dword ptr [rsp+0x20],0x208), so the June frame offsets above never match - every sample was a
    // "picking request caller ABI changed" failure. The observer only records; it is not installed on that layout.
    {const std::array<uint8_t,8> byValue{0xc7,0x44,0x24,0x20,0x08,0x02,0x00,0x00};
     if(read<std::array<uint8_t,8>>(baseAddress+callerRva-0x14)==byValue)
        throw std::runtime_error("1.0 caller passes the picking filter and flags by value; the read-only sample is not adapted");}
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&submit),reinterpret_cast<void**>(&original))!=MH_OK)
        throw std::runtime_error("picking observer creation failed");
    accepting=true;
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK){accepting=false;throw std::runtime_error("picking observer enable failed");}
    enabled=true;logFile->event("input_picking_enabled","\"rva\":"+hex(rva)+",\"caller\":"+hex(callerRva)+",\"pattern\":"+jsonString(pattern));
}
inline void disable(){
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("picking observer disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const auto bytes=parsePattern(pattern);const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("input_picking_removed","\"restored\":"+std::string(restored?"true":"false")+",\"samples\":"+std::to_string(samples.load())+
        ",\"calls\":"+std::to_string(calls.load())+",\"failures\":"+std::to_string(failures.load())+",\"callbacks\":"+std::to_string(callbacks.load()));
    if(!restored||callbacks.load())throw std::runtime_error("picking observer did not quiesce/restore");
}
}
