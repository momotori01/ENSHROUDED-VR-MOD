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
inline std::atomic<unsigned> mouseModeSet{0};
inline unsigned downIn=0;                           // frames until the pull's button is posted (the place goes first)
inline unsigned rightDownIn=0;                      // the same for the right button (the left A, 2026-10-20)
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
// The right A and the left A are the mouse's left and right buttons while the ray is on the screen (2026-10-20, user: "the
// left click on the right A, the right click on the left A - most VR games confirm with the right A and cancel with the
// left A"); the pad's A and B (the left A's in menus, include/evr/live_pad.hpp) are not sent then.
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
        const bool ray=gameMouse?spot.on:onButton,pull=(hand.triggerActive&&hand.trigger>.5f)||rightA,face=gameMouse&&leftA;   // B (the right B): the pad's
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
            if(gameMouse&&(!keepPad||buttons.left||downIn)){PostMessageW(h,WM_MOUSEMOVE,downIn?0:keys(),MAKELPARAM(lastClient.x,lastClient.y));++moves;postedAt=now;}   // before the button: no button in the move
        }else slack.filter(spot,false);
        const bool onOverlay=ray&&overlayButtonAt&&client.right>0&&client.bottom>0&&overlayButtonAt(float(shown.pixelX)/float(client.right),float(shown.pixelY)/float(client.bottom),open);
        if(change.leftDown&&onOverlay){overlayPress=true;if(overlayButtonPress)overlayButtonPress();logFile->event("menu_pointer_overlay_press","\"screen\":"+std::to_string(unsigned(open))+",\"frame\":"+std::to_string(f.frame));}
        if(overlayPress&&buttons.left&&!change.leftDown&&spot.on&&overlayButtonDrag&&client.right>0&&client.bottom>0)
            overlayButtonDrag(float(shown.pixelX)/float(client.right),float(shown.pixelY)/float(client.bottom));
        if(change.leftUp&&overlayPress&&overlayButtonRelease)overlayButtonRelease();
        if(!gameMouse){if(change.leftUp)overlayPress=false;}
        else if(h&&lastClient.x>=0){
            const auto place=MAKELPARAM(lastClient.x,lastClient.y);
            if(change.any()&&!ray){PostMessageW(h,WM_MOUSEMOVE,keys(),place);++moves;} // a release after the ray has gone
            // keepPad: the place goes first (posted above while the pull waits), the button two frames later
            if(change.leftDown&&!overlayPress){
                if(keepPad){if(!ray){PostMessageW(h,WM_MOUSEMOVE,0,place);++moves;}downIn=leadFrames;mouseHeld=true;wantMouse=mouseFirst;postedAt=now;}
                else{PostMessageW(h,WM_LBUTTONDOWN,keys(),place);++clicks;postedAt=now;}}
            else if(downIn&&--downIn==0){PostMessageW(h,WM_LBUTTONDOWN,MK_LBUTTON,place);++clicks;postedAt=now;}
            if(change.leftUp){if(overlayPress)overlayPress=false;else{
                if(downIn){downIn=0;PostMessageW(h,WM_LBUTTONDOWN,MK_LBUTTON,place);++clicks;}   // let go before its button went: a whole click
                PostMessageW(h,WM_LBUTTONUP,keys(),place);++clicks;postedAt=now;}
                mouseHeld=false;wantMouse=false;}
            // The right button (the left A since 2026-10-20) goes as the left one: the place first and the game's mouse mode,
            // held while it is down, and the pad's mode back 0.25 s after it (input_ui_observer.hpp waits on postedAt and
            // mouseHeld). Headset run 20261020-125439: posted alone, it left the game in its mouse mode for the rest of the
            // run - the keyboard's prompts in play and the pad's menu actions (splitting a stack) not taken.
            if(change.rightDown){
                if(keepPad){PostMessageW(h,WM_MOUSEMOVE,0,place);++moves;rightDownIn=leadFrames;mouseHeld=true;wantMouse=mouseFirst;postedAt=now;}
                else{PostMessageW(h,WM_RBUTTONDOWN,keys(),place);++clicks;postedAt=now;}}
            else if(rightDownIn&&--rightDownIn==0){PostMessageW(h,WM_RBUTTONDOWN,MK_RBUTTON,place);++clicks;postedAt=now;}
            if(change.rightUp){
                if(rightDownIn){rightDownIn=0;PostMessageW(h,WM_RBUTTONDOWN,MK_RBUTTON,place);++clicks;}
                PostMessageW(h,WM_RBUTTONUP,keys(),place);++clicks;postedAt=now;
                if(!buttons.left){mouseHeld=false;wantMouse=false;}}
        }
        trigger=buttons.takesTrigger();primary=buttons.takesTrigger();secondary=gameMouse&&buttons.takesSecondary();
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
