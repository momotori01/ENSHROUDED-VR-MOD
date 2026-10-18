// Offline ownership test for the actual diagnostic facade. No game process,
// injection, GPU work or native allocator calls. Caller metadata is a test seam.
#define NOMINMAX
#include <windows.h>
#include <intrin.h>
#include <array>
#include <algorithm>
#include <atomic>
#include <iostream>
#include <cstring>
#include <filesystem>
#include <evr/log.hpp>
#include <evr/signature.hpp>
#include <evr/observer_error.hpp>
#include <MinHook.h>
extern "C" MH_STATUS WINAPI MH_CreateHook(LPVOID,LPVOID,LPVOID*){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_EnableHook(LPVOID){return MH_ERROR_UNSUPPORTED_FUNCTION;}
extern "C" MH_STATUS WINAPI MH_DisableHook(LPVOID){return MH_ERROR_UNSUPPORTED_FUNCTION;}
namespace test {
using namespace evr;
uintptr_t baseAddress=0x10000000,syntheticCaller=0;
Log* logFile=nullptr;
void* privateResolution=nullptr; // Factory observation scope is inactive in this ownership fixture.
std::string hex(uintptr_t value){std::ostringstream out;out<<'"'<<"0x"<<std::hex<<value<<'"';return out.str();}
template<class T>T read(uintptr_t address){T result{};std::memcpy(&result,reinterpret_cast<void*>(address),sizeof(result));return result;}
#define _ReturnAddress() reinterpret_cast<void*>(syntheticCaller)
#include "../hooks/detail_factory_probe.hpp"
#include "../hooks/world_ready_probe.hpp"
#include "../hooks/world_shared_probe.hpp"
#include "../hooks/world_shared_stages.hpp"
#undef _ReturnAddress
struct Native {
    uintptr_t result=0,freed=0,context=0;
    uint64_t size=0,alignment=0;
    uint8_t inputTag=0,flags=0;
    unsigned allocations=0,frees=0;
} native;
void* __fastcall fakeAllocate(void* context,uint64_t size,uint64_t alignment,uint8_t* tag,uint8_t flags){
    native.context=reinterpret_cast<uintptr_t>(context);native.size=size;native.alignment=alignment;
    native.inputTag=*tag;native.flags=flags;++native.allocations;*tag=0x31;SetLastError(1234);
    return reinterpret_cast<void*>(native.result);
}
void __fastcall fakeFree(void* context,void* pointer,uint8_t* tag){
    native.context=reinterpret_cast<uintptr_t>(context);native.freed=reinterpret_cast<uintptr_t>(pointer);
    native.inputTag=*tag;++native.frees;*tag=0x22;SetLastError(5678);
}
void* __fastcall fakeAllocateResult(void* context,void* output,uint64_t size,uint64_t alignment,uint8_t* tag,uint8_t flags){
    fakeAllocate(context,size,alignment,tag,flags);
    auto* words=static_cast<uint64_t*>(output);words[0]=native.result;words[1]=native.result?size:0;words[2]=0;
    return output;
}
bool gateResult=false;void* gateObject=nullptr;void* gateValue=nullptr;
bool __fastcall fakeGate(void* object){gateObject=object;SetLastError(2468);return gateResult;}
bool __fastcall fakeSceneGate(void* object,void* value){gateObject=object;gateValue=value;SetLastError(2468);return gateResult;}
DWORD freeIncomingError=0;
void* __fastcall fakeFreeResult(void* context,void* output,void* pointer,uint8_t* tag){
    freeIncomingError=GetLastError();fakeFree(context,pointer,tag);*static_cast<uintptr_t*>(output)=reinterpret_cast<uintptr_t>(pointer);return output;
}
std::array<void*,3> stageArguments{};unsigned stageCalls=0;
void __fastcall fakeSharedStage(void* graph,void* object,void* params){stageArguments={graph,object,params};++stageCalls;SetLastError(4680);}
} // namespace test
int main(){
    using namespace test;
    using namespace test::detailFactoryDiagnostic;
    unsigned passed=0,failed=0;
    auto check=[&](const char* name,bool value){std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';value?++passed:++failed;};
    std::array<Body,2> bodies{};bodies[0].address=0x12345000;bodies[0].returnedTag=0x48;bodies[0].returnedError=999;
    AllocatorFacade facade{nullptr,0xabcd,&fakeAllocate,&fakeFree,&bodies};
    syntheticCaller=baseAddress+0x4776e;
    uint8_t tag=8;void* pointer=allocate(&facade,0x210,8,&tag,0);
    check("prefetched_body_exact_caller",reinterpret_cast<uintptr_t>(pointer)==bodies[0].address&&bodies[0].consumed&&native.allocations==0);
    check("prefetched_tag_and_last_error",tag==0x48&&GetLastError()==999);
    tag=0;free(&facade,pointer,&tag);
    check("owned_body_free_forwarded_once",bodies[0].freed&&native.frees==1&&native.freed==bodies[0].address&&native.context==0xabcd);
    check("free_tag_and_last_error",tag==0x22&&GetLastError()==5678);
    // TLSF legitimately reuses the freed body address for a new tagged array.
    syntheticCaller=baseAddress+0x7af780;native.result=bodies[0].address;tag=4;
    pointer=allocate(&facade,0x5000,4,&tag,1);
    check("forwarded_allocation_abi",native.context==0xabcd&&native.size==0x5000&&native.alignment==4&&native.inputTag==4&&native.flags==1);
    check("forwarded_allocation_return_tag_last_error",reinterpret_cast<uintptr_t>(pointer)==native.result&&tag==0x31&&GetLastError()==1234);
    tag=0;free(&facade,pointer,&tag);
    check("reused_body_address_array_free_not_suppressed",native.frees==2&&native.freed==bodies[0].address&&!facade.contractViolation&&bodies[0].freed);
    check("forwarded_allocation_tracking_cleared",std::none_of(facade.forwarded.begin(),facade.forwarded.end(),[](const auto& a){return a.active;}));
    tag=0;free(&facade,reinterpret_cast<void*>(bodies[0].address),&tag);
    check("actual_duplicate_body_free_refused",native.frees==2&&facade.contractViolation);
    facade.contractViolation=false;bodies[1].address=0x54321000;facade.eye=1;
    const auto allocationsBefore=native.allocations;native.result=0x34567000;syntheticCaller=baseAddress+0x4776d;tag=8;
    pointer=allocate(&facade,0x210,8,&tag,0);
    check("wrong_caller_does_not_consume_prefetched_body",!bodies[1].consumed&&native.allocations==allocationsBefore+1&&reinterpret_cast<uintptr_t>(pointer)==native.result);
    tag=0;free(&facade,pointer,&tag);
    syntheticCaller=baseAddress+0x7af780;native.result=0;tag=4;
    const auto beforeNull=native.allocations;pointer=allocate(&facade,0x14000,4,&tag,0);
    check("native_allocation_failure_no_retry",pointer==nullptr&&native.allocations==beforeNull+1&&tag==0x31&&GetLastError()==1234);
    check("failed_allocation_not_tracked",std::none_of(facade.forwarded.begin(),facade.forwarded.end(),[](const auto& a){return a.active;}));
    originalAllocateResult=&fakeAllocateResult;
    std::array<uint8_t,0x110> allocatorBytes{};
    std::array<uint64_t,3> output{};
    Scope scope;scope.discoverBody=true;native.result=0x23456000;tag=4;
    syntheticCaller=baseAddress+0x7e3872;
    {ScopeLease lease(scope);pointer=traceAllocateResult(allocatorBytes.data(),output.data(),0x5000,4,&tag,0);}
    check("allocation_observer_preserves_six_arguments_and_return",pointer==output.data()&&output[0]==native.result&&native.size==0x5000&&native.alignment==4&&native.flags==0&&native.inputTag==4&&tag==0x31&&GetLastError()==1234);
    check("allocation_observer_fixed_record",scope.allocationCount==1&&scope.allocations[0].caller==0x7e3872&&scope.allocations[0].result==native.result&&scope.allocations[0].tagBefore==4&&scope.allocations[0].tagAfter==0x31&&scope.allocations[0].before.readable&&scope.allocations[0].after.readable);
    native.result=0;tag=4;
    {ScopeLease lease(scope);pointer=traceAllocateResult(allocatorBytes.data(),output.data(),0x14000,4,&tag,7);}
    check("allocation_observer_preserves_native_null",pointer==output.data()&&output[0]==0&&scope.allocations[1].result==0&&scope.allocations[1].flags==7&&GetLastError()==1234);
    const auto beforeOverflow=native.allocations;
    {ScopeLease lease(scope);for(unsigned i=0;i<20;++i){tag=4;traceAllocateResult(allocatorBytes.data(),output.data(),0x5000,4,&tag,0);}}
    check("allocation_observer_overflow_still_forwards",scope.allocationCount==16&&scope.allocationOverflow&&native.allocations==beforeOverflow+20&&GetLastError()==1234);
    tag=8;const auto beforeUnscoped=native.allocations;
    traceAllocateResult(allocatorBytes.data(),output.data(),0x210,8,&tag,0);
    check("allocation_observer_ignores_unscoped_game_calls",scope.allocationCount==16&&native.allocations==beforeUnscoped+1&&GetLastError()==1234);
    worldReadyDiagnostic::Scope ready;
    worldReadyDiagnostic::entries[0].original=reinterpret_cast<void*>(&fakeSceneGate);
    worldReadyDiagnostic::entries[1].original=reinterpret_cast<void*>(&fakeGate);
    worldReadyDiagnostic::current=&ready;syntheticCaller=baseAddress+0xf7f4e;
    bool readyResult=worldReadyDiagnostic::scene(reinterpret_cast<void*>(0x1234),reinterpret_cast<void*>(0x5678));
    check("world_scene_gate_preserves_two_arguments_false_error",!readyResult&&gateObject==reinterpret_cast<void*>(0x1234)&&gateValue==reinterpret_cast<void*>(0x5678)&&GetLastError()==2468&&ready.count==1&&!ready.observations[0].result);
    syntheticCaller=baseAddress+0xf7f7f;gateResult=true;readyResult=worldReadyDiagnostic::one<1>(reinterpret_cast<void*>(0x4321));
    check("world_one_argument_gate_preserves_true_error",readyResult&&gateObject==reinterpret_cast<void*>(0x4321)&&GetLastError()==2468&&ready.count==2&&ready.observations[1].result);
    syntheticCaller=0;worldReadyDiagnostic::one<1>(nullptr);worldReadyDiagnostic::current=nullptr;
    syntheticCaller=baseAddress+0xf7f7f;worldReadyDiagnostic::one<1>(nullptr);
    check("world_gate_observes_only_scoped_exact_callers",ready.count==2&&GetLastError()==2468);
    worldSharedDiagnostic::originalFree=&fakeFreeResult;
    std::array<uint64_t,4> sharedWords{0,104,11,22};uintptr_t freedResult=0;
    auto* sharedPointer=sharedWords.data()+2;worldSharedDiagnostic::watched[0]=reinterpret_cast<uintptr_t>(sharedPointer);
    worldSharedDiagnostic::file=CreateFileW(L"docs/analysis/detail_facade_tests/shared-free.jsonl",FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    check("shared_free_observer_trace_open",worldSharedDiagnostic::file!=INVALID_HANDLE_VALUE);
    tag=7;SetLastError(9753);
    auto* freeResult=worldSharedDiagnostic::freeMemory(reinterpret_cast<void*>(0xabcd),&freedResult,sharedPointer,&tag);
    check("shared_free_observer_preserves_arguments_return_tag_errors",freeResult==&freedResult&&freedResult==reinterpret_cast<uintptr_t>(sharedPointer)&&native.context==0xabcd&&native.inputTag==7&&tag==0x22&&freeIncomingError==9753&&GetLastError()==5678);
    check("shared_free_observer_never_changes_allocation",sharedWords==std::array<uint64_t,4>{0,104,11,22}&&worldSharedDiagnostic::calls.load()==1&&worldSharedDiagnostic::callbacks.load()==0);
    tag=0;SetLastError(123);
    worldSharedDiagnostic::freeMemory(nullptr,&freedResult,nullptr,&tag);
    check("shared_free_observer_ignores_unwatched_but_forwards",worldSharedDiagnostic::calls.load()==1&&freedResult==0&&freeIncomingError==123&&GetLastError()==5678);
    CloseHandle(worldSharedDiagnostic::file);worldSharedDiagnostic::file=INVALID_HANDLE_VALUE;
    worldSharedStages::entries[0].original=&fakeSharedStage;
    worldSharedStages::once=true;syntheticCaller=baseAddress+0xf9cc9;
    worldSharedStages::Context stage;stage.shared=0x7788;stage.serial=101;
    {worldSharedStages::Scope scope(&stage);worldSharedStages::invoke<0>(reinterpret_cast<void*>(0x11),reinterpret_cast<void*>(0x7788),reinterpret_cast<void*>(0x22));}
    check("shared_update_original_arguments_and_completion",stageCalls==1&&stageArguments==std::array<void*,3>{reinterpret_cast<void*>(0x11),reinterpret_cast<void*>(0x7788),reinterpret_cast<void*>(0x22)}&&GetLastError()==4680&&worldSharedStages::completed[0].valid);
    stage.eye=0;SetLastError(1234);
    {worldSharedStages::Scope scope(&stage);worldSharedStages::invoke<0>(nullptr,reinterpret_cast<void*>(0x7788),nullptr);}
    check("shared_update_eye_skips_only_completed_same_frame",stageCalls==1&&stage.skipped[0]==1&&!stage.rejected&&GetLastError()==1234);
    stage.serial=102;
    {worldSharedStages::Scope scope(&stage);worldSharedStages::invoke<0>(nullptr,reinterpret_cast<void*>(0x7788),nullptr);}
    check("shared_update_stale_frame_refuses_duplicate_work",stageCalls==1&&stage.rejected&&stage.skipped[0]==2);
    stage.rejected=false;stage.serial=101;syntheticCaller=0;
    {worldSharedStages::Scope scope(&stage);worldSharedStages::invoke<0>(nullptr,reinterpret_cast<void*>(0x7788),nullptr);}
    check("shared_update_wrong_caller_keeps_native_behavior",stageCalls==2&&!stage.rejected);
    syntheticCaller=baseAddress+0xf9cc9;
    {worldSharedStages::Scope scope(&stage);worldSharedStages::invoke<0>(nullptr,reinterpret_cast<void*>(0x7799),nullptr);}
    check("shared_update_other_object_keeps_native_behavior",stageCalls==3&&!stage.rejected);
    worldSharedStages::once=false;
    {worldSharedStages::Scope scope(&stage);worldSharedStages::invoke<0>(nullptr,reinterpret_cast<void*>(0x7788),nullptr);}
    check("shared_update_observation_mode_forwards_eyes",stageCalls==4&&!stage.rejected&&worldSharedStages::current==nullptr);
    std::cout<<"RESULT "<<passed<<" PASS "<<failed<<" FAIL\n";
    return failed?1:0;
}
