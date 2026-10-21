#pragma once
// Who asks for the stick-press pictures (2026-10-21, the user: "the quick slot change shows RB + R3 - right, but the press
// of the hard one; there only, show the left stick's press": the mod takes R3 from either stick's press while RB is held).
// The R3 picture is one image for every prompt (Mock game-20261021-142914), so only its choice can differ for that one
// prompt. The game picks it in 0xd1ad80 (button number -> the UiTexture; one caller, 0xcec1c4 in 0xcebdc0) and in
// 0xcff7b0 (button number -> "<img xbox_...>" in text; one caller, 0xb1afcb) - the prompt is told apart only further up.
// EVR_GLYPH_CHOICE_OBSERVE=1 records each distinct call stack (game RVAs) that asks for L3 (13), RB (11) or R3 (14) by
// either route (glyph_choice). Mock game-20261021-154551: the quick slot change ("RB + R3", beside the action bar, in play
// and in the backpack) is the widget route's two-button prompt - RB from 0xceccb4, then R3 from 0xcece52, both under
// 0xcf1925 -, while the R3 of the first-person switch and of the tutorial come from elsewhere (0xb2e189). There the R3
// is handed the Switch's BACK picture instead (never shown with the Xbox buttons), which the Vulkan layer fills with the
// game's own left stick press (button_glyph_labels.hpp originalLeftPress). Xbox button style only (the R3 picture checked
// against the bundle). While VR input runs; EVR_QUICKSLOT_LS_PRESS=0: off.
namespace glyphChoice {
using Pick=uintptr_t(__fastcall*)(uintptr_t,uintptr_t,uintptr_t,uintptr_t);
inline Pick originalTexture=nullptr,originalText=nullptr;inline bool enabled=false;
inline bool observe=false,swap=false;inline std::atomic<unsigned> swapped{0};
constexpr uintptr_t comboRva=0xcf1925,comboFirstRva=0xceccb4,comboSecondRva=0xcece52;   // return addresses (see above)
constexpr uintptr_t xboxRightPress=0xc740,switchBack=0xccb0;                              // FbUiBundle offsets of the UiTextures
constexpr uintptr_t textureRva=0xd1ad80,textRva=0xcff7b0;
constexpr const char* texturePattern="66 89 4C 24 08 53 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 18";
constexpr const char* textPattern="40 55 56 48 8D 6C 24 B1 48 81 EC A8 00 00 00 0F B7 C2 48 8B";
inline std::mutex seenMutex;inline std::vector<uint64_t> seen;inline std::atomic<unsigned> logged{0},inside{0};
struct Inside {Inside(){++inside;}~Inside(){--inside;}};
inline void note(const char* route,unsigned button){
    if(button!=8&&button!=10&&button!=11&&button!=13&&button!=14)return;
    if(logged.load()>=120)return;
    void* stack[14]{};const auto n=RtlCaptureStackBackTrace(2,14,stack,nullptr);
    uint64_t h=1469598103934665603ull^button^(route[1]=='e'?7:3);std::string rvas;
    for(USHORT i=0;i<n;++i){const auto a=reinterpret_cast<uintptr_t>(stack[i]);const auto rva=a>=baseAddress&&a<baseAddress+0x4000000?a-baseAddress:0;
        h=(h^rva)*1099511628211ull;rvas+=(i?",\"":"\"")+hex(rva)+"\"";}
    {std::lock_guard<std::mutex> lock(seenMutex);if(std::find(seen.begin(),seen.end(),h)!=seen.end())return;seen.push_back(h);}
    ++logged;const ObserverLastError preserve;
    try{logFile->event("glyph_choice","\"route\":\""+std::string(route)+"\",\"button\":"+std::to_string(button)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"stack\":["+rvas+"]");}catch(...){}
}
// The bundle the pictures are taken from: 0xd1ad80 loads [[global]+0xc8] (its "mov rax,[rip+disp]" at +0x15).
inline uintptr_t bundle(){
    const auto at=baseAddress+textureRva+0x15;const auto op=read<std::array<uint8_t,3>>(at);
    if(op!=std::array<uint8_t,3>{0x48,0x8b,0x05})return 0;
    const auto global=at+7+read<int32_t>(at+3);const auto holder=read<uintptr_t>(global);return holder?read<uintptr_t>(holder+0xc8):0;
}
inline thread_local uint64_t comboFirstAt=0;
__declspec(noinline) uintptr_t __fastcall texture(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){
    const Inside counted;const unsigned button=unsigned(a&0xffff);
    if(observe)try{note("texture",button);}catch(...){}
    auto result=originalTexture(a,b,c,d);
    if(swap&&(button==11||button==14))try{
        void* st[3]{};const auto n=RtlCaptureStackBackTrace(1,3,st,nullptr);
        const auto rva=[&](int i){const auto v=reinterpret_cast<uintptr_t>(st[i]);return v>baseAddress?v-baseAddress:0;};
        const bool combo=n>=3&&rva(2)==comboRva;const auto now=GetTickCount64();
        if(button==11)comboFirstAt=combo&&rva(1)==comboFirstRva?now:0;
        else if(combo&&rva(1)==comboSecondRva&&comboFirstAt&&now-comboFirstAt<=50){
            comboFirstAt=0;const auto b0=bundle();
            if(b0&&result==b0+xboxRightPress){result=b0+switchBack;const auto k=++swapped;
                if(k<=3||k%5000==0)logFile->event("quick_slot_ls_press","\"count\":"+std::to_string(k)+",\"frame\":"+std::to_string(liveFrame.frame));}}
    }catch(...){}
    return result;
}
__declspec(noinline) uintptr_t __fastcall text(uintptr_t a,uintptr_t b,uintptr_t c,uintptr_t d){
    const Inside counted;if(observe)try{note("text",unsigned(b&0xffff));}catch(...){}
    return originalText(a,b,c,d);
}
inline void install(){
    {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_GLYPH_CHOICE_OBSERVE",v,sizeof v);observe=n==1&&v[0]=='1';}
    {char v[8]{};const auto n=GetEnvironmentVariableA("EVR_QUICKSLOT_LS_PRESS",v,sizeof v);swap=controllerUiFixture::liveRequested&&!(n==1&&v[0]=='0');}
    if(!observe&&!swap)return;
    const auto hook=[](uintptr_t rva,const char* pattern,void* detour,Pick* original){
        const auto bytes=parsePattern(pattern);const auto site=baseAddress+rva;
        if(scan(reinterpret_cast<const uint8_t*>(site),bytes.size(),bytes)!=std::vector<size_t>{0})throw std::runtime_error("glyph choice signature changed at "+hex(rva));
        if(MH_CreateHook(reinterpret_cast<void*>(site),detour,reinterpret_cast<void**>(original))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(site))!=MH_OK)throw std::runtime_error("glyph choice hook failed");};
    hook(textureRva,texturePattern,reinterpret_cast<void*>(&texture),&originalTexture);
    hook(textRva,textPattern,reinterpret_cast<void*>(&text),&originalText);
    enabled=true;logFile->event("glyph_choice_enabled","\"texture_rva\":"+hex(textureRva)+",\"text_rva\":"+hex(textRva)+",\"observe\":"+(observe?"true":"false")+",\"quick_slot_ls_press\":"+(swap?"true":"false"));
}
inline void disable() noexcept {
    if(!enabled)return;enabled=false;
    MH_DisableHook(reinterpret_cast<void*>(baseAddress+textureRva));MH_DisableHook(reinterpret_cast<void*>(baseAddress+textRva));
    const auto deadline=GetTickCount64()+500;while(inside.load()&&GetTickCount64()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    try{logFile->event("glyph_choice_removed","\"stacks\":"+std::to_string(logged.load())+",\"swapped\":"+std::to_string(swapped.load()));}catch(...){}
}
}
