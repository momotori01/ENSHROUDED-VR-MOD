#include <windows.h>
#include <evr/actor_admission.hpp>
#include <evr/combat_epoch_client.hpp>
#include <evr/combat_world_binding.hpp>
#include <evr/world_epoch_broker.hpp>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <mutex>
#include <stdexcept>
using namespace evr;
struct ObserverLastError {DWORD value=GetLastError();~ObserverLastError(){SetLastError(value);}};
std::string hex(uint64_t v){return std::to_string(v);}
std::string jsonString(const char* s){return std::string("\"")+s+"\"";}
struct Log {std::vector<std::pair<std::string,std::string>> rows;void event(const char* n,const std::string& v){rows.emplace_back(n,v);}} logger;
Log* logFile=&logger;
namespace combatWorldBridge {std::string fields(const epoch::Response& r){return "\"serial\":"+std::to_string(r.serial);}}
#include "../hooks/combat_epoch_gate.hpp"
namespace g=combatEpochGate;
epoch::Broker<> broker;bool readFault=false;
uint32_t dispatch(const epoch::Request* q,epoch::Response* out){
 if(readFault&&q->operation==uint32_t(epoch::Operation::Read)&&q->kind==2){*out={};out->status=uint32_t(epoch::Status::WrongToken);return out->status;}
 *out=broker.call(*q);return out->status;
}
int main(){unsigned checks=0;try{
 auto ck=[&](bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);};
 auto rejected=[&](auto fn){try{fn();return false;}catch(const std::runtime_error&){return true;}};
 const auto provider=broker.call(epoch::request(epoch::Operation::Register));
 auto create=[&](uint64_t world,uint64_t registry,uint64_t root,uint32_t kind){auto q=epoch::request(epoch::Operation::Create,provider);q.world=world;q.registry=registry;q.root=root;q.kind=kind;return broker.call(q);};
 auto op=[&](epoch::Operation operation,const epoch::Response& r){return broker.call(epoch::request(operation,r));};
 auto server=create(100,101,102,0),input=create(200,201,202,2);
 ck(epoch::admitsNew(create(300,201,202,1)),"client root includes its actual prediction World");
 const auto worlds=motion::combatWorlds(input,server);
 using A=motion::ActorAdmission;
 A cold;ck(!cold.permitsNew(worlds)&&!cold.bind(worlds.contact,11)&&cold.stop(worlds.contact,11,1)==A::Stop::Rejected,"cold owner has no authority");
 ck(!cold.open({})&&cold.open(worlds)&&!cold.open(worlds),"one valid combat binding only");
 ck(cold.stop(worlds.contact,11,1)==A::Stop::Rejected,"destroying an actor without an owned watch cannot close this binding");
 ck(cold.bind(worlds.contact,11)&&cold.bind(worlds.contact,11),"same actor may own sequential attacks");
 ck(!cold.bind(worlds.contact,12)&&!cold.bind(worlds.input,11),"foreign actor/client cannot replace saved owner");
 for(unsigned field=0;field<8;++field){auto foreign=worlds.contact;switch(field){case 0:++foreign.provider;break;case 1:++foreign.serial;break;case 2:++foreign.world;break;case 3:++foreign.registry;break;case 4:++foreign.root;break;case 5:++foreign.slot;break;case 6:foreign.kind=2;break;default:foreign={};}
  ck(cold.stop(foreign,11,99)==A::Stop::Rejected&&cold.permitsNew(worlds),"wrong complete server identity cannot stop owner");
 }
 ck(cold.stop(worlds.contact,12,99)==A::Stop::Rejected&&cold.stop(worlds.contact,11,0)==A::Stop::Rejected,"foreign entity and missing native call refused");
 ck(cold.stop(worlds.contact,11,41)==A::Stop::Stopped&&!cold.permitsNew(worlds)&&!cold.permitsFence(worlds.contact),"actual destruction closes admission while World still alive");
 ck(cold.stop(worlds.contact,11,42)==A::Stop::AlreadyStopped&&cold.ownerCall()==41&&!cold.bind(worlds.contact,11)&&!cold.open(worlds),"later absent deletion does not rebind or rewrite first owner boundary");
 for(uint64_t generation=1;generation<=32;++generation){auto next=worlds;next.contact.serial+=generation;next.input.serial+=generation;A replacement;
  ck(replacement.open(next)&&replacement.bind(next.contact,11),"same entity/addresses get separate new binding");
  ck(replacement.stop(worlds.contact,11,41)==A::Stop::Rejected&&replacement.permitsNew(next)&&!cold.permitsNew(next),"old callback cannot close replacement; old gate never reopens");
 }
 // Execute the production adapter with a real broker; fake only OS-module lookup
 // and logging dependencies. No native ECS addresses are dereferenced here.
 g::requested=true;ck(g::inputState()==motion::CombatEpochState::Unavailable,"uninitialized adapter not mistaken for normal close");
 ck(g::client.open(dispatch,input,server)&&g::actorAdmission.open(g::client.worlds()),"production gate receives two independently owned value leases");g::initialized=true;
 g::bindActor(worlds.contact,11);ck(g::inputState()==motion::CombatEpochState::Open&&g::actorFenceStamp(100)==worlds.contact,"initial input and actor fence open");
 auto key=g::action({100,11,22,99,123,0,0});motion::NativeAttackTicket ticket{11,99,100,200,1,true,true,5,1,1,1};ticket.worlds=worlds;
 motion::EventRetimeLedger<2> ledger;motion::EventIdentity event{1000,{1,2,3}};ck(ledger.commit(key,event)&&g::impact(ticket,key,250)&&g::suppress(key,true),"live native ticket and owned history admitted");
 motion::ColliderPose pose;pose.valid=true;pose.clientWorld=200;g::pose(pose);ck(pose.valid&&pose.worlds==worlds,"pose bound before owner destruction");
 ck(rejected([&]{g::stopActor(worlds.contact,12,5);})&&g::newAllowed(g::Stage::Input),"adapter rejects foreign actor without stopping real owner");
 auto foreign=worlds.contact;++foreign.serial;ck(rejected([&]{g::stopActor(foreign,11,5);}),"adapter requires fresh exact server generation");
 g::stopActor(worlds.contact,11,7);
 ck(epoch::admitsNew(op(epoch::Operation::Read,server))&&epoch::admitsNew(op(epoch::Operation::Read,input)),"Actor closure does not forge native World closure");
 ck(g::actorStopped()&&g::inputState()==motion::CombatEpochState::Closed&&!g::actorFenceStamp(100).valid(),"production input and saved-Actor fence both closed");
 for(auto stage:{g::Stage::Input,g::Stage::Poll,g::Stage::Ticket,g::Stage::Pose,g::Stage::Event,g::Stage::Append,g::Stage::Collider})ck(!g::newAllowed(stage,200,100),"all NEW publication stages closed");
 ck(!g::capture(g::Stage::Input,200,100).valid()&&!g::impact(ticket,key,250),"old valid ticket cannot authorize another native Impact");
 g::pose(pose);ck(!pose.valid,"pose published after Actor stop invalidated");
 motion::ColliderEvidence evidence;evidence.clientWorld=200;evidence.impactWorld=100;ck(std::string(g::poseRejection(pose,evidence))=="actor_owner_stopped","collider rejection identifies actual Actor stop");
 ck(g::serverStamp(100)==worlds.contact&&g::action(key)==key&&g::suppress(key,ledger.owns(key))&&!g::suppress(key,false)&&ledger.size()==1,"stopping Actor retains EXACT consumed history without granting unowned suppression");
 auto newer=key;++newer.epoch.serial;ck(!g::suppress(newer,true),"stopped context cannot suppress replacement generation");
 g::stopActor(worlds.contact,11,8);unsigned stops=0;for(const auto& row:logger.rows)if(row.first=="combat_actor_admission_stopped"){++stops;ck(row.second.find("\"inflight_callbacks_drained\":false")!=std::string::npos,"admission stop never claims an ECS pin or callback drain");}
 ck(stops==1&&g::actorAdmission.ownerCall()==7,"native duplicate stop logged once, first identity retained");
 readFault=true;ck(g::inputState()==motion::CombatEpochState::Unavailable&&g::suppress(key,true),"read failure remains a fault, not a fabricated normal Actor close; owned server history survives");readFault=false;
 auto closing=op(epoch::Operation::BeginRootClose,server);ck(g::suppress(key,true)&&!g::actorFenceStamp(100).valid(),"server Closing still suppresses; no Actor fence resurrection");
 ck(op(epoch::Operation::EndRootClose,closing).status==0&&!g::suppress(key,true),"server Ended removes native suppression permission");
 auto clientClosing=op(epoch::Operation::BeginRootClose,input);ck(clientClosing.status==0&&op(epoch::Operation::EndRootClose,clientClosing).status==0,"both client and prediction World ended normally");g::finish();
 ck(g::client.retained()==0&&op(epoch::Operation::Stats,provider).worlds==0&&ledger.size()==1,"exact leases released; stopping admission never silently retires value ledger");
 std::cout<<"PASS "<<checks<<" Actor admission/core+production gate checks (CPU, not native in-flight exit)\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}}
