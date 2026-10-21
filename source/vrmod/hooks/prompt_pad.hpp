#pragma once
// The game's "a gamepad is in use" (0xa3f7c0: the byte at [[rcx+0x10]+1]; 282 callers, among them the choice of the
// device whose bindings a prompt shows, 0xd1be20 - docs/analysis/vr_button_glyphs_20261021). The menu pointer's mouse
// messages turn the prompts to the keyboard's and they stay so after the mod has put the device mode back to the pad's
// (Mock game-20261021-143556: 0.5 s after it, the backpack's footer still showed ESC and the mouse; the 16 bytes at the
// device, the local device and the UI mode did not change with it). 2026-10-21, the user: "keep the VR button pictures
// while the pointer is on the UI". The byte (one place all run) is 1 after the game's own pad input and 0 after a mouse
// message; it is 0 at the start, so the play's prompts were the keyboard's (E, TAB, J) until a pad button came, and 0
// again after each pointer click (Mock game-20261021-144128, observed: answers 0 -> 1 at the backpack's opening -> 0 at
// the first click). With the answer always "a gamepad" (Mock game-20261021-144128): the play showed Y / LT / R3, the
// backpack kept A / X / Y / B / LB / RB after the drag, and the pointer still dragged the item. While VR input runs only.
// EVR_PROMPT_PAD: 0 off (not hooked), 1 observed only (each change of the answer or the byte it reads goes on record,
// prompt_pad_answer), 2 (default) the answer is always "a gamepad".
namespace promptPad {
using IsPad=uint8_t(__fastcall*)(void*);
inline IsPad original=nullptr;inline bool enabled=false;
constexpr uintptr_t rva=0xa3f7c0;
constexpr const char* pattern="48 8B 41 10 0F B6 40 01 C3";
inline const int mode=[]{char v[4]{};return GetEnvironmentVariableA("EVR_PROMPT_PAD",v,sizeof v)==1&&v[0]>='0'&&v[0]<='2'?v[0]-'0':2;}();
inline std::atomic<unsigned> logged{0},forced{0};inline std::atomic<uint64_t> calls{0},inside{0};
inline std::atomic<uintptr_t> lastPlace{0};inline std::atomic<int> lastAnswer{-1};
struct Inside {Inside(){++inside;}~Inside(){--inside;}};
__declspec(noinline) uint8_t __fastcall isPad(void* self){
    const Inside counted;++calls;
    const auto answer=original(self);
    // about 23000 calls a second: the byte's place is read only when the answer changed and a record is due
    if(int(answer)!=lastAnswer.load()&&logged.load()<300)try{
        const auto place=read<uintptr_t>(reinterpret_cast<uintptr_t>(self)+0x10)+1;
        {
            lastPlace=place;lastAnswer=int(answer);const ObserverLastError preserve;++logged;
            logFile->event("prompt_pad_answer","\"answer\":"+std::to_string(answer)+",\"place\":"+hex(place)+",\"self\":"+hex(reinterpret_cast<uintptr_t>(self))+
                ",\"caller\":"+hex(reinterpret_cast<uintptr_t>(_ReturnAddress())-baseAddress)+",\"mode\":"+std::to_string(mode)+",\"frame\":"+std::to_string(liveFrame.frame));}
    }catch(...){}
    if(mode==2&&!answer){++forced;return 1;}
    return answer;
}
inline void install(){
    if(!mode||!controllerUiFixture::liveRequested)return;
    const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva;
    if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("prompt pad signature changed");
    if(MH_CreateHook(reinterpret_cast<void*>(site),reinterpret_cast<void*>(&isPad),reinterpret_cast<void**>(&original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)
        throw std::runtime_error("prompt pad hook failed");
    enabled=true;logFile->event("prompt_pad_enabled","\"rva\":"+hex(rva)+",\"mode\":"+std::to_string(mode));
}
inline void disable() noexcept {
    if(!enabled)return;enabled=false;
    const auto status=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rva));
    const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    try{logFile->event("prompt_pad_removed","\"status\":"+std::to_string(status)+",\"calls\":"+std::to_string(calls.load())+",\"forced\":"+std::to_string(forced.load()));}catch(...){}
}
}
