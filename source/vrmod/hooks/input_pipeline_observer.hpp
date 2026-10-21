// Read-only, sampled correlation across a single native input builder call.
namespace inputPipeline {
struct Fields {
    std::array<uint8_t,64> camera{};
    std::array<uint8_t,8> stick{};
    uint64_t digital=0;uint8_t mask=0;
};
inline Fields fields(uintptr_t data){
    return {read<std::array<uint8_t,64>>(data+0x20),read<std::array<uint8_t,8>>(data+game::clientStick),
            read<uint64_t>(data+game::clientDigital),read<uint8_t>(data+game::clientMask)};
}
inline bool same(const Fields& a,const Fields& b){return a.camera==b.camera&&a.stick==b.stick&&a.digital==b.digital&&a.mask==b.mask;}
struct Pending {bool active=false,predicted=false,predictionMatches=false;unsigned sample=0;uint32_t entity=0,tick=0;Fields expected;};
inline thread_local Pending pending;
using Predict=uintptr_t(__fastcall*)(void*,uint32_t,uint32_t,uint32_t,const void*);
using Encode=uintptr_t(__fastcall*)(void*,const void*,const void*,uint32_t);
inline Predict originalPredict=nullptr;inline Encode originalEncode=nullptr;
inline bool predictEnabled=false,encodeEnabled=false;
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},samples{0},failures{0};
struct CallbackScope{CallbackScope(){callbacks.fetch_add(1);}~CallbackScope(){callbacks.fetch_sub(1);}};
constexpr uintptr_t predictRva=0x9560b0,encodeRva=0x3a5640;
constexpr const char* predictPattern="89 54 24 10 53 56 57 41 56 41 57 48 81 EC 80 00 00 00";
constexpr const char* encodePattern="48 89 5C 24 08 48 89 74 24 10 48 89 7C 24 18 41 56 48 83 EC 20 0F 10 42";
inline void failure(const char* reason){if(failures.fetch_add(1)==0)logFile->event("input_pipeline_failure","\"reason\":"+jsonString(reason));}
inline void start(uintptr_t input,uintptr_t callerRsp,unsigned sample,uint32_t entity,uint32_t tick){
    pending={};pending.sample=sample;pending.entity=entity;pending.tick=tick;pending.expected=fields(input);
    // The builder applies this same freecam bit to prediction and history.
    // 1.0: the freecam byte is the builder's sixth argument, at the copy call's rsp +0xf98 (June the seventh, +0xe50).
    pending.expected.mask=uint8_t((pending.expected.mask&~8u)|(read<uint8_t>(callerRsp+0xf98)?8u:0));
    pending.active=true;
}
__declspec(noinline) uintptr_t __fastcall predict(void* context,uint32_t entity,uint32_t tick,uint32_t component,const void* span){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const CallbackScope callback;
    {const ObserverLastError preserve;
     if(accepting.load()&&pending.active&&caller==baseAddress+0x3a2c78){
        try{
            const auto descriptor=read<std::array<uintptr_t,2>>(reinterpret_cast<uintptr_t>(span));
            if(!descriptor[0]||descriptor[1]!=0x528||entity!=pending.entity||tick!=pending.tick)
                throw std::runtime_error("prediction span/entity/tick mismatch");
            pending.predicted=true;pending.predictionMatches=same(pending.expected,fields(descriptor[0]+8));
            if(!pending.predictionMatches)failure("prediction differs from sampled source");
        }catch(const std::exception& e){failure(e.what());}
     }}
    return originalPredict(context,entity,tick,component,span);
}
__declspec(noinline) uintptr_t __fastcall encode(void* output,const void* input,const void* auxiliary,uint32_t flags){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());const CallbackScope callback;
    Pending captured;bool match=false;uint32_t historyTick=0;
    {const ObserverLastError preserve;
     if(accepting.load()&&pending.active&&caller==baseAddress+0x3a2e2c){
        captured=pending;pending={};
        try{
            const auto data=reinterpret_cast<uintptr_t>(input);
            historyTick=read<uint32_t>(data-8);match=same(captured.expected,fields(data));
        }catch(const std::exception& e){failure(e.what());}
     }}
    const auto result=originalEncode(output,input,auxiliary,flags);
    {const ObserverLastError preserve;
     if(captured.active){
        try{
            const auto out=reinterpret_cast<uintptr_t>(output);
            Fields packet{read<std::array<uint8_t,64>>(out),read<std::array<uint8_t,8>>(out+0x40),
                          read<uint64_t>(out+0xe0),read<uint8_t>(out+0xd8)};
            const bool serialized=same(captured.expected,packet);
            std::ostringstream f;f<<"\"sample\":"<<captured.sample<<",\"entity\":"<<captured.entity
                <<",\"native_digital\":"<<captured.expected.digital<<",\"serialized_digital\":"<<packet.digital
                <<",\"tick\":"<<captured.tick<<",\"history_tick\":"<<historyTick
                <<",\"prediction_seen\":"<<(captured.predicted?"true":"false")
                <<",\"prediction_matches\":"<<(captured.predictionMatches?"true":"false")
                <<",\"history_matches\":"<<(match?"true":"false")<<",\"serialized_matches\":"<<(serialized?"true":"false")
                <<",\"thread\":"<<GetCurrentThreadId()<<",\"read_only\":true";
            logFile->event("input_pipeline_sample",f.str());samples.fetch_add(1);
            if(!match||!serialized||!captured.predicted||!captured.predictionMatches||historyTick!=captured.tick)
                failure("prediction/history/serialized input disagreement");
        }catch(const std::exception& e){failure(e.what());}
     }}
    return result;
}
inline void verify(uintptr_t rva,const char* pattern,uintptr_t caller){
    const auto bytes=parsePattern(pattern);const auto call=baseAddress+caller-5;
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0}||
       read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+rva)
        throw std::runtime_error("input signature/caller changed: rva="+hex(rva)+" caller="+hex(caller)+" expected="+pattern);
}
inline void install(){
    verify(predictRva,predictPattern,0x3a2c78);verify(encodeRva,encodePattern,0x3a2e2c);
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+predictRva),reinterpret_cast<void*>(&predict),reinterpret_cast<void**>(&originalPredict))!=MH_OK||
       MH_CreateHook(reinterpret_cast<void*>(baseAddress+encodeRva),reinterpret_cast<void*>(&encode),reinterpret_cast<void**>(&originalEncode))!=MH_OK)
        throw std::runtime_error("input pipeline observer creation failed");
    accepting=true;
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+predictRva))!=MH_OK)throw std::runtime_error("prediction observer enable failed");
    predictEnabled=true;
    if(MH_EnableHook(reinterpret_cast<void*>(baseAddress+encodeRva))!=MH_OK)throw std::runtime_error("encode observer enable failed");
    encodeEnabled=true;logFile->event("input_pipeline_enabled","\"prediction_rva\":"+hex(predictRva)+",\"encode_rva\":"+hex(encodeRva)+
        ",\"prediction_pattern\":"+jsonString(predictPattern)+",\"encode_pattern\":"+jsonString(encodePattern));
}
inline void disable(){
    accepting=false;const bool had=predictEnabled||encodeEnabled;
    if(encodeEnabled){if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+encodeRva))!=MH_OK)throw std::runtime_error("encode observer disable failed");encodeEnabled=false;}
    if(predictEnabled){if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+predictRva))!=MH_OK)throw std::runtime_error("prediction observer disable failed");predictEnabled=false;}
    if(!had)return;
    const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    verify(predictRva,predictPattern,0x3a2c78);verify(encodeRva,encodePattern,0x3a2e2c);
    logFile->event("input_pipeline_removed","\"restored\":true,\"samples\":"+std::to_string(samples.load())+
        ",\"failures\":"+std::to_string(failures.load())+",\"remaining_callbacks\":"+std::to_string(callbacks.load()));
}
}
