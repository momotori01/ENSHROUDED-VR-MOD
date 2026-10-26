// Read-only native frontend input predicate observer. Copies values only;
// the reflective UI singleton is not valid outside UI evaluation scopes.
// 1.0 (2026-10-16): the predicate ("input is blocked by the UI") is no longer a function of the frontend object
// (0x7a9930 in the June build, which the mod hooked); the frontend's update computes it inline (0x845430) and stores it
// through a setter, set(subobject = [frontend+0x5a0], value) at 0x9009d0, called at 0x845baa; the other former callers
// read it back through a getter (0x8fb110). The observer hooks the setter through hooks/input_ui_stub.asm, which hands
// the caller's rsi (the frontend object) over as a third argument. The structure offsets moved: state frontend+0x52888
// -> +0x35280, controller state+0x180 -> +0x1b8, world controller+0x1c0 -> +0x1d0, the screen bytes controller+0x36b8 ->
// +0x1ec8, local device controller+0xa8 -> +0xb0, sender context controller+0x3439c0 -> +0x47d4e0 (loaded for the input
// builder at 0x85841e and 0x86e6ce), entity sender+0x5c4 -> +0x64c, ui frontend+0x490 -> +0x5b0, device frontend+0x4d0 ->
// +0x5f8 (all from the new build's disassembly of the same code; tools/match_functions.py --disp).
#include "input_gamepad_probe.hpp"
#include "controller_ui_fixture.hpp"
#include "ui_memory_scan.hpp"
#include "floating_stamina.hpp"
#include "game_hud_visibility.hpp"
#include "memory_string_scan.hpp"
#include "ui_cursor_config.hpp"
#include "game_ammo_list.hpp"
extern "C" void evr_input_ui_stub();
namespace inputUi {
using Set=void(__fastcall*)(void*,uint8_t);
inline Set original=nullptr;
inline std::atomic<bool> enabled{false};
inline std::atomic<bool> accepting{false};
inline std::atomic<unsigned> callbacks{0},calls{0},failures{0};
constexpr uintptr_t rva=0x902280,callSite=0x84741a,callReturn=0x84741f;
constexpr const char* pattern="48 8B C4 48 83 EC 68 4C 8B 99 30 03 00 00 88 91 A9 20 02 00";
constexpr uintptr_t stateOffset=0x35280,controllerOffset=0x1b8,worldOffset=0x1d0,screenOffset=0x1ec8,localDeviceOffset=0xb0,senderContextOffset=0x47d4e0,entityOffset=0x64c,uiOffset=0x5b0,deviceOffset=0x5f8;
struct Snapshot {
    unsigned number=0;uint64_t milliseconds=0;uintptr_t data=0,world=0,senderContext=0;
    uint32_t entity=0;
    uint8_t screen=255,secondary=255,auxiliary=255,deviceMode=255,localMode=255,uiMode=255;bool readable=false,stable=false,nativeResult=true;
    // The bytes around the three modes (2026-10-21: which of them makes the prompts the keyboard's - the device mode and the
    // local one were back at the pad's 0.5 s before a picture that still showed the keyboard's, Mock game-20261021-142914).
    std::array<uint8_t,16> deviceBytes{},localBytes{},uiBytes{};
};
inline std::string bytesHex(const std::array<uint8_t,16>& b){const char* d="0123456789abcdef";std::string t;for(auto v:b){t+=d[v>>4];t+=d[v&15];}return t;}
inline std::mutex mutex;
inline Snapshot latest;
// Last time a stable read showed a non-HUD screen (full-screen UI or the quick wheel); see rootCamera::uiHold.
inline std::atomic<uint64_t> lastNonHudMs{0};
inline Snapshot snapshot(){std::lock_guard<std::mutex> lock(mutex);return latest;}
inline bool boundNormal(const Snapshot& s,uintptr_t context,uint32_t entity,uint64_t now){
    return enabled&&s.number&&s.readable&&s.stable&&now>=s.milliseconds&&now-s.milliseconds<=200&&
        s.senderContext==context&&context&&s.entity==entity&&entity&&s.world&&
        s.screen==1&&s.auxiliary==0&&!s.nativeResult;
}
// The HUD while the bow or a staff is aimed too (2026-10-18; headset report 11: "taking the bow out at the shoulder
// changes where I walk"): aiming keeps screen 1 and sets the auxiliary HUD to 3, which boundNormal does not count; the
// head camera was then not sent and the stick walked along the game's camera (73 degrees off in that run). The walking
// head camera takes both; the other users of boundNormal stay as they are.
// 2026-10-18 12:40 (headset reports 4 and 5: "while the quick slots are open / a chest is open, I walk the wrong way"):
// the quick slot wheel keeps screen 1 with the auxiliary HUD 1 (bug report 4 of that run), an opened chest is screen 21
// ReadOnlyLootChest (report 5; keen::HudScreenType, 22 StorageChest beside it) - the player walks with both open, so
// the head camera takes them too. (The wheel's selection is RB + the right stick; the head camera turns the left one.)
inline bool walkableScreen(uint8_t screen,uint8_t auxiliary){
    return (screen==1&&(auxiliary==0||auxiliary==1||auxiliary==3))||screen==21||screen==22;
}
inline bool boundGameplay(const Snapshot& s,uintptr_t context,uint32_t entity,uint64_t now){
    return enabled&&s.number&&s.readable&&s.stable&&now>=s.milliseconds&&now-s.milliseconds<=200&&
        s.senderContext==context&&context&&s.entity==entity&&entity&&s.world&&
        walkableScreen(uint8_t(s.screen),uint8_t(s.auxiliary))&&!s.nativeResult;
}
// Building mode: the game's screen BlueprintMultiTool (left trigger with the building hammer; Mock run game100247; 28 until
// June, 29 in 1.0: controllerUiFixture::buildingScreen), same binding.
using controllerUiFixture::buildingScreen;
inline bool boundBuilding(const Snapshot& s,uintptr_t context,uint32_t entity,uint64_t now){
    return enabled&&s.number&&s.readable&&s.stable&&now>=s.milliseconds&&now-s.milliseconds<=200&&
        s.senderContext==context&&context&&s.entity==entity&&entity&&s.world&&
        s.screen==buildingScreen&&!s.nativeResult;
}
struct CallbackScope {CallbackScope(){++callbacks;}~CallbackScope(){--callbacks;}};
// The setter's hook (through the stub): sub is the subobject the value is stored in, value the predicate, frontend the
// caller's rsi. The store itself is left to the game.
inline void gate(void* sub,uint8_t value,void* frontend,uintptr_t returnAddress){
    const auto caller=returnAddress-baseAddress;
    const CallbackScope callback;original(sub,value);const bool result=value!=0;
    const ObserverLastError preserve;
    if(!accepting.load()||caller!=callReturn||!frontend)return;
    Snapshot s;s.number=++calls;s.milliseconds=GetTickCount64();s.nativeResult=result;
    try{
        const auto owner=reinterpret_cast<uintptr_t>(frontend);
        // A closing World may already have erased the UI controller. Service
        // only the owned input backend first, then skip all World-derived reads.
        const auto state=controllerUiFixture::serviceStoppedFrontend(owner)?0:read<uintptr_t>(owner+stateOffset);
        if(state&&read<uint8_t>(state+0x20)){
            const auto controller=read<uintptr_t>(state+controllerOffset);
            if(controller){
                s.data=controller+screenOffset;s.world=read<uintptr_t>(controller+worldOffset);
                // 0x85841e and 0x86e6ce load this exact pointer into R9 for the input builder (0x3a20f0).
                // Do not confuse controller+world with that context.
                s.senderContext=read<uintptr_t>(controller+senderContextOffset);
                if(s.senderContext)s.entity=read<uint32_t>(s.senderContext+entityOffset);
                const auto values=read<std::array<uint8_t,3>>(controller+screenOffset);
                floatingStamina::apply(owner,values[0]);   // the game's floating stamina ring off on the play screen (floating_stamina.hpp)
                gameHudVisibility::apply(owner,values[0]);   // the HUD's auto-hide by the game's own Contextual settings (game_hud_visibility.hpp)
                if(values[0]==1)gameAmmoList::observe(controller,GetTickCount64());   // the HUD's spell / ammunition list (game_ammo_list.hpp)
                const auto ui=read<uintptr_t>(owner+uiOffset),device=read<uintptr_t>(owner+deviceOffset),localDevice=read<uintptr_t>(controller+localDeviceOffset);
                if(ui){s.uiMode=read<uint8_t>(ui+9);s.uiBytes=read<std::array<uint8_t,16>>(ui);}
                // an item menu open in the backpack (hooks/menu_pointer.hpp aPad 3): frontend+0x33a57, 0 with the menu and 7 without (Mock
                // 2026-10-24 -054815, -094956). ui+0x2d2c73 (1 with the menu in the Mock) never rose on the headset (log 20261024-091911).
                if(menuPointer::aPad==3){const auto mark=read<uint8_t>(owner+0x33a57);menuPointer::itemMenuMark=mark;menuPointer::itemMenuOpen=mark==0;
                    // what it reads on the chests and the backpack (2026-10-24: 0 on the start menu too, Mock -133659; a chest's not known)
                    static uint8_t lastMark=255,lastScreen=255;static unsigned markLines=0;const uint8_t screenNow=values[0];
                    if((screenNow==4||screenNow==21||screenNow==22)&&(mark!=lastMark||screenNow!=lastScreen)&&markLines<300){++markLines;
                        logFile->event("item_menu_mark","\"screen\":"+std::to_string(unsigned(screenNow))+",\"mark\":"+std::to_string(unsigned(mark))+",\"tab\":"+std::to_string(unsigned(read<uint8_t>(owner+0x325a0))));}
                    lastMark=mark;lastScreen=screenNow;}
                // the menu's tab (frontend+0x325a0: 1 world map, 2 journal, 3 craft, 4 backpack; tab_diff over 7 copies, Mock -060907)
                menuPointer::menuTab=menuPointer::aPad==3?read<uint8_t>(owner+0x325a0):uint8_t(255);
                if(device)s.deviceBytes=read<std::array<uint8_t,16>>(device);
                if(localDevice)s.localBytes=read<std::array<uint8_t,16>>(localDevice);
                if(device)s.deviceMode=read<uint8_t>(device);
                // diagnostic (2026-10-24): EVR_UI_SELECT_LOG=1 - the backpack's pad choice (frontend+0x325d8) at each change, with the
                // menu pointer's posted moves and buttons (does a button's down move it?)
                {static const bool selectLog=[]{char v[4]{};return GetEnvironmentVariableA("EVR_UI_SELECT_LOG",v,sizeof v)==1&&v[0]=='1';}();
                 if(selectLog){static uint32_t lastSlot=0xffffffffu;static uint64_t lastClicks=0;static uint16_t lastPop=0xffff;const auto slot=read<uint32_t>(owner+0x325d8);const auto clicks=menuPointer::clicks.load();
                  const uint16_t pop=uint16_t((ui?read<uint8_t>(ui+0x2d2c73):0xf)|(read<uint8_t>(owner+0x33a57)<<8));   // candidates: an item menu open (ui+0x2d2c73: 1 open / 0 closed in Mock, never 1 on the headset; frontend+0x33a57: 0 open / 7 closed, Mock 054815)
                  if(slot!=lastSlot||clicks!=lastClicks||pop!=lastPop){lastSlot=slot;lastClicks=clicks;lastPop=pop;try{logFile->event("ui_select","\"slot\":"+std::to_string(slot)+",\"popup_bytes\":"+std::to_string(pop)+",\"tab\":"+std::to_string(unsigned(menuPointer::menuTab.load()))+",\"device_mode\":"+std::to_string(s.deviceMode)+
                      ",\"moves\":"+std::to_string(menuPointer::moves.load())+",\"clicks\":"+std::to_string(clicks)+",\"sample\":"+std::to_string(s.number));}catch(...){}}}}
                // The right A as the pad's A (hooks/menu_pointer.hpp aPad, test): the game in its pad mode for the button's up and the pad's A
                if(device&&menuPointer::forcePad.load()&&menuPointer::installed&&s.deviceMode!=1){*reinterpret_cast<volatile uint8_t*>(device)=1;s.deviceMode=1;}
                // A pull of the menu pointer: the game's mouse mode before its button (hooks/menu_pointer.hpp wantMouse) -
                // the device mode and the local one, both 0 as the game's own first click leaves them.
                if(device&&s.deviceMode!=0&&menuPointer::wantMouse.load()&&menuPointer::installed){
                    *reinterpret_cast<volatile uint8_t*>(device)=0;if(localDevice)*reinterpret_cast<volatile uint8_t*>(localDevice)=0;s.deviceMode=0;
                    const auto n=++menuPointer::mouseModeSet;if(n<=20||n%100==0)try{logFile->event("menu_pointer_mouse_mode","\"count\":"+std::to_string(n)+",\"local_too\":"+(localDevice?"true":"false"));}catch(...){}
                }
                // The menu pointer's mouse messages turn the game to its keyboard-and-mouse prompts (device mode 0);
                // 0.25 to 1.5 s after the last one, with no pull being given, it is put back to the pad's (1) - the key map
                // stays the pad's but for the click itself (user 2026-10-18, hooks/menu_pointer.hpp keepPad).
                if(device&&s.deviceMode==0&&menuPointer::keepPad&&menuPointer::installed&&menuPointer::gameMouse&&!menuPointer::mouseHeld.load()){
                    const auto posted=menuPointer::postedAt.load(),now=GetTickCount64();
                    if(posted&&now>=posted+250&&now-posted<=1500){*reinterpret_cast<volatile uint8_t*>(device)=1;s.deviceMode=1;
                        const auto n=++menuPointer::padKept;if(n<=5||n%300==0)try{logFile->event("menu_pointer_pad_kept","\"count\":"+std::to_string(n)+",\"since_post_ms\":"+std::to_string(now-posted));}catch(...){}}
                }
                if(localDevice)s.localMode=read<uint8_t>(localDevice);
                uiScan::maybe(owner,state,controller,ui,device,localDevice);   // diagnostic, EVR_UI_SCAN_FRAMES
                stringScan::maybe();   // diagnostic, EVR_STRING_SCAN_FRAME / EVR_STRING_SCAN_HEX
                uiCursorConfig::maybe();   // diagnostic, EVR_SPELL_LIST_OFFSET / EVR_UI_CURSOR_SCAN (ui_cursor_config.hpp)
                uiCursorConfig::keep(owner,GetTickCount64());   // the game's spell list off the picture while the mod draws its own
                s.screen=values[0];s.secondary=values[1];s.auxiliary=values[2];s.readable=true;
                s.stable=values==read<std::array<uint8_t,3>>(controller+screenOffset)&&
                    s.world==read<uintptr_t>(controller+worldOffset)&&controller==read<uintptr_t>(state+controllerOffset)&&
                    s.senderContext==read<uintptr_t>(controller+senderContextOffset)&&
                    (!s.senderContext||s.entity==read<uint32_t>(s.senderContext+entityOffset))&&
                    read<uint8_t>(state+0x20)&&state==read<uintptr_t>(owner+stateOffset);
            }
        }
    }catch(...){++failures;}
    if(s.readable&&s.stable){
        menuPointer::screen=s.screen;menuPointer::deviceMode=s.deviceMode;pauseClock::update(s.screen==2,GetTickCount64());
        if(!controllerUiFixture::requested)inputGamepad::capture(reinterpret_cast<uintptr_t>(frontend),s.number);
        // A pad style (2026-10-25): no live input reads the screen, so whether the game shows its play HUD (the bow's and the
        // staff's aim, auxiliary 3, too) goes to the HUD auto-hide from here (view_resource_probe.cpp uiHudBridge).
        if(game::padStyle.load()){controllerUiFixture::liveScreenGameplay=s.world&&s.senderContext&&s.entity&&s.screen==1&&(s.auxiliary==0||s.auxiliary==3)&&!s.nativeResult;
            controllerUiFixture::liveScreenAt=GetTickCount64();}
        else controllerUiFixture::onScope(reinterpret_cast<uintptr_t>(frontend),s.world&&s.senderContext&&s.entity&&s.screen==1&&s.auxiliary==0&&!s.nativeResult,s.screen,s.auxiliary,s.world&&s.senderContext&&s.entity,s.nativeResult);
    }
    const auto previous=snapshot();
    if(s.readable&&s.stable&&s.world&&(s.screen!=1||s.auxiliary!=0))lastNonHudMs=s.milliseconds;
    {std::lock_guard<std::mutex> lock(mutex);latest=s;}
    if(s.number<=8||s.number%120==0||previous.screen!=s.screen||previous.auxiliary!=s.auxiliary||previous.secondary!=s.secondary||previous.deviceMode!=s.deviceMode||previous.localMode!=s.localMode||previous.uiMode!=s.uiMode||
       previous.deviceBytes!=s.deviceBytes||previous.localBytes!=s.localBytes||previous.uiBytes!=s.uiBytes)try{
        logFile->event("input_ui_native_sample","\"number\":"+std::to_string(s.number)+",\"caller\":"+hex(caller)+
            ",\"hud_screen\":"+std::to_string(s.screen)+",\"screen_secondary\":"+std::to_string(s.secondary)+",\"auxiliary_state\":"+std::to_string(s.auxiliary)+
            ",\"frontend_input_mode\":"+std::to_string(s.deviceMode)+",\"local_input_mode\":"+std::to_string(s.localMode)+",\"ui_gamepad_mode\":"+std::to_string(s.uiMode)+
            ",\"device_bytes\":\""+bytesHex(s.deviceBytes)+"\",\"local_bytes\":\""+bytesHex(s.localBytes)+"\",\"ui_bytes\":\""+bytesHex(s.uiBytes)+"\""+
            ",\"native_result\":"+(result?"true":"false")+",\"readable\":"+(s.readable?"true":"false")+
            ",\"stable_read\":"+(s.stable?"true":"false")+",\"world\":"+hex(s.world)+
            ",\"sender_context\":"+hex(s.senderContext)+",\"entity\":"+std::to_string(s.entity)+",\"return_unchanged\":true");
    }catch(...){++failures;}
}
inline void verify(){
    const auto bytes=parsePattern(pattern);const auto call=baseAddress+callSite;
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)!=std::vector<size_t>{0}||
       read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+rva)
        throw std::runtime_error("native frontend input predicate signature/caller changed");
    const auto contextLoad=parsePattern("4C 8B 8B E0 D4 47 00");
    if(scan(reinterpret_cast<const uint8_t*>(baseAddress+0x859c9e),contextLoad.size(),contextLoad)!=std::vector<size_t>{0})
        throw std::runtime_error("UI to input sender context load changed");
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rva),reinterpret_cast<void*>(&evr_input_ui_stub),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("frontend input observer hook failed");
    enabled=true;accepting=true;
    uiScan::install();
    stringScan::install();
    uiCursorConfig::install();
    logFile->event("input_ui_observer_enabled","\"rva\":"+hex(rva)+",\"caller\":\"0x84741f\",\"setter\":true,\"screen_offset\":"+std::to_string(screenOffset)+",\"auxiliary_offset\":"+std::to_string(screenOffset+2)+",\"read_only\":true");
}
inline void observe(unsigned sample,uint32_t entity,uint32_t tick,uintptr_t senderContext){
    if(!enabled)return;
    Snapshot s;{std::lock_guard<std::mutex> lock(mutex);s=latest;}
    const auto now=GetTickCount64(),age=now>=s.milliseconds?now-s.milliseconds:UINT64_MAX;
    const bool known=s.number&&s.readable&&s.stable&&age<=200&&s.screen<50;
    // Auxiliary byte can take 0..4; it is NOT proven to be WheelMenuType.
    const bool sameContext=s.senderContext&&s.senderContext==senderContext&&s.entity&&s.entity==entity;
    const bool candidate=known&&sameContext&&s.screen==1&&s.auxiliary==0&&!s.nativeResult&&s.world;
    std::ostringstream out;out<<"\"sample\":"<<sample<<",\"entity\":"<<entity<<",\"tick\":"<<tick
        <<",\"native_sample\":"<<s.number<<",\"age_ms\":"<<age<<",\"data\":"<<hex(s.data)<<",\"world\":"<<hex(s.world)
        <<",\"sender_context\":"<<hex(s.senderContext)<<",\"ui_entity\":"<<s.entity<<",\"same_input_context\":"<<(sameContext?"true":"false")
        <<",\"readable\":"<<(s.readable?"true":"false")<<",\"stable_read\":"<<(s.stable?"true":"false")
        <<",\"hud_screen\":"<<unsigned(s.screen)<<",\"auxiliary_state\":"<<unsigned(s.auxiliary)
        <<",\"native_result\":"<<(s.nativeResult?"true":"false")<<",\"known_enum\":"<<(known?"true":"false")
        <<",\"normal_hud_candidate\":"<<(candidate?"true":"false")<<",\"gameplay_state_validated\":false,\"applied\":false";
    logFile->event("input_ui_sample",out.str());
}
inline void disable(bool handoffToNativeOwner=false){
    std::exception_ptr fixtureError;
    if(handoffToNativeOwner){
        if(!controllerUiFixture::backendExitRequested||!controllerUiFixture::backendWaitReady.load()||!controllerUiFixture::installed.load()||!controllerUiFixture::inputModulePinned||controllerUiFixture::failed.load())throw std::runtime_error("UI handoff requires installed/pinned proven native controller owner");
        controllerUiFixture::stopGameplayReaders();
    }else try{controllerUiFixture::drain();}catch(...){fixtureError=std::current_exception();}
    accepting=false;if(!enabled)return;
    if(MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva))!=MH_OK)throw std::runtime_error("frontend input observer disable failed");
    enabled=false;const auto deadline=GetTickCount64()+500;
    while(callbacks.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    floatingStamina::putBack("the world is left");   // the player's own floating stamina back in the game's memory
    gameHudVisibility::putBack("the world is left");
    uiCursorConfig::finish();
    const auto bytes=parsePattern(pattern);
    const bool restored=scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};
    logFile->event("input_ui_observer_removed","\"restored\":"+std::string(restored?"true":"false")+
        ",\"calls\":"+std::to_string(calls.load())+",\"read_failures\":"+std::to_string(failures.load())+
        ",\"callbacks\":"+std::to_string(callbacks.load()));
    if(!restored||callbacks.load())throw std::runtime_error("frontend input observer did not quiesce/restore");
    if(fixtureError)std::rethrow_exception(fixtureError);
}
}
// The stub (hooks/input_ui_stub.asm) jumps here with the caller's rsi in r8.
extern "C" void evr_input_ui_gate(void* sub,uint8_t value,void* frontend){inputUi::gate(sub,value,frontend,reinterpret_cast<uintptr_t>(_ReturnAddress()));}
