#pragma once
// The game's ammunition list beside the crosshair, drawn or not by the UI itself (2026-10-26, user: "not cutting - can't the UI side
// hide it, never handing it to the UI? Cutting always goes wrong somewhere" and "the arrow choices still show in the old UI for about
// a second after a shot"). The HUD's crosshair function (0xbd5b70, build 26082781) draws the list's rows with two functions of one
// shape - void(row, float strength); the row's +0x102 marks the chosen one:
//   0xb391d0 (uses namedStyles.hud.ammoListEntry, bundle +0x11824, through 0xb39a40), called at 0xbd6c24 (the loop over the list's
//            rows, its x from spellsListOffsetX), 0xbd6d9c and 0xbd73a3;
//   0xcc8830 (the same style, twice, with the font sizes at bundle +0x109ec/+0x109f0), called at 0xbd695c and 0xbd73b1, and at
//            0xca2064 from another function (0xca1ee0).
// Which branch is which (the crosshair kind at +0x1db1 of the HUD's state, its jump table at 0xbd7514): 4, 5, 7 and 8 take the staff's
// cursors (directional, meteor, single target, beam: bundle +0xd810..+0xd880) and the spells' x (spellsListOffsetX, +0xd890); with
// the weapon kind +0x1cda 4 the rows are the list's (the loop, 0xbd6c24) - the staff's spells. Any other weapon goes the other way
// (0xbd6c54 on: its ammunition from +0x628/+0x630 of another state, not the spells' list) - the bow's arrows, presumably: the arrows
// are never in the spells' list (game_ammo_list.hpp, headset run 20261026-135621) and spellsListOffsetX did not move them.
// Not drawn (2026-10-26): every row of the crosshair function while the mod asks (spell_panel.hpp: a staff in the hands - the mod draws
// its own list; a bow - only for a while after its arrow changes, user: "only on a change input, as the spells"), and for 2 s after
// the last ask. The other function's call (0xca2064) is left alone. Each call is counted by its return address (the site) and a line a
// second logged. EVR_AMMO_DRAW_SKIP=<hex mask of the sites below>: those sites' rows never drawn (a diagnostic for Mock).
namespace ammoListDraw {
constexpr uintptr_t rowRva=0xb391d0,currentRva=0xcc8830;
constexpr const char* rowPattern="48 8B C4 55 57 48 8D A8 88 FD FF FF 48 81 EC 68 03 00 00 80 B9 02 01 00 00 00";
constexpr const char* currentPattern="48 8B C4 55 53 56 57 41 54 41 56 41 57 48 8D A8 48 FD FF FF 48 81 EC 80 03 00 00";
// The calls (rva of the call instruction) and their sites' names; a return address of another place counts as site 7.
struct Site {uintptr_t call;uintptr_t target;const char* name;};
constexpr std::array<Site,6> sites{{{0xbd6c24,rowRva,"row_list_loop"},{0xbd6d9c,rowRva,"row_single"},{0xbd73a3,rowRva,"row_late"},
    {0xbd695c,currentRva,"current_case"},{0xbd73b1,currentRva,"current_late"},{0xca2064,currentRva,"current_other_function"}}};
constexpr size_t otherSite=7;
using Draw=void(__fastcall*)(void*,float);
inline Draw originalRow=nullptr,originalCurrent=nullptr;inline bool enabled=false;
inline std::array<std::atomic<uint64_t>,8> calls{},skippedCalls{};
inline std::array<std::atomic<uint32_t>,8> lastItem{};inline std::array<std::atomic<uint32_t>,8> lastChosen{};inline std::array<std::atomic<float>,8> lastStrength{};
inline std::atomic<uint32_t> skipMask{0};   // sites (bits) whose rows are never drawn (the diagnostic)
// The mod's ask (spell_panel.hpp): rows of the crosshair function (sites 0..4) not drawn while hiddenAskedAt is within 2 s, unless
// before shownUntil.
inline std::atomic<uint64_t> hiddenAskedAt{0},shownUntil{0};
constexpr uint32_t crosshairSites=0x1f;
// EVR_AMMO_DRAW_HIDE_AFTER=<XR frame>: the ask is followed only from that frame on (a diagnostic for Mock: the game's rows before, none after).
inline const uint64_t hideAfter=[]{char v[24]{};const auto n=GetEnvironmentVariableA("EVR_AMMO_DRAW_HIDE_AFTER",v,sizeof v);return n&&n<sizeof v?uint64_t(std::strtoull(v,nullptr,10)):0ull;}();
inline std::atomic<bool> askFollowed{hideAfter==0};
inline bool hiddenNow(size_t site){
    if(skipMask.load(std::memory_order_relaxed)&(1u<<site))return true;
    if(!((crosshairSites>>site)&1)||!askFollowed.load(std::memory_order_relaxed))return false;
    const auto asked=hiddenAskedAt.load(std::memory_order_relaxed);if(!asked)return false;
    const auto now=GetTickCount64();return now<=asked+2000&&now>=shownUntil.load(std::memory_order_relaxed);
}
inline uint32_t requestedSkip=[]{char v[16]{};const auto n=GetEnvironmentVariableA("EVR_AMMO_DRAW_SKIP",v,sizeof v);return n&&n<sizeof v?uint32_t(std::strtoul(v,nullptr,16)):0u;}();
inline size_t siteOf(uintptr_t returnAddress){
    for(size_t i=0;i<sites.size();++i)if(returnAddress==baseAddress+sites[i].call+5)return i;
    return otherSite;
}
inline void note(size_t site,void* row,float strength){
    calls[site].fetch_add(1,std::memory_order_relaxed);lastStrength[site].store(strength,std::memory_order_relaxed);
    try{const auto r=reinterpret_cast<uintptr_t>(row);lastItem[site].store(read<uint32_t>(r),std::memory_order_relaxed);lastChosen[site].store(read<uint8_t>(r+0x102),std::memory_order_relaxed);}catch(...){}
}
inline void __fastcall drawRow(void* row,float strength){
    const auto site=siteOf(reinterpret_cast<uintptr_t>(_ReturnAddress()));note(site,row,strength);
    if(hiddenNow(site)){skippedCalls[site].fetch_add(1,std::memory_order_relaxed);return;}
    originalRow(row,strength);
}
inline void __fastcall drawCurrent(void* row,float strength){
    const auto site=siteOf(reinterpret_cast<uintptr_t>(_ReturnAddress()));note(site,row,strength);
    if(hiddenNow(site)){skippedCalls[site].fetch_add(1,std::memory_order_relaxed);return;}
    originalCurrent(row,strength);
}
// EVR_AMMO_DRAW=0 in the game's environment: no hook (the game draws its list; the staff's falls back to ui_cursor_config.hpp).
inline bool wanted(){char v[8]{};const auto n=GetEnvironmentVariableA("EVR_AMMO_DRAW",v,sizeof v);return !(n==1&&v[0]=='0');}
inline void install(){
    if(enabled||!wanted())return;
    const auto check=[](uintptr_t rva,const char* pattern){const auto bytes=parsePattern(pattern);
        return scan(reinterpret_cast<const uint8_t*>(baseAddress+rva),bytes.size(),bytes)==std::vector<size_t>{0};};
    if(!check(rowRva,rowPattern)||!check(currentRva,currentPattern))throw std::runtime_error("ammo list draw signature mismatch");
    for(const auto& s:sites){const auto call=baseAddress+s.call;
        if(read<uint8_t>(call)!=0xe8||uintptr_t(int64_t(call+5)+read<int32_t>(call+1))!=baseAddress+s.target)throw std::runtime_error("ammo list draw caller mismatch");}
    if(MH_CreateHook(reinterpret_cast<void*>(baseAddress+rowRva),reinterpret_cast<void*>(&drawRow),reinterpret_cast<void**>(&originalRow))!=MH_OK||
       MH_CreateHook(reinterpret_cast<void*>(baseAddress+currentRva),reinterpret_cast<void*>(&drawCurrent),reinterpret_cast<void**>(&originalCurrent))!=MH_OK||
       MH_EnableHook(reinterpret_cast<void*>(baseAddress+rowRva))!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(baseAddress+currentRva))!=MH_OK)
        throw std::runtime_error("ammo list draw hook failed");
    skipMask=requestedSkip;enabled=true;
    logFile->event("ammo_list_draw_hook_enabled","\"row_rva\":"+hex(rowRva)+",\"current_rva\":"+hex(currentRva)+",\"skip_mask\":"+hex(requestedSkip));
}
// Once per XR frame (view_resource_probe.cpp): a line a second while rows are drawn - the calls per site since the last line.
inline std::array<uint64_t,8> loggedCalls{};inline uint64_t lastLogAt=0;inline unsigned logs=0;
inline void frame(uint64_t now){
    if(hideAfter&&!askFollowed.load(std::memory_order_relaxed)&&liveFrame.frame>=hideAfter){askFollowed=true;logFile->event("ammo_list_draw_ask_followed","\"frame\":"+std::to_string(liveFrame.frame));}
    if(!enabled||now<lastLogAt+1000)return;
    lastLogAt=now;std::ostringstream o;bool any=false;o<<std::setprecision(4)<<"\"sites\":{";
    for(size_t i=0;i<calls.size();++i){const auto c=calls[i].load(std::memory_order_relaxed);const auto d=c-loggedCalls[i];loggedCalls[i]=c;if(!d)continue;
        const auto item=lastItem[i].load(std::memory_order_relaxed);const auto* name=::evr::ammo::find(item);
        o<<(any?",":"")<<'"'<<(i<sites.size()?sites[i].name:"other")<<"\":{\"calls\":"<<d<<",\"item\":"<<item<<",\"kind\":"<<(name?int(name->kind):-1)<<",\"chosen\":"<<lastChosen[i].load(std::memory_order_relaxed)
         <<",\"strength\":"<<lastStrength[i].load(std::memory_order_relaxed)<<",\"skipped\":"<<skippedCalls[i].load(std::memory_order_relaxed)<<'}';any=true;}
    o<<'}';
    if(any&&logs<2000){++logs;logFile->event("ammo_list_draw",o.str());}
}
inline void uninstall() noexcept {
    if(!enabled)return;
    const auto a=MH_DisableHook(reinterpret_cast<void*>(baseAddress+rowRva)),b=MH_DisableHook(reinterpret_cast<void*>(baseAddress+currentRva));
    if(a==MH_OK)MH_RemoveHook(reinterpret_cast<void*>(baseAddress+rowRva));
    if(b==MH_OK)MH_RemoveHook(reinterpret_cast<void*>(baseAddress+currentRva));
    if(a==MH_OK&&b==MH_OK)enabled=false;
    try{logFile->event("ammo_list_draw_hook_removed","\"status\":["+std::to_string(int(a))+','+std::to_string(int(b))+"]");}catch(...){}
}
}
