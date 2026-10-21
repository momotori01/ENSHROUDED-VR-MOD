#pragma once
// The game's request of a sequence for an action (1db700: context, the item's action row (keen::ItemSequence, 236
// bytes), a 16-byte record, a flag; called by the melee trigger 275110 at 2758ad after its row selection, and by five
// others), hooked for the airborne attacks by the stroke's direction (2026-10-15 afternoon, user: "swing up in the air
// for the rising slash, down for the crash-down"). Mock game-20261015-142025 / 142407: the follow-up slots of the Actors
// the take (1d9e30) searched were empty and the takes failed (96, 160), yet the rising slash started with every airborne
// press - so the airborne attack comes through this request, with a row that names the slash-up's sequence. Here, for
// a request from the melee trigger within 250 ms of an airborne press whose direction asks for the other kind
// (controllerUiFixture::airborneRequest), the sequence number found in the row (one of the jump attacks, searched as a
// 4-byte number over the row) is replaced by the wanted kind's for the weapon group for the duration of the call and
// put back afterwards; the row belongs to the item's table and is left as it was. The first 12 requests from the melee
// trigger go on record with the record's and the row's bytes (sequence_request). Mock game-20261015-144335: neither the
// record (1, a 64-bit number) nor the row (0x170 bytes, empty but for its first number) carries the sequence, so the
// rewrite never fired - the airborne attack's sequence is chosen deeper (open; see docs/STATUS.md 2026-10-15 15:05).
// Opt-in: EVR_SEQUENCE_REQUEST=1 turns the hook on.
namespace sequenceRequestObserver {
// 1db700 reads its fifth to ninth arguments from the caller's stack ([rsp+60..80] after its prologue): the hook passes
// ten through (Mock game-20261015-143424: four alone crashed the game at the first airborne press, 0xc0000005).
using Request=uint64_t(__fastcall*)(void*,void*,void*,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t,uint64_t);
inline Request original=nullptr;inline bool enabled=false;
constexpr uintptr_t rva=0x2162f0,meleeTriggerCall=0x2c5d7e;   // the return address of the call at 2758ad
constexpr const char* pattern="48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 40 48 8B EA 49 8B F8 4C 8B";
constexpr std::array<uint32_t,4> slashUp{2251110514u,1990608986u,1548769672u,1253034539u};
constexpr std::array<uint32_t,5> crashDown{3198880426u,1061916186u,2045364040u,2614964733u,755063106u};
inline std::atomic<unsigned> logged{0},calls{0},inside{0},rewritten{0};
struct Inside {Inside(){++inside;}~Inside(){--inside;}};
inline uint32_t wanted(uint32_t current,int kind,uint8_t weaponType){
    const bool oneHand=current==2251110514u||current==3198880426u||current==1061916186u,daggers=current==1990608986u||current==2045364040u;
    const bool greatSword=current==1548769672u||current==2614964733u,hammer=current==1253034539u||current==755063106u;
    if(kind>0)return oneHand?2251110514u:daggers?1990608986u:greatSword?1548769672u:hammer?1253034539u:0u;
    if(kind<0)return oneHand?(weaponType==2?3198880426u:1061916186u):daggers?2045364040u:greatSword?2614964733u:hammer?755063106u:0u;
    return 0u;
}
inline std::string hexBytes(const uint8_t* p,size_t n){static const char* digits="0123456789abcdef";std::string s;for(size_t i=0;i<n;++i){s+=digits[p[i]>>4];s+=digits[p[i]&15];}return s;}
inline bool writable(const void* at){MEMORY_BASIC_INFORMATION m{};if(!VirtualQuery(at,&m,sizeof m)||m.State!=MEM_COMMIT)return false;const auto pr=m.Protect&0xff;return pr==PAGE_READWRITE||pr==PAGE_EXECUTE_READWRITE||pr==PAGE_WRITECOPY||pr==PAGE_EXECUTE_WRITECOPY;}
uint64_t __fastcall request(void* context,void* row,void* record,uint64_t flag,uint64_t a5,uint64_t a6,uint64_t a7,uint64_t a8,uint64_t a9,uint64_t a10){
    const Inside counted;++calls;
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress;
    const auto r=reinterpret_cast<uintptr_t>(row);
    uint32_t* field=nullptr;uint32_t before=0,after=0;size_t at=0;std::string bytes;
    const auto rec=reinterpret_cast<uintptr_t>(record);
    if(caller==meleeTriggerCall&&r&&rec)try{
        // 1db700 reads the sequence to start from the 16-byte record (eax = [record], rax = [record+8]) and fills the
        // 0x170-byte structure at `row` (Mock game-20261015-143933: that structure came in empty but for its first number)
        const auto recBytes=read<std::array<uint8_t,16>>(rec);uint32_t v0;std::memcpy(&v0,recBytes.data(),4);
        {bool jump=false;for(auto s:slashUp)if(s==v0)jump=true;for(auto s:crashDown)if(s==v0)jump=true;
         if(jump){field=reinterpret_cast<uint32_t*>(rec);before=v0;at=1000;}}
        if(!field){const auto rowBytes=read<std::array<uint8_t,236>>(r);
            for(size_t i=0;i+4<=rowBytes.size();i+=4){uint32_t v;std::memcpy(&v,rowBytes.data()+i,4);
                bool jump=false;for(auto s:slashUp)if(s==v)jump=true;for(auto s:crashDown)if(s==v)jump=true;
                if(jump){field=reinterpret_cast<uint32_t*>(r+i);before=v;at=i;break;}}}
        if(logged.load()<12)bytes=hexBytes(recBytes.data(),recBytes.size())+"|"+hexBytes(read<std::array<uint8_t,64>>(r).data(),64);
        const auto askedAt=controllerUiFixture::airborneRequest::at.load();const int kind=controllerUiFixture::airborneRequest::kind.load();
        if(field&&kind&&GetTickCount64()-askedAt<=250){const auto w=wanted(before,kind,controllerUiFixture::airborneRequest::weaponType.load());
            if(w&&w!=before&&writable(field)){*field=w;after=w;++rewritten;}}
    }catch(...){field=nullptr;after=0;}
    const auto result=original(context,row,record,flag,a5,a6,a7,a8,a9,a10);
    if(after&&field)try{*field=before;}catch(...){}
    if(caller==meleeTriggerCall&&logged.load()<12){const ObserverLastError preserve;
        try{++logged;logFile->event("sequence_request","\"caller\":"+hex(caller)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"flag\":"+std::to_string(flag&0xff)+",\"jump_sequence\":"+std::to_string(before)+",\"at\":"+std::to_string(at)+",\"written\":"+std::to_string(after)+",\"result\":"+std::to_string(result&0xff)+",\"row\":\""+bytes+"\"");}catch(...){}}
    else if(after)try{logFile->event("sequence_request_rewritten","\"frame\":"+std::to_string(liveFrame.frame)+",\"from\":"+std::to_string(before)+",\"to\":"+std::to_string(after)+",\"at\":"+std::to_string(at)+",\"result\":"+std::to_string(result&0xff));}catch(...){}
    return result;
}
inline void install() noexcept {
    try{
        // Opt-in (2026-10-15 15:00): a diagnostic on a hot path of the game; EVR_SEQUENCE_REQUEST=1 turns it on.
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SEQUENCE_REQUEST",value,sizeof value);
        if(!(n==1&&value[0]=='1'))return;
        const auto bytesWanted=parsePattern(pattern);const auto site=baseAddress+rva;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytesWanted.size(),bytesWanted)!=std::vector<size_t>{0})throw std::runtime_error("sequence request signature changed");
        const auto call=baseAddress+meleeTriggerCall-5;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=site)throw std::runtime_error("sequence request caller changed");
        if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&request),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
            throw std::runtime_error("sequence request hook failed");
        enabled=true;logFile->event("sequence_request_observer_enabled","\"rva\":"+hex(rva)+",\"melee_trigger_call\":"+hex(meleeTriggerCall));
    }catch(const std::exception& e){try{logFile->event("sequence_request_observer_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void disable() noexcept {
    if(!enabled)return;enabled=false;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));
    const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    try{logFile->event("sequence_request_observer_removed","\"status\":"+std::to_string(status)+",\"calls\":"+std::to_string(calls.load())+",\"rewritten\":"+std::to_string(rewritten.load()));}catch(...){}
}
}
