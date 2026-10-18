#include <evr/log.hpp>
#include <intrin.h>
uintptr_t arenaSyntheticCaller=0;
#define _ReturnAddress() reinterpret_cast<void*>(arenaSyntheticCaller)
#include "../layer/native_cpu_arena.hpp"
#undef _ReturnAddress
#include <array>
#include <iostream>
using namespace evr::diagnostic::cpuArena;
unsigned calls=0;
Span received{};
uint8_t receivedFlags=0;
const Name* receivedName=nullptr;
// 1.0 (2026-10-16): the flags come by value (Game_Render passes 0x80); the call that makes Game_Render returns to 0x838098 (0x838118 in Hotfix 1.0.0.1).
void* __fastcall fakeInitialize(void* object,const Span* span,uint8_t flags,const Name* name){
    ++calls;received=*span;receivedFlags=flags;receivedName=name;
    std::memcpy(static_cast<uint8_t*>(object)+0x78,span,16);SetLastError(777);
    return reinterpret_cast<void*>(0x12345678);
}
int main(){
    unsigned passed=0,failed=0;
    auto check=[&](const char* name,bool condition){std::cout<<(condition?"PASS ":"FAIL ")<<name<<'\n';condition?++passed:++failed;};
    base=0x10000000;original=&fakeInitialize;requestedBytes=16ull*1024*1024;
    std::array<uint8_t,0x110> object{};Span native{reinterpret_cast<void*>(0x20000000),450ull*1024*1024};
    const auto before=native;const uint8_t flags=0x80;Name name{"Game_Render",11};
    arenaSyntheticCaller=base+0x8386d7;
    initialize(object.data(),&native,flags,&name);
    check("exact_caller_required",calls==1&&!consumed&&backing==nullptr&&received.data==native.data);
    arenaSyntheticCaller=base+0x8386d8;Name wrong{"Game_Client",11};
    initialize(object.data(),&native,flags,&wrong);
    check("exact_native_allocator_name_required",calls==2&&!consumed&&backing==nullptr);
    Span changed=native;changed.size+=8;initialize(object.data(),&changed,flags,&name);
    check("native_size_must_match",calls==3&&!consumed&&backing==nullptr);
    void* result=initialize(object.data(),&native,flags,&name);
    check("independent_backing_allocated",calls==4&&consumed&&backing&&received.data==backing&&received.size==requestedBytes);
    check("native_constructor_parameters_preserved",receivedFlags==flags&&receivedName==&name&&result==reinterpret_cast<void*>(0x12345678)&&GetLastError()==777);
    check("original_partition_descriptor_unchanged",native.data==before.data&&native.size==before.size);
    check("callback_quiescent_after_return",callbacks==0);
    initialize(object.data(),&native,flags,&name);
    check("one_backing_per_process_second_init_native",calls==5&&received.data==native.data&&received.size==native.size);
    // This fixture has no native allocator/users. Production retains until exit.
    if(backing)VirtualFree(backing,0,MEM_RELEASE);
    std::cout<<"RESULT "<<passed<<" PASS "<<failed<<" FAIL\n";
    return failed?1:0;
}
