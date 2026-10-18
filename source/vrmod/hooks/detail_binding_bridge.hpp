// Match native detail draw descriptors to the private wrapper used by eeff0/eb590.
// The original scene/wrapper and all resource owners remain untouched.
namespace detailBinding {
using Get=void*(__fastcall*)(void*);
inline Get original=nullptr;
inline bool requested=false,enabled=false,failed=false;
inline std::array<uint64_t,2> substitutions{},readyChecks{},warmupCalls{};
inline uint64_t failures=0;
// The game's "small foliage" setting can be switched off while playing: the original View then has no detail object
// (the getter returns null) and there is nothing to substitute. Mock run game-20261013-032832: the session ended
// there with "unreadable view metadata" (the checks below read through the null). Such a call counts as handled.
// When the setting comes back the game makes a new detail object, and the private ones of the eyes - made when the
// session began - no longer belong to it: from then on the eyes draw with the original's object, as they did before
// this bridge existed (`passThrough`), instead of ending the session.
inline bool passThrough=false,wasAbsent=false;inline uint64_t absentCalls=0,passThroughCalls=0;
struct Context {PairResources* pair;unsigned eye;unsigned calls=0;};
inline thread_local Context* current=nullptr;
struct Scope {
    Context context;Context* previous;
    Scope(PairResources& pair,unsigned eye):context{&pair,eye},previous(current){if(requested)current=&context;}
    ~Scope(){current=previous;}
};
// 1.0 (Hotfix 1.0.0.1): the getter 0xf3b20 reads the detail object through the owner's +0xe8 (layout::ownerResolution;
// June 0xc2290: +0xc0), the owner's View is +0xa0 (layout::ownerView, June +0x78); the getter is called
// from the shading bindings (0x13d7d0, June 0x10aaf0) at 0x13d9d7.
inline uintptr_t detail(uintptr_t wrapper){return read<uintptr_t>(read<uintptr_t>(wrapper+layout::ownerResolution));}
// 1.0 (the two objects side by side, Mock game-20261016-111408): eleven buffer handles from +0x18, 0x10 apart (June:
// eight), then the shared pointer +0xd0 (June +0xa0) and the object itself +0xe0 (June +0xb0). The indirect list and the
// sorted instances (June the handles at +0x38 / +0x58) are found by their sizes.
constexpr size_t detailHandles=0x18,detailHandleCount=11,detailShared=0xd0,detailSelf=0xe0;
inline uintptr_t buffer(PairResources& pair,uintptr_t owner,size_t offset,uint64_t bytes);
inline uint64_t handleBytes(PairResources& pair,uintptr_t owner,size_t offset){
    const auto h=owner+offset;const auto graph=read<uintptr_t>(h);const auto index=read<uint16_t>(h+8);
    if(graph!=pair.graph||index>=read<uint64_t>(graph+0x270))throw std::runtime_error("detail binding graph handle differs");
    const auto object=read<uintptr_t>(read<uintptr_t>(graph+0x268)+uintptr_t(index)*8);
    const auto bytes=object?read<uint64_t>(object+8):0;if(!bytes)throw std::runtime_error("detail binding native buffer extent differs");
    return bytes;
}
// Several handles can have the size; then the one at June's place (+0x38 / +0x58) is taken when it is among them.
inline size_t handleOfSize(PairResources& pair,uintptr_t owner,uint64_t bytes,size_t junePlace){
    std::vector<size_t> found;std::string sizes;
    for(size_t k=0;k<detailHandleCount;++k){const auto h=owner+detailHandles+k*0x10;const auto graph=read<uintptr_t>(h);const auto index=read<uint16_t>(h+8);
        uint64_t size=0;
        if(graph==pair.graph&&index<read<uint64_t>(graph+0x270)){
            const auto object=read<uintptr_t>(read<uintptr_t>(graph+0x268)+uintptr_t(index)*8);
            if(object)size=read<uint64_t>(object+8);}
        sizes+=(k?",":"")+std::to_string(size);
        if(size==bytes)found.push_back(detailHandles+k*0x10);}
    if(found.size()==1)return found[0];
    if(found.size()>1&&std::find(found.begin(),found.end(),junePlace)!=found.end())return junePlace;
    throw std::runtime_error("detail binding buffer of that size not unique (sizes "+sizes+")");
}
inline uintptr_t buffer(PairResources& pair,uintptr_t owner,size_t offset,uint64_t bytes){
    const auto h=owner+offset,graph=read<uintptr_t>(h);const auto index=read<uint16_t>(h+8);
    // 1.0: the graph's buffer objects +0x268, their count +0x270 (June +0x110 / +0x118; culling_history_bridge.hpp).
    if(graph!=pair.graph||index>=read<uint64_t>(graph+0x270))throw std::runtime_error("detail binding graph handle differs");
    const auto object=read<uintptr_t>(read<uintptr_t>(graph+0x268)+uintptr_t(index)*8);
    if(!object||read<uint64_t>(object+8)!=bytes||!read<uintptr_t>(object+0x18))
        throw std::runtime_error("detail binding native buffer extent differs");
    return read<uintptr_t>(object+0x18);
}
void* __fastcall get(void* wrapper){
    const auto caller=reinterpret_cast<uintptr_t>(_ReturnAddress());
    auto result=original(wrapper);const ObserverLastError preserve;
    // 10aaf0 uses Scene[0], while native detail draws use View[0]. Restrict
    // substitution to this one descriptor-building call in our render scope.
    if(!current||caller!=baseAddress+0x13da1c)return result;
    auto& c=*current;auto& pair=*c.pair;
    try{
        if(failed||c.eye>1||GetCurrentThreadId()!=pair.renderThread||++c.calls!=1)
            throw std::runtime_error("detail binding private render scope differs");
        if(!result){
            // Switched off in the settings: nothing to draw, nothing to substitute.
            ++substitutions[c.eye];
            if(!wasAbsent||absentCalls%600==0)logFile->event("detail_binding_absent","\"eye\":"+std::to_string(c.eye)+",\"serial\":"+std::to_string(pair.serial)+",\"frame\":"+std::to_string(liveFrame.frame)+
                ",\"calls\":"+std::to_string(absentCalls)+",\"original_has_no_detail_object\":true,\"nothing_substituted\":true");
            wasAbsent=true;++absentCalls;return result;
        }
        if(passThrough){++substitutions[c.eye];++passThroughCalls;return result;}
        const auto sourceWrapper=read<uintptr_t>(pair.originalView);
        const auto eyeWrapper=reinterpret_cast<uintptr_t>(pair.owners[c.eye].data());
        const auto source=detail(sourceWrapper),own=detail(eyeWrapper);
        const char* differs=reinterpret_cast<uintptr_t>(wrapper)!=sourceWrapper||read<uintptr_t>(pair.scene)!=sourceWrapper?"source wrapper":
           read<uintptr_t>(pair.created[c.eye])!=eyeWrapper?"eye view":read<uintptr_t>(eyeWrapper+layout::ownerView)!=pair.created[c.eye]?"eye wrapper":
           reinterpret_cast<uintptr_t>(result)!=source?"result":!own||own==source?"own":
           own==detail(reinterpret_cast<uintptr_t>(pair.owners[1-c.eye].data()))?"other eye":
           read<uintptr_t>(own+detailShared)!=read<uintptr_t>(source+detailShared)?"+a0":read<uintptr_t>(own+detailSelf)!=own?"+b0":nullptr;
        // 1.0 (2026-10-16): the detail object's own fields are not placed yet. The owners and the getter are; past them,
        // the first time, both objects go to the record (0x200 bytes each) and the eyes draw with the original detail
        // object for the session (as before this bridge, `passThrough`) instead of the session ending.
        const bool layoutKnown=!differs;
        auto unplaced=[&](const char* what){
            static std::atomic<bool> dumped{false};
            if(!dumped.exchange(true))try{auto hexOf=[](uintptr_t a){std::string s;char b[4];for(auto v:read<std::array<uint8_t,0x200>>(a)){std::snprintf(b,sizeof b,"%02x",v);s+=b;}return s;};
                logFile->event("detail_binding_layout_unplaced","\"what\":\""+std::string(what)+"\",\"own\":"+hex(own)+",\"source\":"+hex(source)+
                    ",\"own_bytes\":\""+hexOf(own)+"\",\"source_bytes\":\""+hexOf(source)+"\",\"graph\":"+hex(pair.graph));}catch(...){}
            passThrough=true;++substitutions[c.eye];++passThroughCalls;};
        if(differs&&(!std::strcmp(differs,"+a0")||!std::strcmp(differs,"+b0"))){unplaced(differs);return result;}
        if(!layoutKnown)throw std::runtime_error(std::string("detail binding owner topology differs (")+differs+")");
        // These are exactly the indirect list and sorted instances used by
        // native detail shader preparation (4cf00 and 10f993 onward).
        uintptr_t ownIndirect=0,sourceIndirect=0,ownInstances=0,sourceInstances=0;
        // 1.0 (Mock game-20261016-112850, the eleven sizes 0xa0000 0x280000 0x300000 0x100 0x280000 0x20000 0x20000 0xc
        // 0x100 0x100 0x80000): no 0x60000 any more; the handles at June's places (+0x38 / +0x58) are taken, each with
        // the size the original object's has.
        try{const auto indirectBytes=handleBytes(pair,source,0x38),instancesBytes=handleBytes(pair,source,0x58);
            ownIndirect=buffer(pair,own,0x38,indirectBytes);sourceIndirect=buffer(pair,source,0x38,indirectBytes);
            ownInstances=buffer(pair,own,0x58,instancesBytes);sourceInstances=buffer(pair,source,0x58,instancesBytes);}
        catch(const std::exception& e){unplaced(e.what());return result;}
        if(ownIndirect==sourceIndirect||ownInstances==sourceInstances)
            throw std::runtime_error("detail binding private buffers alias source");
        const auto count=++substitutions[c.eye];
        if(count<=3||count%120==0)logFile->event("detail_binding_substituted","\"eye\":"+std::to_string(c.eye)+
            ",\"serial\":"+std::to_string(pair.serial)+",\"frame\":"+std::to_string(liveFrame.frame)+
            ",\"count\":"+std::to_string(count)+",\"source\":"+hex(source)+",\"private\":"+hex(own)+
            ",\"source_indirect\":"+hex(sourceIndirect)+",\"private_indirect\":"+hex(ownIndirect)+
            ",\"source_instances\":"+hex(sourceInstances)+",\"private_instances\":"+hex(ownInstances)+
            ",\"source_owner_unchanged\":true,\"scope\":\"10acf9_private_eye\"");
        return reinterpret_cast<void*>(own);
    }catch(const std::exception& e){
        if(wasAbsent&&!failed&&c.eye<=1&&c.calls==1){
            // The detail object came back as a new one (see passThrough above).
            passThrough=true;++substitutions[c.eye];++passThroughCalls;
            try{logFile->event("detail_binding_pass_through","\"eye\":"+std::to_string(c.eye)+",\"serial\":"+std::to_string(pair.serial)+",\"frame\":"+std::to_string(liveFrame.frame)+
                ",\"reason\":"+jsonString(e.what())+",\"eyes_draw_with_the_original_detail_object\":true,\"until_the_session_ends\":true");}catch(...){}
            return result;
        }
        failed=true;++failures;
        try{logFile->event("detail_binding_failure","\"reason\":"+jsonString(e.what()));}catch(...){}
        return result;}
}
inline void auditEye(unsigned eye,bool before,bool after,uint64_t countBefore){
    if(!requested)return;
    const auto calls=substitutions.at(eye)-countBefore;
    if(before&&after){if(calls!=1)throw std::runtime_error("ready eye omitted private detail shader owner");++readyChecks[eye];}
    else warmupCalls[eye]+=calls;
}
inline void install(){
    const auto pattern=parsePattern("48 8B 81 E8 00 00 00 48 8B 00 C3");
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+0xf3b50),pattern.size(),pattern)!=std::vector<size_t>{0})
        throw std::runtime_error("detail owner getter signature differs");
    const auto* call=reinterpret_cast<const uint8_t*>(baseAddress+0x13da17);int32_t delta=0;std::memcpy(&delta,call+1,4);
    if(*call!=0xe8||reinterpret_cast<uintptr_t>(call+5)+delta!=baseAddress+0xf3b50)
        throw std::runtime_error("detail descriptor callsite differs");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+0xf3b50),reinterpret_cast<void*>(&get),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+0xf3b50))!=MH_OK)throw std::runtime_error("detail owner getter hook failed");
    enabled=true;logFile->event("detail_binding_enabled","\"private_eye_only\":true,\"source_owner_unchanged\":true");
}
inline void disable(){
    if(!enabled)return;
    const auto result=MH_DisableHook(reinterpret_cast<void*>(baseAddress+0xf3b50));
    if(result!=MH_OK)throw std::runtime_error("detail binding hook disable failed");enabled=false;
    logFile->event("detail_binding_disabled","\"left\":"+std::to_string(substitutions[0])+",\"right\":"+std::to_string(substitutions[1])+
        ",\"ready_checks\":["+std::to_string(readyChecks[0])+","+std::to_string(readyChecks[1])+"],\"warmup_calls\":["+
        std::to_string(warmupCalls[0])+","+std::to_string(warmupCalls[1])+"]"+
        ",\"failures\":"+std::to_string(failures)+",\"absent_calls\":"+std::to_string(absentCalls)+",\"pass_through_calls\":"+std::to_string(passThroughCalls)+",\"hook_disabled\":true");
}
}
