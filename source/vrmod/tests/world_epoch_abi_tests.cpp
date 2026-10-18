#include <evr/world_epoch_abi.hpp>
#include <windows.h>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace evr::epoch;
int main(int argc,char** argv){unsigned checks=0;try{
 auto check=[&](bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);};
 if(argc!=4)throw std::runtime_error("provider DLL and two independent consumer DLL paths required");
 auto provider=LoadLibraryA(argv[1]),a=LoadLibraryA(argv[2]),b=LoadLibraryA(argv[3]);check(provider&&a&&b&&a!=b,"three separately loaded modules");
 auto A=reinterpret_cast<Call>(GetProcAddress(a,"evrTestEpochCall")),B=reinterpret_cast<Call>(GetProcAddress(b,"evrTestEpochCall"));check(A&&B&&A!=B,"independent consumer entrypoints");
 auto call=[&](Call fn,Request r,Status expected=Status::Ok){Response out;SetLastError(123456);const auto code=fn(&r,&out);check(GetLastError()==123456,"native last error preserved across DLLs");check(code==uint32_t(expected),"expected broker response code");if(code==uint32_t(Status::Ok))check(out.status==code,"response and return code agree");return out;};
 auto registered=call(A,request(Operation::Register));check(registered.provider!=0,"one provider identity");call(B,request(Operation::Register),Status::Duplicate);
 auto create=[&](uint64_t world,uint64_t registry,uint64_t root,uint32_t kind){auto r=request(Operation::Create);r.provider=registered.provider;r.world=world;r.registry=registry;r.root=root;r.kind=kind;return call(A,r);};
 auto begin=[&](Response s){auto r=request(Operation::BeginRootClose,s);r.handle=0;return call(A,r);};
 uint64_t oldSerial=0,oldPin=0;
 for(unsigned cycle=0;cycle<3000;++cycle){
  auto server=create(100,101,102,0),client=create(200,201,202,2),prediction=create(300,201,202,1);
  check(server.serial>oldSerial&&client.serial>server.serial&&prediction.serial>client.serial,"serials monotonic despite all native address reuse");oldSerial=prediction.serial;
  auto q=request(Operation::Resolve);q.world=200;q.kind=2;auto visible=call(B,q);check(visible.serial==client.serial&&visible.provider==client.provider&&admitsNew(visible),"second DLL reads first DLL's generation");
  auto pin1=call(B,request(Operation::Acquire,client)),pin2=call(A,request(Operation::Acquire,client));check(pin1.handle>oldPin&&pin2.handle>pin1.handle,"distinct pin handle for every acquisition");oldPin=pin2.handle;
  call(B,request(Operation::Stop,registered),Status::Busy);
  auto closing=begin(client);auto closedRead=call(B,request(Operation::Read,client));check(closedRead.phase==uint32_t(Phase::Closing)&&!admitsNew(closedRead),"root invalidation visible immediately to another DLL");
  call(B,request(Operation::Acquire,client),Status::Closing);auto predRead=call(B,request(Operation::Read,prediction));check(predRead.phase==uint32_t(Phase::Closing),"prediction shares root invalidation, not a World token");
  auto end=call(A,request(Operation::EndRootClose,closing));check(end.pins==2&&end.phase==uint32_t(Phase::Ended),"native end response reports remaining consumer pins");call(A,request(Operation::EndRootClose,closing),Status::Unknown);
  auto old=call(B,request(Operation::Read,client));check(old.phase==uint32_t(Phase::Ended)&&old.pins==2&&!admitsNew(old),"native end retains both outstanding consumer pins");
  auto replacement=create(200,201,202,2);auto replacementPrediction=create(300,201,202,1);check(replacement.serial!=client.serial,"same address replacement has new native generation");
  auto wrong=request(Operation::Release,pin1);wrong.serial=replacement.serial;call(B,wrong,Status::WrongToken);
  call(B,request(Operation::Release,pin1));call(B,request(Operation::Release,pin1),Status::Unknown);
  auto retained=call(A,request(Operation::Read,client));check(retained.pins==1,"duplicate release cannot consume another callback's pin");
  call(A,request(Operation::Release,pin2));call(B,request(Operation::Read,client),Status::WrongToken);
  auto fresh=call(B,q);check(fresh.serial==replacement.serial&&admitsNew(fresh),"late old release left replacement intact");
  auto closeReplacement=begin(replacement);call(A,request(Operation::EndRootClose,closeReplacement));auto closeServer=begin(server);auto empty=call(A,request(Operation::EndRootClose,closeServer));
  check(empty.worlds==0&&empty.leases==0&&empty.closures==0,"all value slots and consumer handles recycled");oldSerial=replacementPrediction.serial;
 }
 auto alive=create(100,101,102,0);std::atomic<unsigned> bad{0};
 auto worker=[&](Call fn){for(unsigned i=0;i<2000;++i){Response pin;auto q=request(Operation::Acquire,alive);if(fn(&q,&pin)||!pin.handle){++bad;continue;}auto release=request(Operation::Release,pin);Response done;if(fn(&release,&done))++bad;}};
 std::thread t1(worker,A),t2(worker,B);t1.join();t2.join();check(bad==0,"concurrent independent DLL consumers acquire/release safely");
 auto after=call(A,request(Operation::Read,alive));check(after.pins==0&&after.leases==0,"concurrent pins drain completely");
 auto abi=request(Operation::Read,alive);abi.version++;Response out;out.serial=0x1234;check(B(&abi,&out)==uint32_t(Status::AbiMismatch)&&out.serial==0x1234,"ABI mismatch leaves caller buffer untouched");
 check(B(nullptr,&out)==uint32_t(Status::Invalid)&&B(&abi,nullptr)==uint32_t(Status::Invalid),"null ABI arguments rejected");
 auto badRequest=request(Operation::Read,alive);badRequest.reserved=1;call(B,badRequest,Status::Invalid);badRequest=request(Operation::Read,alive);badRequest.provider++;call(B,badRequest,Status::Unavailable);
 auto closing=begin(alive);call(A,request(Operation::EndRootClose,closing));auto stopped=call(A,request(Operation::Stop,registered));check(!stopped.flags&&!stopped.worlds,"provider stops only after complete drain");call(B,request(Operation::Register),Status::Duplicate);
 auto stale=request(Operation::Create,alive);stale.handle=0;call(A,stale,Status::Unavailable);
 std::cout<<"PASS "<<checks<<" shared World epoch ABI checks; 3000 cycles + 4000 concurrent leases across two DLL consumers\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
