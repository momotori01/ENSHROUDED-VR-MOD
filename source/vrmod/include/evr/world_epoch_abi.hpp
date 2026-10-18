#pragma once
#include <cstdint>
#include <type_traits>
namespace evr::epoch {
// Versioned C-call boundary. Numeric addresses are opaque identity values only.
// No STL objects, mutexes, references, or ownership of ECS memory cross it.
inline constexpr uint32_t abiVersion=1;
enum class Operation:uint32_t {Register=1,Create=2,BeginRootClose=3,EndRootClose=4,Resolve=5,Read=6,Acquire=7,Release=8,Stop=9,Stats=10,Fault=11,CloseAdmission=12};
enum class Status:uint32_t {Ok=0,Invalid=1,AbiMismatch=2,Unavailable=3,Duplicate=4,Unknown=5,WrongToken=6,Closing=7,Capacity=8,Busy=9,Fault=10};
enum class Phase:uint32_t {Empty=0,Alive=1,Closing=2,Ended=3};
inline constexpr uint32_t flagActive=1,flagFaulted=2,flagAdmissionClosed=4;
struct Request {
 uint32_t size=sizeof(Request),version=abiVersion,operation=0,reserved=0;
 uint64_t provider=0,serial=0,world=0,registry=0,root=0,handle=0;
 uint32_t slot=0,kind=0;
};
struct Response {
 uint32_t size=sizeof(Response),version=abiVersion,status=uint32_t(Status::Unavailable),phase=0;
 uint64_t provider=0,serial=0,world=0,registry=0,root=0,handle=0;
 uint32_t slot=0,kind=0,pins=0,flags=0;
 uint32_t worlds=0,closures=0,leases=0,reserved=0;
};
static_assert(sizeof(Request)==72&&sizeof(Response)==96,"World epoch ABI layout");
static_assert(std::is_trivially_copyable_v<Request>&&std::is_standard_layout_v<Request>&&std::is_trivially_copyable_v<Response>&&std::is_standard_layout_v<Response>,"POD only ABI");
using Call=uint32_t(*)(const Request*,Response*);
inline Request request(Operation op){Request r;r.operation=uint32_t(op);return r;}
inline Request request(Operation op,const Response& from){auto r=request(op);r.provider=from.provider;r.serial=from.serial;r.world=from.world;r.registry=from.registry;r.root=from.root;r.handle=from.handle;r.slot=from.slot;r.kind=from.kind;return r;}
inline bool admitsNew(const Response& r){return r.status==uint32_t(Status::Ok)&&r.version==abiVersion&&r.size==sizeof(Response)&&r.flags==flagActive&&r.phase==uint32_t(Phase::Alive)&&r.provider&&r.serial;}
}
