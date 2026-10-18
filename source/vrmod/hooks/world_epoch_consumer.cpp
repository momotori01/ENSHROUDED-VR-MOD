#include <evr/world_epoch_abi.hpp>
#include <evr/log.hpp>
#include <windows.h>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
using namespace evr;
namespace {
std::atomic<bool> started{false};
std::string fields(const epoch::Response& r){
 std::ostringstream s;s<<"\"provider\":"<<r.provider<<",\"serial\":"<<r.serial<<",\"slot\":"<<r.slot<<",\"world\":\"0x"<<std::hex<<r.world<<"\",\"registry\":\"0x"<<r.registry<<"\",\"owner_root\":\"0x"<<r.root<<'"'<<std::dec<<",\"kind\":"<<r.kind<<",\"phase\":"<<r.phase<<",\"pins\":"<<r.pins<<",\"handle\":"<<r.handle<<",\"flags\":"<<r.flags<<",\"worlds\":"<<r.worlds<<",\"leases\":"<<r.leases;return s.str();
}
}
// Separate DLL; no World book, no native hooks, and no ECS reads/writes.
extern "C" __declspec(dllexport) DWORD WINAPI EVR_ObserveWorldEpochConsumer(void* directory){
 if(!directory||started.exchange(true))return 2;
 HMODULE pinned{};if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&EVR_ObserveWorldEpochConsumer),&pinned))return 3;
 std::unique_ptr<Log> log;epoch::Call fn=nullptr;std::array<epoch::Response,3> leases{};
 try{
  const auto path=std::filesystem::path(static_cast<wchar_t*>(directory));log=std::make_unique<Log>((path/"world-epoch-consumer.jsonl").string());
  const bool earlyAdmission=std::filesystem::exists(path/"world-preclose-admission.txt");
  if(earlyAdmission){std::ifstream f(path/"world-preclose-admission.txt");std::string value;f>>value>>std::ws;if(!f||!f.eof()||value!="EVR_WORLD_PRECLOSE_ADMISSION_1")throw std::runtime_error("bad early admission consumer policy");}
  std::ifstream policy(path/"world-epoch-consumer.txt");std::string magic;unsigned seconds=0;uint64_t provider=0;
  policy>>magic>>seconds>>provider;
  if(!policy||magic!="EVR_WORLD_EPOCH_CONSUMER_1"||seconds<30||seconds>150||!provider)throw std::runtime_error("invalid finite consumer policy");
  std::array<epoch::Request,3> identities{};unsigned kinds=0;
  for(auto& r:identities){r=epoch::request(epoch::Operation::Resolve);r.provider=provider;policy>>r.world>>r.registry>>r.root>>r.kind>>r.slot>>r.serial;
   if(!policy||!r.world||!r.registry||!r.root||!r.serial||r.slot>=8||r.kind>2||(kinds&(1u<<r.kind)))throw std::runtime_error("invalid distinct World identities");kinds|=1u<<r.kind;}
  policy>>std::ws;if(!policy.eof()||kinds!=7)throw std::runtime_error("extra consumer policy data");
  auto module=GetModuleHandleW(L"evr_diagnostic.dll");fn=module?reinterpret_cast<epoch::Call>(GetProcAddress(module,"evrWorldEpochCall")):nullptr;if(!fn)throw std::runtime_error("process World provider missing");
  auto call=[&](const epoch::Request& r,epoch::Status expected=epoch::Status::Ok){epoch::Response out;const auto saved=GetLastError();auto result=fn(&r,&out);
   if(GetLastError()!=saved||result!=uint32_t(expected)||out.status!=result||out.size!=sizeof(out)||out.version!=epoch::abiVersion)throw std::runtime_error("consumer ABI/status/LastError mismatch");return out;};
  for(size_t i=0;i<identities.size();++i){const auto& r=identities[i];auto resolved=call(r);
   if(!epoch::admitsNew(resolved)||resolved.provider!=r.provider||resolved.serial!=r.serial||resolved.slot!=r.slot||resolved.world!=r.world||resolved.registry!=r.registry||resolved.root!=r.root||resolved.kind!=r.kind)throw std::runtime_error("consumer resolved a different World");
   leases[i]=call(epoch::request(epoch::Operation::Acquire,resolved));if(!leases[i].handle)throw std::runtime_error("missing unique consumer lease");
   log->event("world_consumer_pinned",fields(leases[i])+",\"ecs_access\":false");
  }
  log->event("world_consumer_ready","\"worlds\":3,\"seconds\":"+std::to_string(seconds)+",\"separate_dll\":true");
  const auto deadline=GetTickCount64()+uint64_t(seconds)*1000;std::array<bool,3> sawClosing{},sawAdmission{};unsigned retired=0;
  while(retired<3&&GetTickCount64()<deadline){
   for(size_t i=0;i<leases.size();++i){if(!leases[i].handle)continue;auto value=call(epoch::request(epoch::Operation::Read,leases[i]));
    if(value.provider!=provider||!value.pins||(value.flags!=epoch::flagActive&&(!earlyAdmission||value.flags!=(epoch::flagActive|epoch::flagAdmissionClosed))))throw std::runtime_error("consumer lost owned generation");
    if(earlyAdmission&&(value.flags&epoch::flagAdmissionClosed)&&!sawAdmission[i]){
     if(epoch::admitsNew(value))throw std::runtime_error("early admission still permits new work");
     auto q=epoch::request(epoch::Operation::Acquire,value);q.handle=0;call(q,epoch::Status::Closing);
     log->event("world_consumer_admission_closed",fields(value)+",\"new_lease_rejected\":true,\"ecs_access\":false");sawAdmission[i]=true;
    }
    if(value.phase==uint32_t(epoch::Phase::Alive))continue;
    if(epoch::admitsNew(value))throw std::runtime_error("closed generation still admits new work");
    auto acquire=epoch::request(epoch::Operation::Acquire,value);acquire.handle=0;call(acquire,epoch::Status::Closing);
    if(value.phase==uint32_t(epoch::Phase::Closing)){if(!sawClosing[i]){log->event("world_consumer_closing",fields(value));sawClosing[i]=true;}continue;}
    if(value.phase!=uint32_t(epoch::Phase::Ended))throw std::runtime_error("invalid consumer World phase");
    log->event("world_consumer_ended",fields(value)+",\"new_lease_rejected\":true");
    auto release=epoch::request(epoch::Operation::Release,leases[i]);auto released=call(release);log->event("world_consumer_released",fields(released));
    call(release,epoch::Status::Unknown);call(epoch::request(epoch::Operation::Read,leases[i]),epoch::Status::WrongToken);
    log->event("world_consumer_old_rejected",fields(leases[i])+",\"duplicate_release_rejected\":true,\"reclaimed_token_rejected\":true");leases[i]={};++retired;
   }
   if(retired<3)std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  if(earlyAdmission&&(!sawAdmission[0]||!sawAdmission[1]||!sawAdmission[2]))throw std::runtime_error("not all owners observed early admission closure");
  if(retired!=3)throw std::runtime_error("native World exit not observed before consumer deadline");
  log->event("world_consumer_complete","\"result\":0,\"worlds_retired\":3,\"ecs_access\":false");return 0;
 }catch(const std::exception& e){
  // Release only exact outstanding handles. Never reset the provider on failure.
  if(fn)for(auto& lease:leases)if(lease.handle){auto q=epoch::request(epoch::Operation::Release,lease);epoch::Response out;fn(&q,&out);}
  if(log)try{log->event("world_consumer_failure","\"reason\":"+jsonString(e.what()));}catch(...){}return 1;
 }
}
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH)DisableThreadLibraryCalls(instance);return TRUE;}
