#include <Windows.h>
#include <Xinput.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <filesystem>
std::filesystem::path calibrationDirectory;   // hooks/key_script.hpp: the probe's folder (none here)
struct ObserverLastError {DWORD value=GetLastError();~ObserverLastError(){SetLastError(value);}};
template<class T>T read(uintptr_t at){if(!at)throw std::runtime_error("null read");T value;memcpy(&value,reinterpret_cast<void*>(at),sizeof(value));return value;}
std::string hex(uintptr_t v){return std::to_string(v);}std::string jsonString(const char* v){return v;}
struct Log {std::vector<std::string> events;void event(const char* name,const std::string& fields){events.emplace_back(std::string(name)+fields);}} logger;
Log* logFile=&logger;
#include "../hooks/controller_ui_fixture.hpp"
namespace f=controllerUiFixture;
alignas(8) std::array<uint8_t,0x600> frontend{};
alignas(8) std::array<uint8_t,0x100> managerMemory{},backendMemory{};
std::atomic<int> physical{-1};std::atomic<unsigned> nativeRumbles{0};
uint64_t nowMs=100;bool raceInstall=false;
bool pinAllowed=true;HMODULE pinnedResult=HMODULE(1);unsigned modulePins=0;
BOOL WINAPI fakePin(DWORD flags,LPCWSTR address,HMODULE* result){++modulePins;if(flags!=(GET_MODULE_HANDLE_EX_FLAG_PIN|GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)||!address||!pinAllowed)return FALSE;*result=pinnedResult;return TRUE;}
template<class T>void write(uintptr_t at,T v){memcpy(reinterpret_cast<void*>(at),&v,sizeof(v));}
DWORD WINAPI fakeState(DWORD user,XINPUT_STATE* s){SetLastError(123);if(int(user)!=physical)return ERROR_DEVICE_NOT_CONNECTED;if(s){*s={};s->dwPacketNumber=42;s->Gamepad.wButtons=XINPUT_GAMEPAD_Y;}return ERROR_SUCCESS;}
DWORD WINAPI fakeCaps(DWORD user,DWORD,XINPUT_CAPABILITIES* c){SetLastError(124);if(raceInstall&&user==3)write(reinterpret_cast<uintptr_t>(backendMemory.data())+0x20,uintptr_t(0x12345678));if(int(user)!=physical)return ERROR_DEVICE_NOT_CONNECTED;if(c){*c={};c->SubType=9;}return ERROR_SUCCESS;}
DWORD WINAPI fakeRumble(DWORD,XINPUT_VIBRATION*){++nativeRumbles;return 99;}
FARPROC WINAPI resolve(HMODULE,LPCSTR name){if(reinterpret_cast<uintptr_t>(name)==100||std::string(name)=="XInputGetState")return reinterpret_cast<FARPROC>(&fakeState);if(std::string(name)=="XInputSetState")return reinterpret_cast<FARPROC>(&fakeRumble);if(std::string(name)=="XInputGetCapabilities")return reinterpret_cast<FARPROC>(&fakeCaps);return nullptr;}
ULONGLONG WINAPI clockValue(){return nowMs;}
uintptr_t front(){return reinterpret_cast<uintptr_t>(frontend.data());}
uintptr_t back(){return reinterpret_cast<uintptr_t>(backendMemory.data());}
bool __fastcall fakeDeviceEvent(void* manager,const void* event){if(read<uint8_t>(uintptr_t(event))!=20)return false;write(read<uintptr_t>(uintptr_t(manager)+0x48)+0x28,uint8_t(1));return true;}
void consumeConnect(){write(back()+0x28,uint8_t(0));write(back()+0x50,uint8_t(1));f::onScope(front(),true);}
void consumeDisconnect(){write(back()+0x50,uint8_t(0));f::onScope(front(),true);}
void reset(){
    f::backendExitRequested=f::inputModulePinned=f::backendExitTableRestored=f::holdConnectionOnEpochStop=false;f::backendWaitReady=false;f::readersStopped=false;f::backendExitBegun=false;f::backendExitReturned=false;f::pendingBackendExit={};f::ownerSerial=0;f::ownerThread=0;f::pinInputModule=&fakePin;pinAllowed=true;pinnedResult=HMODULE(1);modulePins=0;
    f::epochCloseExpected=false;f::epochCloseObserved=false;f::epochStopped=false;f::epochCoordinatorStopped=false;f::observeEpoch=nullptr;f::meleeSession.reset();
    frontend={};managerMemory={};backendMemory={};logger.events.clear();physical=-1;nativeRumbles=0;nowMs=100;raceInstall=false;
    const auto m=reinterpret_cast<uintptr_t>(managerMemory.data());write(front()+0x4b0,m);write(m+0x48,back());write(back(),m);write(back()+8,HMODULE(1));
    write(back()+0x10,reinterpret_cast<uintptr_t>(&fakeState));write(back()+0x18,reinterpret_cast<uintptr_t>(&fakeRumble));write(back()+0x20,reinterpret_cast<uintptr_t>(&fakeCaps));
    f::wheelRequested=false;f::wheelProgress=evr::motion::QuickWheelProgress{};f::wheelState={};f::wheelObserved=0;
    f::gestureRequested=f::gestureCompleted=f::gestureSawMenu=f::gestureOpenCapture=f::gestureSelectionCapture=f::gestureClosedCapture=false;f::gestureResult={};f::gestureMs=0;
    f::motionAttackRequested=f::motionAttackCompleted=false;f::swing=evr::game::MotionAttackFixture{{2.f,.3f,.6f,2,100000000,12.f,30.f}};f::observeCombat=nullptr;f::handFrame={};f::handMs=f::lastLoggedHand=0;
    f::attackTicket={};f::attackTicketSerial=0;f::attackRequested=f::attackCompleted=false;f::attackButtons=0;f::attack=evr::game::ControllerAttackFixture{};f::attackPacket.~EpochAttackPacket();new(&f::attackPacket) evr::motion::EpochAttackPacket;f::epochAllowsPoll=nullptr;f::epochAllowsTicket=nullptr;f::epochContextFields=nullptr;f::observeAttack=nullptr;
    f::menuRequested=false;f::menu=evr::game::ControllerMenuFixture{};f::tabsRequested=false;f::altarRequested=false;f::motionRequested=false;f::motionCompleted=false;f::doubleJumpRequested=false;f::glideRequested=false;f::observeMotion=nullptr;f::motion=evr::game::ControllerMotionFixture{};f::observeCamera=nullptr;f::lastCamera=-1;f::observedPacket=0;f::packetButtons=1ull<<16;f::emittedTransitions=0;f::menuCompleted=false;f::requested=true;f::active=false;f::connected=false;f::stop=false;f::done=false;f::failed=false;f::installed=false;f::failureReported=false;
    f::callbacks=0;f::states=0;f::capabilities=0;f::rumbles=0;f::disconnects=0;f::begin=f::disconnectAt=f::releaseAt=0;f::trackingLossExpected=f::trackingLossObserved=false;f::offhandReleaseExpected=f::offhandReleaseObserved=false;f::twoHandAttackRequested=false;f::twoHandSupportWeapon={};f::port=0;
    f::resolveExport=&resolve;f::clockNow=&clockValue;f::nativeDeviceEvent=&fakeDeviceEvent;
    f::nativeConnectedSeen=f::nativeDisconnectedSeen=false;f::lastNativeConnection=-1;
    write(back()+0x48,back()+8);
}
bool nativeTable(){return read<uintptr_t>(back()+0x10)==uintptr_t(&fakeState)&&read<uintptr_t>(back()+0x18)==uintptr_t(&fakeRumble)&&read<uintptr_t>(back()+0x20)==uintptr_t(&fakeCaps);}
int main(){try{std::cout<<std::unitbuf;unsigned passed=0,failed=0;auto check=[&](bool yes,const char* name){std::cout<<(yes?"PASS ":"FAIL ")<<name<<'\n';yes?++passed:++failed;};
    for(unsigned flags=0;flags<16;flags++){
        f::trackingLossExpected=bool(flags&1);f::trackingLossObserved=bool(flags&2);f::offhandReleaseExpected=bool(flags&4);f::offhandReleaseObserved=bool(flags&8);
        check(f::expectedMotionCancellation()==((flags&3)==3||(flags&12)==12),"cancellation completion needs a matching explicit policy and observed event");
    }
    reset();f::onScope(front(),false);check(!f::installed&&nativeTable(),"no installation outside validated HUD");
    f::onScope(front(),true);check(f::installed&&f::active&&f::connected&&!f::done,"neutral connection installs all entries");
    consumeConnect();check(f::nativeConnectedSeen&&read<uint8_t>(back()+0x28)==0,"native consumer acknowledges rescan and connection");
    XINPUT_STATE s{};s.Gamepad.wButtons=0xffff;auto result=f::state(0,&s);XINPUT_GAMEPAD zero{};
    check(result==ERROR_SUCCESS&&s.dwPacketNumber==1&&!memcmp(&s.Gamepad,&zero,sizeof(zero))&&GetLastError()==123,"native status replaced with neutral packet; LastError retained");
    XINPUT_CAPABILITIES c{};check(f::caps(0,0,&c)==ERROR_SUCCESS&&c.Type==XINPUT_DEVTYPE_GAMEPAD&&c.SubType==XINPUT_DEVSUBTYPE_GAMEPAD,"controller capabilities are native gamepad shape");
    XINPUT_VIBRATION v{100,200};check(f::rumble(0,&v)==ERROR_SUCCESS&&nativeRumbles==0,"synthetic port never vibrates physical hardware");
    check(f::state(1,&s)==ERROR_DEVICE_NOT_CONNECTED&&f::rumble(1,&v)==ERROR_DEVICE_NOT_CONNECTED&&nativeRumbles==0,"other ports are empty to the game while the mod's pad is in, and no real pad shakes");
    nowMs=2599;f::onScope(front(),true);check(f::connected,"connection survives until specified bound");
    nowMs=2600;f::onScope(front(),true);check(!f::connected&&f::state(0,&s)==ERROR_DEVICE_NOT_CONNECTED&&f::disconnects==1,"disconnect is observable by native consumer");
    consumeDisconnect();nowMs=4100;f::onScope(front(),true);f::drain();check(nativeTable()&&f::done&&!f::installed&&!f::failed&&f::callbacks==0,"all pointers restored and callbacks drained");
    f::onScope(front(),true);check(nativeTable()&&!f::active,"completed fixture never reconnects");
    reset();f::wheelRequested=f::gestureRequested=true;f::onScope(front(),true);consumeConnect();
    for(float x:{-1.f,0.f,1.f})for(float y:{-1.f,0.f,1.f}){
        f::publishWheel(512,x,y);f::state(0,&s);
        check(s.Gamepad.wButtons==512&&s.Gamepad.sThumbRX==SHORT(x*32767)&&s.Gamepad.sThumbRY==SHORT(y*32767)&&!s.Gamepad.sThumbLX&&!s.Gamepad.sThumbLY&&!s.Gamepad.bLeftTrigger&&!s.Gamepad.bRightTrigger,"wheel's two axes reach only the owned VR port");
        auto sequence=s.dwPacketNumber;f::publishWheel(512,x,y);f::state(0,&s);check(s.dwPacketNumber==sequence,"unchanged radial selection does not create a new packet");
    }
    f::stop=true;nowMs+=10;f::onScope(front(),true);check(f::connected&&!f::disconnectAt,"wheel release waits for an actual neutral poll");
    f::state(0,&s);check(!memcmp(&s.Gamepad,&zero,sizeof(zero)),"wheel stop releases grip and both axes");
    nowMs+=10;f::onScope(front(),true);check(!f::connected&&f::disconnectAt,"wheel disconnect follows the consumed neutral packet");
    f::state(0,&s);consumeDisconnect();nowMs+=1500;f::onScope(front(),true);check(nativeTable()&&f::done&&!f::failed,"wheel input table restores on the native owner");
    // A real pad (2026-10-19, a player's logs: an Xbox controller plugged in, no buttons): hidden from the game while the mod's pad is in.
    reset();physical=2;f::onScope(front(),true);
    {bool told=false;for(const auto& e:logger.events)told=told||e.rfind("controller_fixture_physical_pads_hidden",0)==0;
     check(f::installed&&f::active&&!f::failed&&!f::done&&told,"a real pad on another port no longer blocks the mod's pad (it is hidden)");}
    consumeConnect();s.Gamepad.wButtons=0xffff;check(f::state(2,&s)==ERROR_DEVICE_NOT_CONNECTED&&s.Gamepad.wButtons==0&&f::caps(2,0,&c)==ERROR_DEVICE_NOT_CONNECTED,"the real pad's port answers as empty");
    check(f::state(0,&s)==ERROR_SUCCESS&&s.Gamepad.wButtons==0&&s.dwPacketNumber!=42,"the mod's port is the mod's pad beside a real one");
    reset();f::onScope(front(),true);consumeConnect();physical=0;
    check(f::state(0,&s)==ERROR_SUCCESS&&s.dwPacketNumber!=42&&s.Gamepad.wButtons==0&&f::active&&!f::failed&&f::physicalHidden>0,"a real pad arriving on the mod's port stays hidden; the mod's pad goes on");
    f::onScope(front(),true);check(!nativeTable()&&!f::done&&f::installed,"a real pad's arrival keeps the mod's table");
    nowMs=2600;f::onScope(front(),true);check(!f::connected&&f::state(0,&s)==ERROR_DEVICE_NOT_CONNECTED,"leaving: the mod's pad goes although a real pad sits on its port");
    consumeDisconnect();nowMs=4100;f::onScope(front(),true);f::drain();check(nativeTable()&&f::done&&!f::failed,"after leaving the native table is back (the real pad is the game's again)");
    reset();write(back()+0x10,uintptr_t(0x123));f::onScope(front(),true);check(f::failed&&f::done&&read<uintptr_t>(back()+0x10)==0x123,"unknown backend export is never called or overwritten");
    reset();raceInstall=true;f::onScope(front(),true);check(f::failed&&f::done&&!f::installed&&read<uintptr_t>(back()+0x10)==uintptr_t(&fakeState)&&read<uintptr_t>(back()+0x18)==uintptr_t(&fakeRumble)&&read<uintptr_t>(back()+0x20)==0x12345678,"partial install rolls back ours and preserves foreign entry");
    reset();f::onScope(front(),true);write(back()+0x20,uintptr_t(0x333));f::restore();check(f::done&&f::failed&&read<uintptr_t>(back()+0x20)==0x333&&read<uintptr_t>(back()+0x10)==uintptr_t(&fakeState),"restore conflict cannot overwrite new owner entry");
    reset();f::onScope(front(),true);write(back()+0x28,uint8_t(0));write(back()+0x10,uintptr_t(&fakeState));f::restore();check(nativeTable()&&!f::failed,"already restored entry is accepted idempotently");
    reset();f::onScope(front(),true);write(reinterpret_cast<uintptr_t>(managerMemory.data())+0x48,uintptr_t(0));f::onScope(front(),true);const auto logs=logger.events.size();f::onScope(front(),true);check(!f::active&&f::failed&&!f::done&&logger.events.size()==logs,"owner loss stops synthesis without stale owner writes or log flooding");
    write(reinterpret_cast<uintptr_t>(managerMemory.data())+0x48,back());f::onScope(front(),true);check(nativeTable()&&f::done,"same verified owner returning permits guarded restoration");
    reset();f::onScope(front(),true);f::restore();bool refused=false;try{f::drain();}catch(...){refused=true;}check(refused&&nativeTable(),"unpolled fixture is a failure despite successful restoration");
    reset();f::onScope(front(),true);std::atomic<bool> running{true};std::thread poll([&]{while(running){XINPUT_STATE out{};f::state(0,&out);}});
    std::this_thread::sleep_for(std::chrono::milliseconds(10));f::restore();running=false;poll.join();check(nativeTable()&&f::callbacks==0&&!f::active,"in-flight API callbacks quiesce across restoration");
    reset();f::attackRequested=true;f::observeAttack=[](){return evr::game::AttackObservation{1,100,true,false,0};};
    f::onScope(front(),true);consumeConnect();f::state(0,&s);nowMs=600;f::onScope(front(),true);f::state(0,&s);
    auto wanted=zero;wanted.bRightTrigger=255;
    check(s.dwPacketNumber==2&&!memcmp(&s.Gamepad,&wanted,sizeof(wanted)),"attack fixture emits only one RT payload with fresh packet");
    nowMs=720;f::onScope(front(),true);f::state(0,&s);check(s.dwPacketNumber==3&&!memcmp(&s.Gamepad,&zero,sizeof(zero)),"attack fixture releases RT at bounded duration");
    f::observeAttack=[](){return evr::game::AttackObservation{1,100,true,false,12};};nowMs=2220;f::onScope(front(),true);
    check(f::attackCompleted&&!f::connected,"native active evidence then idle completes one attack fixture");
    f::state(0,&s);consumeDisconnect();nowMs=3720;f::onScope(front(),true);f::drain();check(nativeTable()&&!f::failed,"attack table cleanup and disconnected native status restored");
    for(auto button:{XINPUT_GAMEPAD_RIGHT_SHOULDER,XINPUT_GAMEPAD_X}){
    reset();f::attackRequested=true;f::attackButtons=uint16_t(button);f::observeAttack=[](){return evr::game::AttackObservation{1,100,true,false,0};};
    f::onScope(front(),true);consumeConnect();f::state(0,&s);nowMs=600;f::onScope(front(),true);f::state(0,&s);
    wanted=zero;wanted.wButtons=uint16_t(button);
    check(s.dwPacketNumber==2&&!memcmp(&s.Gamepad,&wanted,sizeof(wanted)),"button fixture emits only selected button; no trigger or axes");
    nowMs=720;f::onScope(front(),true);f::state(0,&s);check(s.dwPacketNumber==3&&!memcmp(&s.Gamepad,&zero,sizeof(zero)),"button fixture releases all input at bounded duration");
    f::observeAttack=[](){return evr::game::AttackObservation{1,100,true,false,12};};nowMs=2220;f::onScope(front(),true);
    f::state(0,&s);consumeDisconnect();nowMs=3720;f::onScope(front(),true);f::drain();check(nativeTable()&&!f::failed,"button fixture restores original table after native evidence");
    }
    // Full frontend wrapper path, with independent XR wrist samples and native
    // action observations. It must synthesize only X and restore all entries.
    reset();f::attackRequested=true;f::attackButtons=XINPUT_GAMEPAD_X;f::observeAttack=[](){return evr::game::AttackObservation{1,100,true,false,0};};
    f::onScope(front(),true);consumeConnect();f::state(0,&s);nowMs=600;f::onScope(front(),true);f::state(0,&s);
    check(s.Gamepad.wButtons==XINPUT_GAMEPAD_X,"precondition: native consumer holds attack");
    f::stop=true;nowMs=610;f::onScope(front(),true);check(f::connected&&!f::disconnectAt&&uint8_t(f::attackPacket.load())==0,"abort waits for one neutral native poll");
    check(f::state(0,&s)==ERROR_SUCCESS&&!memcmp(&s.Gamepad,&zero,sizeof(zero)),"aborted input releases while still connected");
    nowMs=620;f::onScope(front(),true);check(!f::connected&&f::disconnectAt==nowMs,"only consumed neutral packet allows disconnect");
    f::state(0,&s);consumeDisconnect();nowMs=2120;f::onScope(front(),true);check(nativeTable()&&f::done&&!f::failed,"aborted input backend restored after release");
    reset();f::motionAttackRequested=true;f::attackButtons=XINPUT_GAMEPAD_X;
    f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};
    evr::motion::HandFrame hand;hand.focused=true;hand.sessionGeneration=hand.originGeneration=1;
    auto& right=hand.hands[1];right.gripActive=right.grabActive=right.grab=true;right.gripFlags=15;right.profile=1;
    auto feed=[&](float dx=0){nowMs+=10;hand.frame++;hand.predictedTime+=10000000;right.grip.position.x+=dx;f::publishHands(hand,nowMs);f::onScope(front(),true);};
    feed();consumeConnect();for(int i=0;i<60;i++)feed();f::state(0,&s);
    check(!s.Gamepad.wButtons&&f::swing.phase()==evr::game::MotionAttackFixture::Phase::WaitSwing,"motion wrapper waits for a real gesture, not an elapsed timer");
    feed(.04f);f::state(0,&s);wanted=zero;wanted.wButtons=XINPUT_GAMEPAD_X;
    check(!memcmp(&s.Gamepad,&wanted,sizeof(wanted)),"XR wrist swing emits only native X input");
    f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=o.usedItem=100;o.binding=101;o.valid=o.alive=o.ready=o.active=true;o.sequence=10;o.triggerTime=100;o.length=1000;o.currentTime=500;return o;};
    for(int i=0;i<20;i++)feed();f::state(0,&s);check(!memcmp(&s.Gamepad,&zero,sizeof(zero)),"motion pulse releases all native buttons after bounded press");
    f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};
    for(int i=0;i<30;i++)feed();check(f::motionAttackCompleted&&!f::connected,"acknowledged native motion action finishes and disconnects");
    f::state(0,&s);consumeDisconnect();nowMs+=1500;f::onScope(front(),true);f::drain();check(nativeTable()&&!f::failed,"motion wrapper returns native table, connection and callbacks");
    // Genuine offhand grab release keeps both poses valid, revokes the ticket,
    // drains neutral input, and does not pretend that tracking was lost.
    for(bool accepted:{false,true})for(bool expected:{false,true}){
        reset();f::motionAttackRequested=f::twoHandAttackRequested=true;f::offhandReleaseExpected=expected;f::attackButtons=XINPUT_GAMEPAD_X;
        evr::motion::Weapon w;w.twoHanded=true;w.gripToModel.position={0,0,.63f};w.offHandTolerance=.035f;f::swing.setTwoHandWeapon(w,100);w.binding=101;f::twoHandSupportWeapon=w;
        f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};
        hand={};hand.focused=true;hand.sessionGeneration=hand.originGeneration=1;
        for(auto& h:hand.hands){h.gripActive=h.grabActive=h.grab=true;h.gripFlags=15;h.profile=1;}
        hand.hands[0].grip.position.z=.63f;
        feed();consumeConnect();for(unsigned i=0;i<60;i++)feed();f::state(0,&s);
        hand.hands[0].grip.position.x+=.04f;feed(.04f);f::state(0,&s);
        check(s.Gamepad.wButtons==XINPUT_GAMEPAD_X&&f::ticketSnapshot().valid,"two-hand qualified native request before support release");
        if(accepted){f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=o.usedItem=100;o.binding=101;o.valid=o.alive=o.ready=o.active=true;o.sequence=10;o.triggerTime=100;o.length=1000;o.currentTime=50;return o;};feed();}
        hand.hands[0].grab=false;feed();
        check(f::swing.phase()==evr::game::MotionAttackFixture::Phase::Aborted&&f::ticketSnapshot().revoked&&!f::trackingLossObserved&&f::offhandReleaseObserved==expected&&f::failed.load()==!expected,"offhand release distinct from tracking loss; unplanned abort still fails diagnostic");
        f::state(0,&s);check(!memcmp(&s.Gamepad,&zero,sizeof(zero)),"offhand abort releases all native input");
        hand.hands[0].grab=true;feed();f::state(0,&s);consumeDisconnect();nowMs+=1500;f::onScope(front(),true);
        check(nativeTable()&&f::done&&!f::connected&&f::ticketSnapshot().revoked,"regrip cannot revive same request; native backend restored");
        if(expected){f::drain();check(!f::failed,"expected offhand release reaches clean diagnostic teardown");}
    }
    reset();f::attackRequested=true;f::attackButtons=XINPUT_GAMEPAD_X;
    f::observeAttack=[](){return evr::game::AttackObservation{1,100,true,false,0};};
    f::onScope(front(),true);consumeConnect();f::publishAttack(255);f::attackTicket.valid=true;f::state(0,&s);
    const auto heldPacket=s.dwPacketNumber;check(s.Gamepad.wButtons==XINPUT_GAMEPAD_X,"epoch poll test begins with actually held native input");
    f::epochAllowsPoll=[](){return false;};f::state(0,&s);
    check(s.dwPacketNumber>heldPacket&&!s.Gamepad.wButtons&&f::ticketSnapshot().revoked,"epoch rejection returns new neutral packet and revokes ticket");
    f::epochAllowsPoll=[](){return true;};f::publishAttack(255);f::state(0,&s);
    check(!s.Gamepad.wButtons&&!uint8_t(f::attackPacket.load()),"late worker publication cannot reactivate rejected epoch");
    // World stop must latch even when the last attack packet is already neutral.
    for(bool held:{false,true})for(bool expected:{false,true})for(bool unavailable:{false,true}){
        reset();f::motionAttackRequested=true;f::epochCloseExpected=expected;f::attackButtons=XINPUT_GAMEPAD_X;
        f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};
        f::handFrame.focused=true;f::handFrame.frame=1;f::handFrame.predictedTime=10000000;f::handFrame.sessionGeneration=f::handFrame.originGeneration=1;f::handMs=nowMs;
        for(auto& h:f::handFrame.hands){h.profile=1;h.gripFlags=15;h.gripActive=h.grabActive=h.grab=true;}
        f::onScope(front(),true);consumeConnect();f::publishAttack(held?255:0);f::attackTicket.valid=true;f::handFeedback.amplitude={.5f,.5f};f::state(0,&s);
        f::observeEpoch=unavailable?+[](){return evr::motion::CombatEpochState::Unavailable;}:+[](){return evr::motion::CombatEpochState::Closed;};
        f::state(0,&s);
        check(!s.Gamepad.wButtons&&f::epochStopped&&f::ticketSnapshot().revoked&&!f::handFeedback.amplitude[0]&&!f::handFeedback.amplitude[1],"World stop cancels neutral/held transport, ticket and both haptics");
        check(f::epochCloseObserved.load()==(expected&&!unavailable)&&f::failed.load()==(!expected||unavailable),"unavailable broker is never counted as expected World closure");
        f::observeEpoch=[](){return evr::motion::CombatEpochState::Open;};f::publishAttack(255);f::state(0,&s);
        check(!s.Gamepad.wButtons,"cached worker or recovered broker cannot revive stopped transport");
        nowMs+=10;f::onScope(front(),true);f::state(0,&s);consumeDisconnect();nowMs+=1500;f::onScope(front(),true);
        check(nativeTable()&&f::done&&f::nativeDisconnectedSeen,"World admission stop preserves native neutral/disconnect/restoration path");
        if(expected&&!unavailable){f::drain();check(!f::failed,"expected admission cancellation is fully drained");}
    }
    for(bool held:{false,true}){
        reset();f::motionAttackRequested=true;f::epochCloseExpected=true;f::attackButtons=XINPUT_GAMEPAD_X;
        f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};
        f::handFrame.focused=true;f::handFrame.frame=1;f::handFrame.predictedTime=10000000;f::handFrame.sessionGeneration=f::handFrame.originGeneration=1;f::handMs=nowMs;
        for(auto& h:f::handFrame.hands){h.profile=1;h.gripFlags=15;h.gripActive=h.grabActive=h.grab=true;}
        f::observeEpoch=[](){return evr::motion::CombatEpochState::Open;};
        check(!f::serviceStoppedFrontend(0)&&!f::installed,"open cold frontend service neither reads owner nor installs synthetic input");
        f::onScope(front(),true);consumeConnect();f::publishAttack(held?255:0);f::state(0,&s);
        f::attackTicket.valid=true;
        f::observeEpoch=[](){return evr::motion::CombatEpochState::Closed;};
        f::observeCombat=[]()->evr::game::MotionCombatObservation{throw std::runtime_error("Actor is no longer readable");};
        check(f::serviceStoppedFrontend(front())&&f::epochStopped&&f::epochCoordinatorStopped&&!f::ticketSnapshot().valid&&!f::failed,"frontend epoch close drains without reading vanished Actor/UI");
        if(held)check(f::connected&&!f::disconnectAt,"closed frontend still requires actual neutral poll");
        f::state(0,&s);check(!s.Gamepad.wButtons,"owned backend supplies real neutral after UI disappeared");
        nowMs+=10;f::serviceStoppedFrontend(front());f::state(0,&s);
        check(f::disconnectAt&&!f::connected&&f::disconnects,"real disconnect is observed while HUD unavailable");
        write(back()+0x50,uint8_t(0));nowMs+=1500;f::serviceStoppedFrontend(front());f::drain();
        check(nativeTable()&&f::done&&f::nativeDisconnectedSeen&&!f::failed,"backend restored without an Actor or UI update");
        check(f::serviceStoppedFrontend(0),"finished closed epoch service reads no vanished frontend memory");
    }
    reset();f::motionAttackRequested=true;f::epochCloseExpected=true;f::observeEpoch=[](){return evr::motion::CombatEpochState::Closed;};
    check(f::serviceStoppedFrontend(0)&&f::done&&!f::installed&&nativeTable(),"epoch closes before installation without any owner write");
    // A proven normal close survives release of its value broker leases.
    reset();f::motionAttackRequested=true;f::epochCloseExpected=true;f::attackButtons=XINPUT_GAMEPAD_X;
    f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};f::onScope(front(),true);
    f::observeEpoch=[](){return evr::motion::CombatEpochState::Closed;};f::state(0,&s);
    f::observeEpoch=[]()->evr::motion::CombatEpochState{throw std::runtime_error("released World lease must not be queried");};
    f::publishAttack(255);f::state(0,&s);
    check(f::epochStopped&&f::epochCloseObserved&&!f::failed&&!s.Gamepad.wButtons,"known close remains neutral after broker release without resampling it");
    reset();bool readerCold=false;try{f::stopGameplayReaders();}catch(...){readerCold=true;}
    check(readerCold&&!f::readersStopped&&!f::stop,"reader handoff cannot start without installed/pinned owner");
    // An unpolled held attack may be neutralized by the worker, but only the
    // actual owner callback can restore/free the backend. No fake drain proof.
    for(bool held:{false,true}){
        reset();f::backendExitRequested=true;f::motionAttackRequested=true;f::attackButtons=XINPUT_GAMEPAD_X;
        f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};f::onScope(front(),true);f::state(0,&s);
        write(back()+0x28,uint8_t(0));write(back()+0x50,uint8_t(1));f::backendWaitReady=true;
        f::publishAttack(held?255:0);f::state(0,&s);const auto polled=f::observedPacket.load();
        f::attackTicket.valid=true;f::handFeedback.amplitude={.5f,.7f};
        f::stopGameplayReaders();const auto stopped=f::attackPacket.load();
        check(f::readersStopped&&f::stop&&f::active&&f::connected&&f::installed&&!f::done&&!f::backendExitBegun,"reader stop preserves native owner pending its genuine destructor");
        check(!uint8_t(stopped)&&f::ticketSnapshot().revoked&&!f::handFeedback.amplitude[0]&&!f::handFeedback.amplitude[1]&&f::observedPacket==polled,"worker stop revokes held request and haptics without fabricating neutral consumption");
        f::observeEpoch=[]()->evr::motion::CombatEpochState{throw std::runtime_error("World gone");};
        f::observeCombat=[]()->evr::game::MotionCombatObservation{throw std::runtime_error("Actor gone");};
        f::onScope(0,true);check(f::serviceStoppedFrontend(0)&&!f::failed,"stopped reader never dereferences missing frontend or calls gameplay observers");
        f::publishAttack(255);f::state(0,&s);check(!s.Gamepad.wButtons&&f::observedPacket==stopped,"real native polling may still consume the immutable neutral release");
        const auto logs=logger.events.size();f::stopGameplayReaders();check(f::attackPacket.load()==stopped&&logger.events.size()==logs,"reader stop is idempotent and does not invent another input packet");
        const auto token=f::beginBackendExit(back(),0xf99fd4);check(token.valid()&&f::finishBackendExit(token)&&nativeTable(),"genuine owner may finish a reader-stopped motion input");f::drain();
        check(!f::failed&&f::done&&!f::disconnects&&!f::nativeDisconnectedSeen,"owner completion stays distinct from ordinary polled disconnect");
    }
    reset();f::backendExitRequested=true;f::onScope(front(),true);f::state(0,&s);consumeConnect();f::gestureRequested=true;
    bool unsupportedReader=false;try{f::stopGameplayReaders();}catch(...){unsupportedReader=true;}
    check(unsupportedReader&&!f::readersStopped,"unjoined menu/gesture mode does not silently use motion-only handoff");
    for(bool held:{false,true}){
        reset();f::backendExitRequested=f::holdConnectionOnEpochStop=true;f::motionAttackRequested=true;f::epochCloseExpected=true;f::attackButtons=XINPUT_GAMEPAD_X;
        f::observeCombat=[](){evr::game::MotionCombatObservation o;o.entity=1;o.item=100;o.binding=101;o.valid=o.alive=o.ready=true;return o;};
        f::onScope(front(),true);f::publishAttack(held?255:0);f::state(0,&s);write(back()+0x28,uint8_t(0));write(back()+0x50,uint8_t(1));
        f::observeEpoch=[](){return evr::motion::CombatEpochState::Closed;};
        check(f::serviceStoppedFrontend(front())&&f::epochStopped&&f::backendWaitReady&&f::epochCoordinatorStopped,"explicit owner trial observes real epoch close before retaining neutral connection");
        f::observeEpoch=[]()->evr::motion::CombatEpochState{throw std::runtime_error("released metadata");};
        f::observeCombat=[]()->evr::game::MotionCombatObservation{throw std::runtime_error("destroyed Actor");};
        nowMs+=10000;f::serviceStoppedFrontend(front());
        check(f::connected&&f::installed&&!f::done&&!f::disconnectAt&&!f::failed&&!uint8_t(f::attackPacket.load()),"explicit terminal hold neither times out to stale restore nor resumes Actor reads");
        f::state(0,&s);check(!s.Gamepad.wButtons&&f::observedPacket==f::attackPacket.load(),"held terminal connection still delivers real neutral poll");
        f::stopGameplayReaders();const auto heldOwner=f::beginBackendExit(back(),0xf99fd4);
        check(f::finishBackendExit(heldOwner)&&nativeTable()&&!f::disconnects,"owner returns through independent proof after completed-action hold");f::drain();
    }
    // Native-owner handoff is a separate completion from neutral polling.
    reset();f::backendExitRequested=true;f::onScope(front(),true);consumeConnect();
    f::publishAttack(255);f::attackTicket.valid=true;
    const auto beforePacket=f::observedPacket.load();
    const auto ownerExit=f::beginBackendExit(back(),0xf99fd4);
    check(ownerExit.valid()&&nativeTable()&&!f::active&&!f::connected&&!f::installed&&!f::done&&f::inputModulePinned&&modulePins==1,"owner entry restores exact API table and pins DLL before native destruction");
    check(read<uint8_t>(back()+0x50)==1&&f::disconnects==0&&!f::nativeDisconnectedSeen&&f::observedPacket.load()==beforePacket,"owner handoff does not fake a neutral poll or write native connection status");
    f::publishAttack(255);check(uint8_t(f::attackPacket.load())==0&&!f::ticketSnapshot().valid&&f::ticketSnapshot().revoked,"owner stop latches input and revokes active ticket");
    check(f::finishBackendExit(ownerExit)&&f::done&&f::backendExitReturned&&!f::failed,"owner completion follows matching original return without a poll requirement");
    check(!f::finishBackendExit(ownerExit),"duplicate owner return cannot finish twice");
    f::drain();check(!f::failed&&f::disconnects==0,"owner drain has its own proof and never fabricates disconnected-poll counts");
    reset();f::backendExitRequested=true;f::onScope(front(),true);consumeConnect();
    check(!f::beginBackendExit(0,0xf99fd4).valid()&&!f::backendExitBegun&&f::installed,"foreign owner argument is rejected without reading its memory");
    bool wrongCaller=false;try{f::beginBackendExit(back(),0);}catch(...){wrongCaller=true;}
    check(wrongCaller&&!f::backendExitBegun&&f::installed,"unverified native caller cannot start handoff");
    bool wrongThread=false;std::thread foreignThread([&]{try{f::beginBackendExit(back(),0xf99fd4);}catch(...){wrongThread=true;}});foreignThread.join();
    check(wrongThread&&!f::backendExitBegun,"handoff must run on the original frontend owner thread");
    write(back()+8,HMODULE(2));bool changedOwner=false;try{f::beginBackendExit(back(),0xf99fd4);}catch(...){changedOwner=true;}
    check(changedOwner&&!f::backendExitBegun&&f::installed,"different module/owner identity refuses table mutation");
    reset();f::backendExitRequested=true;pinAllowed=false;f::onScope(front(),true);
    check(f::failed&&!f::installed&&nativeTable()&&!f::inputModulePinned,"failed module pin prevents wrapper installation");
    reset();f::backendExitRequested=true;pinnedResult=HMODULE(2);f::onScope(front(),true);
    check(f::failed&&!f::installed&&nativeTable(),"pinning a different module cannot authorize the owner path");
    reset();f::backendExitRequested=true;f::onScope(front(),true);write(back()+0x20,uintptr_t(0x333));
    auto conflictExit=f::beginBackendExit(back(),0xf99fd4);
    check(f::failed&&read<uintptr_t>(back()+0x20)==0x333&&read<uintptr_t>(back()+0x10)==uintptr_t(&fakeState),"handoff restores only our entries and retains foreign replacement as failure");
    check(!f::finishBackendExit(conflictExit)&&f::done,"native return cannot hide a table ownership conflict");
    for(unsigned field=0;field<5;++field){
        reset();f::backendExitRequested=true;f::onScope(front(),true);auto token=f::beginBackendExit(back(),0xf99fd4);auto bad=token;
        switch(field){case 0:++bad.serial;break;case 1:++bad.backend;break;case 2:++bad.manager;break;case 3:++bad.module;break;case 4:++bad.thread;break;}
        check(!f::finishBackendExit(bad)&&!f::done&&f::finishBackendExit(token),"return token requires exact serial/backend/manager/module/thread");
    }
    reset();f::backendExitRequested=true;f::onScope(front(),true);const auto old=f::beginBackendExit(back(),0xf99fd4);f::finishBackendExit(old);
    reset();f::backendExitRequested=true;f::onScope(front(),true);const auto fresh=f::beginBackendExit(back(),0xf99fd4);
    check(old.backend==fresh.backend&&old.serial!=fresh.serial&&!f::finishBackendExit(old)&&!f::done&&f::finishBackendExit(fresh),"same backend address reused by a new installation rejects the old completion");
    reset();f::backendExitRequested=true;f::onScope(front(),true);auto pending=f::beginBackendExit(back(),0xf99fd4);f::callbacks=1;
    check(!f::finishBackendExit(pending)&&f::failed,"outstanding wrapper callback cannot produce a successful owner completion");f::callbacks=0;
    reset();f::backendExitRequested=true;f::onScope(front(),true);f::state(0,&s);const auto neutralToken=f::beginBackendExit(back(),0xf99fd4);
    check(logger.events.back().find("\"packet_kind\":buttons")!=std::string::npos&&logger.events.back().find("\"neutral_was_polled\":true")!=std::string::npos,"neutral button packet audit uses its actual 16-bit packed representation");f::finishBackendExit(neutralToken);
    reset();f::backendExitRequested=true;f::attackRequested=true;f::observeAttack=[](){return evr::game::AttackObservation{1,100,true,false,0};};f::onScope(front(),true);f::publishAttack(255);f::state(0,&s);const auto attackToken=f::beginBackendExit(back(),0xf99fd4);
    check(logger.events.back().find("\"packet_kind\":attack")!=std::string::npos&&logger.events.back().find("\"neutral_was_polled\":false")!=std::string::npos&&!uint8_t(f::attackPacket.load()),"unpolled attack release is never confused with neutral buttons or claimed consumed");f::finishBackendExit(attackToken);
    // Protect actual owner pages after a simulated native destructor. Any stale
    // read in finish, restore, onScope or drain would fault this test process.
    reset();f::backendExitRequested=true;
    const auto dynamicBackend=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    const auto dynamicManager=reinterpret_cast<uintptr_t>(VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE));
    if(!dynamicBackend||!dynamicManager)throw std::runtime_error("protected owner test allocation failed");
    memcpy(reinterpret_cast<void*>(dynamicBackend),backendMemory.data(),backendMemory.size());
    write(front()+0x4b0,dynamicManager);write(dynamicManager+0x48,dynamicBackend);write(dynamicBackend,dynamicManager);write(dynamicBackend+0x48,dynamicBackend+8);
    f::onScope(front(),true);write(dynamicBackend+0x50,uint8_t(1));
    f::backendWaitReady=true;DWORD oldProtect=0;
    if(!VirtualProtect(reinterpret_cast<void*>(dynamicBackend),4096,PAGE_NOACCESS,&oldProtect)||!VirtualProtect(reinterpret_cast<void*>(dynamicManager),4096,PAGE_NOACCESS,&oldProtect))throw std::runtime_error("protect worker-inaccessible owners failed");
    f::stopGameplayReaders();f::onScope(0,false);check(f::serviceStoppedFrontend(0)&&!f::done&&f::installed,"worker reader stop does not read PAGE_NOACCESS backend or manager and cannot claim completion");
    if(!VirtualProtect(reinterpret_cast<void*>(dynamicBackend),4096,PAGE_READWRITE,&oldProtect)||!VirtualProtect(reinterpret_cast<void*>(dynamicManager),4096,PAGE_READWRITE,&oldProtect))throw std::runtime_error("restore native owner entry pages failed");
    const auto gone=f::beginBackendExit(dynamicBackend,0xf99fd4);
    if(!VirtualProtect(reinterpret_cast<void*>(dynamicBackend),4096,PAGE_NOACCESS,&oldProtect)||!VirtualProtect(reinterpret_cast<void*>(dynamicManager),4096,PAGE_NOACCESS,&oldProtect))throw std::runtime_error("protect destroyed owners failed");
    check(f::finishBackendExit(gone),"original-return completion never reads PAGE_NOACCESS destroyed owners");
    f::restore();f::onScope(0,false);check(f::serviceStoppedFrontend(0),"late frontend/restore skip all destroyed owner reads");f::drain();
    VirtualFree(reinterpret_cast<void*>(dynamicBackend),0,MEM_RELEASE);VirtualFree(reinterpret_cast<void*>(dynamicManager),0,MEM_RELEASE);
    reset();f::gestureRequested=true;f::onScope(front(),true);f::state(0,&s);consumeConnect();
    f::gestureResult.frame=450;f::gestureResult.pad.buttons=16;f::gestureMs=nowMs;f::onScope(front(),true);f::state(0,&s);
    check(s.Gamepad.wButtons==16,"gesture mailbox reaches native Start without other axes/buttons");
    nowMs+=10;f::onScope(front(),false,9,0,true);
    check(f::failed&&f::active&&f::connected&&uint16_t(f::packetButtons.load())==0,"unknown UI failure neutralizes gesture while retaining drain");
    nowMs+=10;f::onScope(front(),false,9,0,true);check(f::connected,"failed gesture waits for actual neutral poll");
    f::state(0,&s);check(!memcmp(&s.Gamepad,&zero,sizeof(zero)),"failed gesture releases native Start");
    nowMs+=10;f::onScope(front(),false,9,0,true);check(!f::connected&&f::disconnectAt,"failed gesture disconnects only after release");
    f::state(0,&s);consumeDisconnect();nowMs+=1500;f::onScope(front(),true);
    check(nativeTable()&&f::done&&f::failed&&f::nativeDisconnectedSeen,"failed diagnostic still restores table and native disconnect, without claiming success");
    reset();
    for(unsigned fault=0;fault<6;fault++){
        evr::game::ControllerAttackFixture a;evr::game::AttackObservation o{1,100,true,false,0};a.update(100,true,1,0,o);
        uint8_t screen=1;bool connected=true;uint64_t now=600;
        if(fault==0)o.valid=false;if(fault==1)o.item=0;if(fault==2)o.entity=2;if(fault==3)screen=4;if(fault==4)connected=false;if(fault==5)now=99;
        auto c=a.update(now,connected,screen,0,o);check(c.phase==evr::game::ControllerAttackFixture::Phase::Aborted&&!c.pulseLevel,"attack fixture rejects stale/item/entity/UI/connection/time faults without input");
    }
    {evr::game::ControllerAttackFixture a;evr::game::AttackObservation o{1,100,true,false,0};a.update(100,true,1,0,o);a.update(600,true,1,0,o);a.update(720,true,1,0,o);
     auto c=a.update(8200,true,1,0,o);check(c.phase==evr::game::ControllerAttackFixture::Phase::Aborted&&!c.pulseLevel,"missing native attack evidence times out without retrying attack");}
    std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}catch(const std::exception& e){std::cerr<<"FAIL exception: "<<e.what()<<"\n";return 1;}}
