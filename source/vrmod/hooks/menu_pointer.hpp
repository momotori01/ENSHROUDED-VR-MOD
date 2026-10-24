#pragma once
// view_resource_probe.cpp includes these at file scope first, like the other value headers: there this file sits
// inside its unnamed namespace.
#include <evr/head_gesture.hpp>
#include <evr/menu_pointer.hpp>
#include <evr/motion_actions.hpp>
#include <evr/pause_clock.hpp>
// VHVR's pointer on the game's menus (docs/analysis/vhvr_spec_20261010/ui_hud.md, section 2; the value logic is
// include/evr/menu_pointer.hpp). The pointed place is given to the game as the usual mouse messages, posted to the
// game's own window: WM_MOUSEMOVE with the pointed pixel, the left button for the right trigger, the right button for
// B (VHVR's assignment). Mock run game193917: the slot under the posted place is highlighted and its description is
// shown; the game never asked GetCursorPos on the way (a substitution of that import was tried there and was called
// zero times, so it is gone). The system's cursor is never moved and nothing is injected system wide - another
// program on this PC may be using the mouse.
// What comes with it: the game switches to its keyboard-and-mouse prompts while the pointer moves, and back to the
// pad's at the next pad input (inputUi deviceMode 0 / 1; the same run).
// The dot that shows the pointed place in the headset is drawn by the layer into the UI image (evrSetUiCursor).
// Mock run game200550: a left click on the craft tab opens the craft page, one on the backpack tab goes back, a
// right click on an item opens its menu (equip / split / delete), and B with the hand pointing away from the plate
// is the pad's "back" as before.
// On with the launch flag -MenuPointer (policy file world-menu-pointer.txt), or with EVR_MENU_POINTER=1 in the game's
// environment.
// The plate of an open menu stands still (include/evr/menu_pointer.hpp plateOrigin, VHVR LockGuiPositionWhenMenuOpen;
// the user after the headset run of 2026-10-13: "once the menu is out, fix it in space instead of following the
// head"): when a menu opens, its place of that moment - upright - is frozen and told to the layer (evrSetUiPlate),
// and the pointer is cut with that plate. During play (the HUD) the plate follows the head as before.
// Not yet: the middle button and the chords, scrolling, a drawn ray, a plate in the hand.
// The age of what the per-tick observers saw, on a clock that stands still while the game is paused
// (include/evr/pause_clock.hpp): the pause menu and the settings are UI screen 2. Fed from the frontend's scope
// (input_ui_observer.hpp), asked by whoever shows something from such a sample (the hands, the held weapon, the
// shield, the selected item).
namespace pauseClock {
inline std::atomic<uint64_t> pausedAt{0},resumedAt{0};inline std::atomic<bool> paused{false};
inline void update(bool nowPaused,uint64_t now){
    evr::game::PauseClock c{pausedAt.load(),resumedAt.load(),paused.load()};const bool before=c.paused;c.update(nowPaused,now);
    pausedAt=c.pausedAt;resumedAt=c.resumedAt;paused=c.paused;
    if(before!=c.paused&&logFile)logFile->event("pause_clock","\"paused\":"+std::string(c.paused?"true":"false")+",\"milliseconds\":"+std::to_string(now)+
        ",\"samples_keep_their_age_while_paused\":true");
}
inline bool fresh(uint64_t taken,uint64_t now,uint64_t limit){
    return evr::game::PauseClock{pausedAt.load(),resumedAt.load(),paused.load()}.fresh(taken,now,limit);
}
}
namespace menuPointer {
inline unsigned mode=0;inline bool requested=false,installed=false;
// The key map (2026-10-18). Morning headset run: "in the settings and the UI the key map changes with whether the pointer is
// on the screen; it causes wrong operations often - make it not change". Each posted mouse button made the game show its
// keyboard-and-mouse prompts (frontend+0x5f8 to 0, all six presses of that run), and on the plate B was the right mouse
// button instead of "back". 11:00 to 13:10 the ray worked the mod's own buttons only; the user then: "that will confuse
// - either buttons and sticks only, or the laser pointer back with the trigger selecting, and the key map not
// switching; the latter is ideal". So: the ray is the game's mouse again for the trigger, B is always the pad's B.
// The game takes the mouse only in its mouse mode (Mock game-20261018-130739: with the device mode held at the pad's the
// click on "settings" did nothing and the highlight stayed; -131053 without: the settings opened). So the mouse is told
// only around a press (keepPad): while the trigger is up the dot alone shows where the ray is and nothing is posted; a
// pull posts the place for six frames and then the button (the game turns to its mouse mode for that moment), the place
// follows while the trigger is held (dragging a slider), and 0.25 s after the release the device mode is put back to the
// pad's (hooks/input_ui_observer.hpp) - the prompts are the mouse's only for the click. EVR_MENU_POINTER_GAME=0: the mod's
// buttons only (11:00 to 13:10); EVR_MENU_POINTER_KEEP_PAD=0: the place posted all the time, the game keeps its mouse mode.
inline bool gameMouse=true;
inline bool keepPad=true;                          // EVR_MENU_POINTER_KEEP_PAD=0: the game may switch to its mouse prompts
inline std::atomic<uint64_t> postedAt{0};          // the last mouse message posted to the game (GetTickCount64)
inline std::atomic<unsigned> padKept{0};
inline std::atomic<bool> mouseHeld{false};          // a pull is being given to the game (its button down, or about to be)
// The game's mouse mode for the pull (Mock game-20261018-140143, the backpack): 140 posted moves left the device mode at the
// pad's (1); the first WM_LBUTTONDOWN turned it to 0 and was itself dropped - the drag of an item that began with it carried
// nothing - while a later click, with the mode already 0, chose its item. So a pull asks for the mouse mode first
// (input_ui_observer.hpp writes 0 to the device mode on the game's next frame), the place follows for the lead frames (the
// slot under it is taken in the mouse mode), and the button comes after. EVR_MENU_POINTER_MOUSE_FIRST=0: not asked.
inline bool mouseFirst=true;
inline std::atomic<bool> wantMouse{false};
inline bool aMiddle=false;     // diagnostic: EVR_MENU_POINTER_A_BUTTON=middle - the right A on the plate is the middle button (does a click move the pad's choice only?)
inline unsigned aHold=0;       // diagnostic (2026-10-24): EVR_MENU_POINTER_A_HOLD=<frames> - the right A's button held that many frames after its down (does the down alone move the pad's choice, and does the game act on the down or the up?)
inline bool hoverMouse=false;   // diagnostic (ui_memory_scan.hpp): EVR_MENU_POINTER_HOVER_MOUSE=1 holds the game's mouse mode while the ray is on the plate
// The right A as the game's A on the plate (2026-10-24; user 10-23: "with the ray on the screen the key map changes - fix it").
// Mock game-20261024-052201 (EVR_MENU_POINTER_A_HOLD=30): the right button's down alone moved the pad's choice to the slot under
// the cursor (backpack slot 13 -> 14) and closed the open item menu; its up opened the item's menu. So (test, EVR_MENU_POINTER_A_PAD
// =1 with the right button, 2 with the left): the button's down in the game's mouse mode, then the game back in its pad mode
// (forcePad, input_ui_observer.hpp), the up there, and the pad's A for a moment (padAUntil, controller_ui_fixture.hpp) - the A of
// the thing pointed at.
// Mock -053617 / -053931: the right button's down does not move the pad's choice onto a row of an open item menu (it passes to
// the slot behind and the menu closes), the left button's does (its Delete row then deleted the item, as the pad's A on it); on
// the map (the world tab) the left button's down and the move away dragged the map far off (-055636). So 3 (auto): the left
// button while an item menu is open (itemMenuOpen, input_ui_observer.hpp: ui+0x2d2c73, 1 with the menu, 0 without - found by
// popup_diff over 8 memory copies, Mock -054815, and the same in -060059), the right button else.
// The map (the world tab) with either button's down and the move away: the map ran far off (-060533; with the right click as
// before it stayed, -060907). So 3 does it in the backpack's tab only (menuTab 4, frontend+0x325a0, input_ui_observer.hpp); the
// other tabs and screens keep the right click. EVR_MENU_POINTER_A_PAD=3 (test only), 1 / 2 the right / left button on every screen.
// 2026-10-24 09:40, the user after the headset: "with an item selected, A to open its menu (split, delete) acts like A pressed twice:
// the menu's top row - often Delete - is chosen at once; stop at the menu opening". Mock game-20261024-094956: on the item already
// chosen, the right button's down itself opens its menu (the mark below at once), and the pad's A then took the top row (the ring
// equipped). So the pad's A is not given when the menu was shut at the press and is open after the button (openAtPress). The item
// menu's mark is frontend+0x33a57 (0 open, 7 shut); ui+0x2d2c73 (1 open in the Mock) never rose on the headset (log 20261024-091911).
// The default (the user: "the way itself is right"); EVR_MENU_POINTER_A_PAD=0: the right click everywhere as before.
inline unsigned aPad=3;inline std::atomic<uint8_t> itemMenuMark{255};
inline std::atomic<uint8_t> menuTab{255};
inline std::atomic<bool> forcePad{false},itemMenuOpen{false};inline std::atomic<uint64_t> padAUntil{0},padAPulses{0};
// 2026-10-24 13:40, the user after the headset (log 20261024-130740): "in the left menu (the start menu, screen 2) and the item
// box (the chests, 21 / 22) the buttons do nothing while the pointer is there". The right A was the right click there: 20 presses on
// the start menu (on the game's rows and on the mod's bug report button) and 14 on the box, none of them did anything.
// The start menu's rows do not take the right button's down as a choice (Mock game-20261024-133659: the right A on "Difficulty" by
// the backpack's way gave the pad's A to "Back to the game", which was chosen, and the menu closed), so there the right A is the left
// click at the pointed place, as the trigger (startMenuA; Mock -134334: "Difficulty" opened). The chests: the right A is the pad's A
// as it is, on the slot the game has chosen, nothing to the mouse (chestA). The backpack's way (the down at the pointed place first)
// asks the item menu's mark frontend+0x33a57 (7 shut / 0 open in the bag), and the mark read 0 on the start menu with no item menu
// (the same Mock) - what it reads on a chest is not known (three Mock tries did not open a chest: the left trigger took the flame
// altar beside the spawn, -134758), and a wrong "open" there would pick the item up with the left button and drag it off the window.
// EVR_MENU_POINTER_A_PAD_SCREENS=0: the backpack's tab only, the right click elsewhere, as before.
inline bool aPadScreens=true;
struct PadA {unsigned stage=0,frames=0;bool left=false,openAtPress=false,items=false;};inline PadA padA;
inline bool overlayRight=false;   // the right A pressed a button of the mod's own (the start menu's VR settings and bug report)
inline bool aAsLeft=false;        // the right A is giving the left click (the start menu)
inline std::atomic<unsigned> mouseModeSet{0};
// A button's way to the game (keepPad): the place first, the button down after the lead frames, up at least
// minimumDownFrames after its down (2026-10-20 17:40, user: "with the pointer the stack's menu needs several presses -
// unsteady"; headset run 20261020-172049: the right A was held 130..160 ms, under the 6 lead frames at the menu's ~35
// frames a second, so its down and its up went in one frame - a whole click, which the game often did not take).
constexpr unsigned minimumDownFrames=3;
struct Posted {unsigned downIn=0,upIn=0,sinceDown=0;bool upLater=false,down=false;bool busy()const{return downIn||upIn||upLater||down;}};
inline Posted leftPosted,rightPosted;
// 6 frames (headset run 13:30: items of the backpack took no click or drag with 2 - guess: the game turns to its mouse mode and
// moves its focus under the cursor a frame or two after the first move)
inline unsigned leadFrames=6;
inline std::atomic<uint64_t> moves{0},clicks{0};
// From the frontend's scope (input_ui_observer.hpp): the screen that is open and the device the game listens to.
inline std::atomic<uint8_t> screen{255},deviceMode{255};
// A UI that covers the HUD screen (user 2026-10-19 03:00: "when I die I cannot respawn"): the death screen keeps the HUD
// screen 1, but the player's PlayerFlags gain UiCoverRequest (bit 2; InFullScreenUi is bit 1) - 517 from just before the
// player stopped moving to the end of that session (log 20261019-024824), and never in the earlier sessions' 6000 samples
// of play. While it stands, the pointer and the controller treat the screen as a menu: the ray clicks, the sticks choose,
// A confirms and B goes back (before, the death screen was played as the HUD: no pointer, the stick turned the player,
// and nothing reached the game's buttons). Published by input_head_idle.hpp observeState (the player's state each tick).
inline std::atomic<uint32_t> playerFlags{0};inline std::atomic<uint64_t> playerFlagsMs{0};
inline bool uiCover(uint64_t now){const auto at=playerFlagsMs.load();return at&&now>=at&&now-at<=500&&(playerFlags.load()&((1u<<1)|(1u<<2)))!=0;}
// For the pad that is sent (controller_ui_fixture.hpp): the right trigger and B are the mouse's buttons just now.
inline std::atomic<bool> trigger{false},secondary{false};
// The buttons keep their roles with the ray on the screen or not (2026-10-20 17:20, user: "the role of a button changes with
// the pointer on the screen or not - make it the same: the trigger drags; the right A opens the menu of a stack (split
// and the like) with the pointer too"): the trigger is the mouse's left button (pick up, drag, choose), the right A its
// right button - an item's menu (equip, split, delete; Mock game200550), as the pad's A opens it - and the left A stays
// the pad's B (back). The pad's A is not sent while the ray is on the screen. (12:30 to 17:20: the right A the left click,
// the left A the right click.)
inline std::atomic<bool> primary{false};
using CursorFn=int(*)(float,float,int);
inline CursorFn showCursor=nullptr;
using PlateFn=int(*)(int,const float*);
inline PlateFn setPlate=nullptr;
inline bool plateFixed=false,plateTold=false;inline evr::Pose plateOrigin=evr::identityPose();inline uint64_t plateSession=0,plateGeneration=0;inline unsigned plateLogs=0;
inline void install(){
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER",value,sizeof value);if(n&&n<sizeof value)mode=unsigned(std::strtoul(value,nullptr,10));}
    if(requested)mode=1;
    if(mode!=1){mode=0;return;}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_GAME",value,sizeof value);gameMouse=!(n==1&&value[0]=='0');}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_KEEP_PAD",value,sizeof value);keepPad=!(n==1&&value[0]=='0');}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_MOUSE_FIRST",value,sizeof value);mouseFirst=!(n==1&&value[0]=='0');}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_HOVER_MOUSE",value,sizeof value);hoverMouse=n==1&&value[0]=='1';}
    {char value[16]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_A_BUTTON",value,sizeof value);aMiddle=n==6&&std::string(value)=="middle";}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_A_HOLD",value,sizeof value);if(n&&n<sizeof value)aHold=unsigned(std::min(300ul,std::strtoul(value,nullptr,10)));}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_A_PAD",value,sizeof value);if(n==1&&value[0]>='0'&&value[0]<='3')aPad=unsigned(value[0]-'0');}
    {char value[8]{};const auto n=GetEnvironmentVariableA("EVR_MENU_POINTER_A_PAD_SCREENS",value,sizeof value);aPadScreens=!(n==1&&value[0]=='0');}
    if(auto layer=GetModuleHandleW(L"evr_diagnostic.dll")){showCursor=reinterpret_cast<CursorFn>(GetProcAddress(layer,"evrSetUiCursor"));setPlate=reinterpret_cast<PlateFn>(GetProcAddress(layer,"evrSetUiPlate"));}
    installed=true;
    logFile->event("menu_pointer_enabled",std::string("\"game_mouse\":")+(gameMouse?"true":"false")+",\"keep_pad\":"+(keepPad?"true":"false")+",\"mouse_mode_first\":"+(mouseFirst?"true":"false")+",\"b_is_the_pads\":true"+",\"messages_to_the_game_window_only\":true,\"system_cursor_moved\":false,\"dot_in_ui_image\":"+(showCursor?"true":"false")+",\"menu_plate_fixed_in_space\":"+(setPlate?"true":"false"));
}
inline HWND window=nullptr;
inline BOOL CALLBACK pick(HWND h,LPARAM out){
    DWORD process=0;GetWindowThreadProcessId(h,&process);
    if(process!=GetCurrentProcessId()||!IsWindowVisible(h)||GetWindow(h,GW_OWNER))return TRUE;
    RECT r{};if(!GetClientRect(h,&r))return TRUE;
    auto& best=*reinterpret_cast<std::pair<HWND,long>*>(out);const long area=(r.right-r.left)*(r.bottom-r.top);
    if(area>best.second)best={h,area};
    return TRUE;
}
inline HWND gameWindow(){
    if(window&&IsWindow(window))return window;
    std::pair<HWND,long> best{nullptr,0};EnumWindows(&pick,reinterpret_cast<LPARAM>(&best));window=best.first;return window;
}
inline evr::menu::ClickSlack slack;inline evr::menu::Buttons buttons;inline std::mutex mutex;
// A button of the mod's own on the plate (the VR bug report, bug_report.hpp): overlayButtonAt(u, v, screen) says whether
// the pointed place (0..1 of the UI image) is on it - and whether it is shown; a press there goes to overlayButtonPress
// and its click is not posted to the game (whatever lies under it stays untouched).
inline bool (*overlayButtonAt)(float u,float v,uint8_t screen)=nullptr;inline void (*overlayButtonPress)()=nullptr;inline bool overlayPress=false;
// While such a press is held (the trigger down), the pointed place every frame, and its release (the VR settings' slider).
inline void (*overlayButtonDrag)(float u,float v)=nullptr;inline void (*overlayButtonRelease)()=nullptr;
inline void (*onDisable)()=nullptr;   // the bug report's end, before the pointer goes
inline POINT lastClient{-1,-1};inline uint64_t lastLog=0;inline int lastState=-1,lastShown=0;
// Once per XR frame, on the thread that publishes the hands (view_resource_probe.cpp, next to publishGesture).
inline void frame(const evr::motion::HandFrame& f,const evr::motion::GestureHead& head,uint64_t now){
    if(!installed)return;
    try{
        std::lock_guard<std::mutex> lock(mutex);
        const auto open=screen.load();
        // 1 is the HUD, the building mode (its own ray, building_cursor_ray.hpp) and the summoning staff's adjustment
        // (NpcSummonerMenu, the hand aims at the world there) keep the hand's ray, 255 not known yet. 4 is the menu with
        // the tabs; the other screens with a cursor (chests, craftsmen) get the pointer. The game's HudScreenType moved
        // in 1.0 (LogicEditor came in at 20): the building mode 28 -> 29, the summoner 33 -> 37, and 33 is now the
        // screwdriver's building mode (ScrewdriverMultiToolBuildingMode), which aims at the world as the building mode.
        const bool menu=(open!=1&&open!=29&&open!=33&&open!=37&&open!=255)||(open==1&&uiCover(now));   // + the death screen (uiCover)
        // The world tab (the map, menuTab 1) as the chests (2026-10-24, user after the headset: "on the world screen the buttons
        // change with the pointer and without it"; then, with the left click at the pointed place: "clicking a location drags the
        // map; and with a location picked, A is not taken unless the pointer is on it - dragging is fine, but with a location
        // picked A should always bring its choices (fast travel and the like)"): the right A is the pad's A as it is, on the
        // location the game has picked, nothing to the mouse; the trigger's left click moves the map.
        const bool worldTab=(open==4&&menuTab.load()==1)||open==5;   // + the map of its own (screen 5, the map key; Mock -183453)
        // Every other screen the same (2026-10-24, user after the headset: "in the craft screen too, with the pointer there A
        // does not make anything - every UI has this, go through all of them"): the right A is the game's A on what the game has
        // chosen - the trigger's left click chooses - but for the backpack's tab (its own way: the slot pointed at, then the
        // pad's A; user "UI OK" 10-24) and the start menu (the left click at the pointed place: its rows take no pad's A on the
        // row pointed at, Mock -133659). Before: the right click everywhere else.
        const bool bagTab=open==4&&menuTab.load()==4;
        const bool startMenuA=aPadScreens&&aPad&&open==2,chestA=aPadScreens&&aPad&&menu&&open!=2&&!bagTab;   // the right A there (aPadScreens above)
        const auto& hand=f.hands[1];
        const bool headTracked=(head.flags&15)==15&&evr::finite(head.pose.position);
        const bool tracked=f.focused&&hand.aimActive&&(hand.aimFlags&15)==15&&headTracked;
        // The plate of an open menu: frozen where it stood when the menu opened (and again when the tracking's origin
        // changes), free with the head when the menu has gone.
        if(!menu)plateFixed=false;
        else if(setPlate&&headTracked&&(!plateFixed||plateSession!=f.sessionGeneration||plateGeneration!=f.originGeneration)){
            plateOrigin=evr::menu::plateOrigin(head.pose);plateFixed=true;plateSession=f.sessionGeneration;plateGeneration=f.originGeneration;
            if(plateLogs++<100){std::ostringstream o;o<<std::setprecision(7)<<"\"screen\":"<<unsigned(open)<<",\"frame\":"<<f.frame<<",\"origin_position\":["<<plateOrigin.position.x<<','<<plateOrigin.position.y<<','<<plateOrigin.position.z
                <<"],\"origin_orientation\":["<<plateOrigin.orientation.x<<','<<plateOrigin.orientation.y<<','<<plateOrigin.orientation.z<<','<<plateOrigin.orientation.w<<"],\"upright\":true,\"distance_m\":1.5";
                logFile->event("menu_plate_fixed",o.str());}
        }
        if(setPlate){
            if(plateFixed){const float p[7]={plateOrigin.orientation.x,plateOrigin.orientation.y,plateOrigin.orientation.z,plateOrigin.orientation.w,plateOrigin.position.x,plateOrigin.position.y,plateOrigin.position.z};
                if(setPlate(1,p))plateFixed=false;else plateTold=true;}
            if(!plateFixed&&plateTold){setPlate(0,nullptr);plateTold=false;if(plateLogs++<100)logFile->event("menu_plate_free","\"screen\":"+std::to_string(unsigned(open))+",\"frame\":"+std::to_string(f.frame));}
        }
        evr::menu::Spot spot;RECT client{};const HWND h=gameWindow();
        if(menu&&tracked&&h&&GetClientRect(h,&client)&&client.right>0&&client.bottom>0){
            const float height=2.f*float(client.bottom)/float(client.right);
            if(plateFixed){
                // The frozen plate and the hand's ray, both in the tracking's own space.
                const auto plate=evr::menu::fixedPlate(plateOrigin,1.5f,2.f,height,uint32_t(client.right),uint32_t(client.bottom));
                spot=evr::menu::spot(plate,hand.aim.position,evr::rotate(hand.aim.orientation,{0,0,-1}));
            }else{
                // The plate is fixed to the head (layer/native_xr.hpp, markUiRendered with the default placement: the
                // VIEW space, 1.5 m ahead, 2 m wide, the image's aspect). The hand's ray goes into the head's frame.
                const auto inHead=evr::compose(evr::inverse(head.pose),hand.aim);
                evr::menu::Plate plate;plate.pose.position={0,0,-1.5f};plate.width=2.f;plate.height=height;
                plate.pixelsX=uint32_t(client.right);plate.pixelsY=uint32_t(client.bottom);
                spot=evr::menu::spot(plate,inHead.position,evr::rotate(inHead.orientation,{0,0,-1}));
            }
        }
        // Controller-only (not gameMouse): the ray counts only on a button of the mod's own; B is never the mouse's.
        const bool onButton=spot.on&&overlayButtonAt&&client.right>0&&client.bottom>0&&overlayButtonAt(float(spot.pixelX)/float(client.right),float(spot.pixelY)/float(client.bottom),open);
        if(!spot.on&&overlayButtonAt)overlayButtonAt(-1.f,-1.f,open);   // off the plate: nothing of the mod's is pointed at
        const auto& leftHand=f.hands[0];
        const bool rightA=hand.primaryActive&&hand.primaryPressed,leftA=leftHand.primaryActive&&leftHand.primaryPressed;
        (void)leftA;   // the left A: the pad's B (primary above)
        const bool ray=gameMouse?spot.on:onButton,pull=hand.triggerActive&&hand.trigger>.5f,face=gameMouse&&rightA;   // B (the right B): the pad's
        const auto change=buttons.update(ray,pull,face);
        const auto keys=[&]{return WPARAM((buttons.left?MK_LBUTTON:0)|(buttons.right?MK_RBUTTON:0));};
        const bool held=buttons.left||buttons.right;
        auto shown=spot;
        // The game's window procedure (da6050) keeps the place of the last WM_MOUSEMOVE (window+d0) and makes its
        // button events (da19f0 / da1a70) at that place. On a move it also asks the system to tell it when the mouse
        // leaves (TrackMouseEvent); the real cursor is somewhere else, so WM_MOUSELEAVE follows every posted move.
        // Mock run game200021: a click posted a second after the last move did nothing (the craft tab stayed
        // closed). So the place is posted on every frame while the ray is on the plate, and once more right before
        // a button, which is then handled before that move's WM_MOUSELEAVE.
        if(ray){
            shown=slack.filter(spot,buttons.left);
            lastClient={shown.pixelX,shown.pixelY};
            if(gameMouse&&(!keepPad||buttons.left||leftPosted.downIn)){PostMessageW(h,WM_MOUSEMOVE,leftPosted.downIn?0:keys(),MAKELPARAM(lastClient.x,lastClient.y));++moves;postedAt=now;}   // before the button: no button in the move
        }else slack.filter(spot,false);
        const bool onOverlay=ray&&overlayButtonAt&&client.right>0&&client.bottom>0&&overlayButtonAt(float(shown.pixelX)/float(client.right),float(shown.pixelY)/float(client.bottom),open);
        if(change.leftDown&&onOverlay){overlayPress=true;if(overlayButtonPress)overlayButtonPress();logFile->event("menu_pointer_overlay_press","\"screen\":"+std::to_string(unsigned(open))+",\"frame\":"+std::to_string(f.frame));}
        if(overlayPress&&buttons.left&&!change.leftDown&&spot.on&&overlayButtonDrag&&client.right>0&&client.bottom>0)
            overlayButtonDrag(float(shown.pixelX)/float(client.right),float(shown.pixelY)/float(client.bottom));
        if(change.leftUp&&overlayPress&&overlayButtonRelease)overlayButtonRelease();
        // The right A on a button of the mod's own presses it as the trigger does (2026-10-24: the user pressed the right A on the
        // bug report's button eight times, log 20261024-130740 frames 8845-8923); nothing of it goes to the game.
        const bool rightOverlay=change.rightDown&&onOverlay&&!overlayPress;
        if(rightOverlay){overlayRight=true;if(overlayButtonPress)overlayButtonPress();logFile->event("menu_pointer_overlay_press","\"screen\":"+std::to_string(unsigned(open))+",\"by\":\"right_a\",\"frame\":"+std::to_string(f.frame));}
        const bool rightOverlayUp=change.rightUp&&overlayRight;
        if(rightOverlayUp){overlayRight=false;if(overlayButtonRelease)overlayButtonRelease();}
        if(!gameMouse){if(change.leftUp)overlayPress=false;}
        else if(h&&lastClient.x>=0){
            const auto place=MAKELPARAM(lastClient.x,lastClient.y);
            if(change.any()&&!ray){PostMessageW(h,WM_MOUSEMOVE,keys(),place);++moves;} // a release after the ray has gone
            // keepPad: the place goes first (posted above while the pull waits), the button after the lead frames, and its up
            // at least minimumDownFrames after its down (Posted above). The left button is the trigger, the right one the
            // right A (17:20).
            const auto run=[&](Posted& b,bool pressed,bool released,UINT downMessage,UINT upMessage,WPARAM mask){
                bool upNow=false;
                if(b.upIn&&--b.upIn==0)upNow=true;
                if(pressed){b=Posted{};
                    if(keepPad){PostMessageW(h,WM_MOUSEMOVE,0,place);++moves;b.downIn=leadFrames;mouseHeld=true;wantMouse=mouseFirst;postedAt=now;}
                    else{PostMessageW(h,downMessage,keys()|mask,place);++clicks;b.down=true;postedAt=now;}}
                // The place once more right before the button (2026-10-23, a player's report 124345: "the backpack's item menu
                // needs 3 to 17 presses of the right A"; replayed in Mock game-20261023-201858: presses 1 to 6 opened nothing, the
                // 7th did; with this every press opened it, -202434). The right button had its place only at the press, six frames
                // before its down; the left button has a move on every frame of its lead. Why the earlier place was not enough is
                // not known (no WM_MOUSELEAVE reached the game's window in Mock -204728).
                else if(b.downIn){if(--b.downIn==0){PostMessageW(h,WM_MOUSEMOVE,keys()&~mask,place);++moves;PostMessageW(h,downMessage,keys()|mask,place);++clicks;b.down=true;b.sinceDown=0;postedAt=now;
                    if(b.upLater){b.upLater=false;b.upIn=aHold&&mask==MK_RBUTTON?aHold:minimumDownFrames;}}}
                else if(b.down&&b.sinceDown<1000)++b.sinceDown;
                const unsigned least=aHold&&mask==MK_RBUTTON?aHold:minimumDownFrames;   // EVR_MENU_POINTER_A_HOLD (diagnostic)
                if(released){if(b.downIn)b.upLater=true;else if(b.down&&b.sinceDown<least)b.upIn=least-b.sinceDown;else upNow=true;}
                if(upNow&&b.down){PostMessageW(h,upMessage,keys()&~mask,place);++clicks;b=Posted{};postedAt=now;
                    if(!leftPosted.busy()&&!rightPosted.busy()&&!buttons.left&&!buttons.right){mouseHeld=false;wantMouse=false;}}
            };
            // the start menu: the right A is the left click at the pointed place (startMenuA above), when the trigger is not giving one
            const bool rightAsLeft=startMenuA&&change.rightDown&&ray&&!rightOverlay&&!buttons.left&&!leftPosted.busy();
            const bool leftPress=(change.leftDown&&!onOverlay&&!overlayPress)||rightAsLeft;
            if(change.leftDown&&onOverlay)leftPosted=Posted{};
            bool leftRelease=change.leftUp;if(change.leftUp&&overlayPress){overlayPress=false;leftRelease=false;}
            if(rightAsLeft){aAsLeft=true;logFile->event("menu_pointer_a_left_click","\"screen\":"+std::to_string(unsigned(open))+",\"pixel\":["+std::to_string(lastClient.x)+","+std::to_string(lastClient.y)+"],\"frame\":"+std::to_string(f.frame));}
            else if(change.rightUp&&aAsLeft){aAsLeft=false;leftRelease=true;}
            run(leftPosted,leftPress,leftRelease,WM_LBUTTONDOWN,WM_LBUTTONUP,MK_LBUTTON);
            // aPad 3: the backpack's tab; items: the screens with an item menu
            const bool items=open==4&&menuTab.load()==4;
            const bool padHere=aPad&&(aPad!=3||items);
            if((startMenuA||chestA)&&!padA.stage){}   // the start menu: the right A's left click above; the chests: the pad's A (primary below)
            else if(padA.stage||(padHere&&change.rightDown&&ray&&!rightOverlay&&!rightPosted.busy())){   // the right A as the pad's A on what is pointed at (aPad above); frames of this thread
                const bool left=padA.left;
                const UINT down=left?WM_LBUTTONDOWN:WM_RBUTTONDOWN,up=left?WM_LBUTTONUP:WM_RBUTTONUP;const WPARAM mk=left?MK_LBUTTON:MK_RBUTTON;
                auto stage=[&](unsigned s){const bool l=padA.left,o=padA.openAtPress,it=padA.items;padA={s,0,l,o,it};try{logFile->event("menu_pointer_pad_a","\"stage\":"+std::to_string(s)+",\"screen\":"+std::to_string(unsigned(open))+",\"left_button\":"+(l?"true":"false")+",\"item_menu_mark\":"+std::to_string(unsigned(itemMenuMark.load()))+",\"pixel\":["+std::to_string(lastClient.x)+","+std::to_string(lastClient.y)+"],\"device_mode\":"+std::to_string(unsigned(deviceMode.load()))+",\"frame\":"+std::to_string(f.frame));}catch(...){}};
                if(change.rightDown&&ray&&padHere&&!rightOverlay&&(padA.stage==0||padA.stage>=3)){PostMessageW(h,WM_MOUSEMOVE,0,place);++moves;mouseHeld=true;wantMouse=true;forcePad=false;postedAt=now;   // (not while its down waits for its up)
                    padA.items=items;padA.openAtPress=items&&itemMenuOpen.load();padA.left=aPad==2||(aPad==3&&padA.openAtPress);stage(1);}
                else if(padA.stage){++padA.frames;
                    // the up in the mouse mode but away from the plate (outside the window): a click is not made (Mock game-20261024-053135:
                    // an up in the pad mode was not taken - the game's button stayed down and the next down moved nothing)
                    if(padA.stage==1&&padA.frames>=leadFrames){PostMessageW(h,WM_MOUSEMOVE,0,place);++moves;PostMessageW(h,down,mk,place);++clicks;postedAt=now;stage(2);}
                    else if(padA.stage==2&&padA.frames>=2){const auto away=MAKELPARAM(-60,-60);PostMessageW(h,WM_MOUSEMOVE,mk,away);++moves;PostMessageW(h,up,0,away);++clicks;postedAt=now;stage(3);}
                    else if(padA.stage==3&&padA.frames>=2){wantMouse=false;forcePad=true;stage(4);}
                    else if(padA.stage==4&&padA.frames>=2){
                        // the menu opened by the button itself (an item chosen already): no pad's A, it would take the menu's top row
                        if(aPad==3&&padA.items&&!padA.openAtPress&&itemMenuOpen.load()){try{logFile->event("menu_pointer_pad_a_withheld","\"reason\":\"the item menu opened by the button\",\"frame\":"+std::to_string(f.frame));}catch(...){}}
                        else{padAUntil=GetTickCount64()+120;++padAPulses;}
                        stage(5);}
                    else if(padA.stage==5&&padA.frames>=20){forcePad=false;if(!leftPosted.busy()&&!buttons.left)mouseHeld=false;padA={};}}
            }
            else if(aMiddle)run(rightPosted,change.rightDown&&!rightOverlay,change.rightUp&&!rightOverlayUp,WM_MBUTTONDOWN,WM_MBUTTONUP,MK_MBUTTON);   // diagnostic: EVR_MENU_POINTER_A_BUTTON=middle
            else run(rightPosted,change.rightDown&&!rightOverlay,change.rightUp&&!rightOverlayUp,WM_RBUTTONDOWN,WM_RBUTTONUP,MK_RBUTTON);
            if(change.leftUp&&!leftPosted.busy()&&!rightPosted.busy()&&!buttons.right){mouseHeld=false;wantMouse=false;}
        }
        if(hoverMouse&&gameMouse)wantMouse=ray||leftPosted.busy()||rightPosted.busy();
        trigger=buttons.takesTrigger();primary=gameMouse&&buttons.takesSecondary()&&!(chestA&&!overlayRight&&!rightPosted.busy());secondary=false;   // the chests: the right A goes to the game as its A
        // The dot: where the cursor is (a held click keeps its place), larger and green while a button is down. Only
        // where it can press something - on the mod's own buttons (user 2026-10-18 12:40: "if the pointer cannot work the
        // menu, do not show it - it is confusing"; 11:00 to 12:40 it showed over the whole plate).
        const int dot=ray&&client.right>0?(held?2:1):0;   // with the game's mouse: wherever the ray meets the plate
        if(showCursor&&(dot||lastShown)){
            if(dot)showCursor(float(shown.pixelX)/float(client.right),float(shown.pixelY)/float(client.bottom),dot);else showCursor(0,0,0);
            lastShown=dot;
        }
        const int state=(ray?1:0)|(buttons.left?2:0)|(buttons.right?4:0)|(menu?8:0)|(int(open)<<8)|(int(deviceMode.load())<<16);
        if(state!=lastState||change.any()||(menu&&now>=lastLog+1000)){
            std::ostringstream o;o<<"\"screen\":"<<unsigned(open)<<",\"device_mode\":"<<unsigned(deviceMode.load())<<",\"menu\":"<<(menu?"true":"false")<<",\"hand_tracked\":"<<(tracked?"true":"false")
                <<",\"ray_on_plate\":"<<(ray?"true":"false")<<",\"pixel\":["<<shown.pixelX<<','<<shown.pixelY<<"],\"on_plate_m\":["<<spot.x<<','<<spot.y<<"],\"client\":["<<client.right<<','<<client.bottom
                <<"],\"left_button\":"<<(buttons.left?"true":"false")<<",\"right_button\":"<<(buttons.right?"true":"false")
                <<",\"left_down\":"<<(change.leftDown?"true":"false")<<",\"left_up\":"<<(change.leftUp?"true":"false")<<",\"right_down\":"<<(change.rightDown?"true":"false")<<",\"right_up\":"<<(change.rightUp?"true":"false")
                <<",\"trigger_taken\":"<<(trigger.load()?"true":"false")<<",\"secondary_taken\":"<<(secondary.load()?"true":"false")<<",\"dot\":"<<dot<<",\"plate_fixed\":"<<(plateFixed?"true":"false")
                <<",\"moves_posted\":"<<moves.load()<<",\"clicks_posted\":"<<clicks.load()<<",\"pad_kept\":"<<padKept.load()<<",\"frame\":"<<f.frame;
            logFile->event("menu_pointer_state",o.str());lastState=state;lastLog=now;
        }
    }catch(const std::exception& e){static std::atomic<unsigned> failures{0};if(failures.fetch_add(1)<3)logFile->event("menu_pointer_failure","\"reason\":"+jsonString(e.what()));}
}
inline void disable(){
    if(!installed)return;
    if(onDisable){const auto f=onDisable;onDisable=nullptr;f();}
    installed=false;trigger=false;secondary=false;primary=false;
    {std::lock_guard<std::mutex> lock(mutex);
     // A click that is still down gets its release before the pointer goes.
     if(window&&IsWindow(window)&&lastClient.x>=0){
        const auto place=MAKELPARAM(lastClient.x,lastClient.y);
        if(buttons.left)PostMessageW(window,WM_LBUTTONUP,0,place);
        if(buttons.right)PostMessageW(window,WM_RBUTTONUP,0,place);
     }
     buttons={};wantMouse=false;if(showCursor)showCursor(0,0,0);
     if(setPlate&&plateTold){setPlate(0,nullptr);plateTold=false;}plateFixed=false;}
    logFile->event("menu_pointer_removed","\"moves_posted\":"+std::to_string(moves.load())+",\"clicks_posted\":"+std::to_string(clicks.load()));
}
}
