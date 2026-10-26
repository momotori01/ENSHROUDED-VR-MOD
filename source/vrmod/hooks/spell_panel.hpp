#pragma once
// The staff's spell list and the bow's arrow list beside the aim mark, drawn by the mod as the game draws it (2026-10-26; user: "cutting the UI always goes
// wrong somewhere - take the spell switching out of the UI and draw what is needed at the reticle"; after the first try in a font of
// Windows, always showing the spell in use: "the icons and the letters shape the mood - use the game's own font; the game shows the
// list only when a spell is chosen"). What it shows: the game's own list (game_ammo_list.hpp: the spells it offers, in its order,
// with their counts) and the spell in use (equipment slot 15, live_equipment.hpp); painted with the game's own font files and icons,
// read from the game's files (include/evr/ammo_panel_paint.hpp, include/evr/game_content.hpp), in the game's language (Steam's).
// When: only after the spell in use changes - in over 0.125 s, 3 s shown, out over 1 s (the HUD's contextualVisibility:
// changeTriggerFadeInDuration, postChangeLingerDuration, postLingerFadeOutDuration). Where: the layer (evrSetAimPanel,
// layer/native_xr.hpp) draws it into both eyes at the aim mark's depth, its left edge and size as the game's list stood round the
// crosshair on the HUD plate (100 pixels of a 1080 rows high HUD right of it; the plate 2 m wide at 1.5 m: 25.1 pixels a degree).
// The game's own list is not drawn meanwhile (ammo_list_draw.hpp; without it moved off its picture, ui_cursor_config.hpp keep). Painting (and loading the fonts the first
// time) runs on a thread of its own. EVR_SPELL_PANEL=0: neither.
// (<evr/ammo_names.hpp>, <evr/ammo_panel_paint.hpp>: view_resource_probe.cpp, at the global scope)
namespace spellPanel {
using PanelFn=int(*)(const uint8_t*,uint32_t,uint32_t,float,float,float,float);
inline PanelFn setPanel=nullptr;inline bool resolved=false;
inline bool enabled(){static const bool on=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_SPELL_PANEL",v,sizeof v);return !(n==1&&v[0]=='0');}();return on;}
constexpr float hudPixelsPerDegree=25.1f;constexpr uint64_t fadeInMs=125,lingerMs=3000,fadeOutMs=1000;
struct Look {std::vector<evr::ammopanel::Row> rows;size_t chosen=0;
    bool operator==(const Look& o)const{if(chosen!=o.chosen||rows.size()!=o.rows.size())return false;
        for(size_t i=0;i<rows.size();++i)if(rows[i].item!=o.rows[i].item||rows[i].count!=o.rows[i].count||rows[i].eternal!=o.rows[i].eternal)return false;return true;}};
// Steam's game language (as hooks/bug_report.hpp), kept once read.
inline unsigned language(){
    static unsigned index=[]{
        if(const auto steam=GetModuleHandleW(L"steam_api64.dll")){
            using AppsFn=void*(*)();using LanguageFn=const char*(*)(void*);
            auto apps=reinterpret_cast<AppsFn>(GetProcAddress(steam,"SteamAPI_SteamApps_v009"));if(!apps)apps=reinterpret_cast<AppsFn>(GetProcAddress(steam,"SteamAPI_SteamApps_v008"));
            const auto current=reinterpret_cast<LanguageFn>(GetProcAddress(steam,"SteamAPI_ISteamApps_GetCurrentGameLanguage"));
            if(apps&&current)try{if(void* a=apps())if(const char* l=current(a);l&&*l)return evr::ammo::languageIndex(l);}catch(...){}
        }
        return PRIMARYLANGID(GetUserDefaultUILanguage())==LANG_JAPANESE?evr::ammo::languageIndex("japanese"):0u;}();
    return index;
}
// The painter: one thread, the latest look asked for, the picture of the last one painted.
struct Painter {
    std::thread worker;std::mutex lock;std::condition_variable wake;bool stop=false;
    Look asked,painted;uint64_t askedSerial=0,paintedSerial=0;evr::ammopanel::Picture picture;std::unique_ptr<evr::ammopanel::Assets> assets;std::string failure;
    void run(){
        for(;;){
            Look look;uint64_t serial=0;
            {std::unique_lock<std::mutex> l(lock);wake.wait(l,[&]{return stop||askedSerial!=paintedSerial;});if(stop)return;look=asked;serial=askedSerial;}
            if(!assets)assets=std::make_unique<evr::ammopanel::Assets>(evr::content::Index::gameFolder());
            const auto began=GetTickCount64();auto p=evr::ammopanel::paint(*assets,look.rows,look.chosen,language());const auto ms=GetTickCount64()-began;
            uint32_t w=p.width,h=p.height;const bool fonts=p.fonts;
            {std::lock_guard<std::mutex> l(lock);picture=std::move(p);painted=look;paintedSerial=serial;failure=assets->failure();}
            static unsigned logs=0;if(logs++<200){std::ostringstream o;o<<"\"rows\":"<<look.rows.size()<<",\"chosen\":"<<look.chosen<<",\"ms\":"<<ms<<",\"size\":["<<w<<','<<h
                <<"],\"fonts\":"<<(fonts?"true":"false")<<",\"failure\":"<<jsonString(assets->failure());logFile->event("spell_panel_painted",o.str());}
        }
    }
    void ask(const Look& look){
        std::lock_guard<std::mutex> l(lock);if(askedSerial&&look==asked)return;asked=look;++askedSerial;
        if(!worker.joinable()){stop=false;worker=std::thread([this]{run();});}
        wake.notify_all();
    }
    void finish(){{std::lock_guard<std::mutex> l(lock);stop=true;}wake.notify_all();if(worker.joinable())worker.join();
        std::lock_guard<std::mutex> l(lock);askedSerial=paintedSerial=0;asked=painted={};picture={};}
};
inline Painter painter;
// The game's own list kept off the HUD: not drawn at all (ammo_list_draw.hpp, the UI's own drawing skipped - 2026-10-26, user: "not
// cutting: can't the UI side hide it, never handing it to the UI?"), or - without that hook - moved off its picture (ui_cursor_config.hpp).
inline void askHidden(uint64_t now){if(ammoListDraw::enabled)ammoListDraw::hiddenAskedAt=now;else uiCursorConfig::hiddenAskedAt=now;}
inline void showGameList(uint64_t until){if(ammoListDraw::enabled)ammoListDraw::shownUntil=until;else uiCursorConfig::shownUntil=until;}
inline uint64_t shownAt=0,sentSerial=0;inline uint32_t lastSelected=0;inline bool sentShown=false;inline unsigned logs=0;
inline float sentDegrees=1,sentAnchor=.5f;
// Hidden: strength 0. While a staff is in the hands this goes every frame - the layer then keeps the HUD whole round the aim (no
// cluster) whether the list shows or not; once more when the staff leaves the hands, then nothing (the cluster comes back).
inline void hide(bool keep){if((keep||sentShown)&&setPanel)setPanel(nullptr,0,0,sentDegrees,sentAnchor,evr::ammopanel::leftOfPicture/hudPixelsPerDegree,0);sentShown=keep;}
// Every frame of the play screen (view_resource_probe.cpp, beside the aim mark): rangedItem the ranged weapon in the hands,
// markShown whether the aim mark is shown this frame.
inline void frame(uint32_t rangedItem,bool markShown,uint64_t now){
    if(!enabled())return;
    if(!resolved){resolved=true;if(auto layer=GetModuleHandleW(L"evr_diagnostic.dll"))setPanel=reinterpret_cast<PanelFn>(GetProcAddress(layer,"evrSetAimPanel"));
        logFile->event("spell_panel_enabled","\"layer\":"+std::string(setPanel?"true":"false")+",\"language\":"+jsonString(evr::ammo::languages[language()]));}
    if(!setPanel)return;
    // What is in the hands: a staff (its spells; the spell in use in equipment slot 15) or a bow (its arrows, slot 14; the mod's bow
    // from the shoulder too) - 2026-10-26, user after the headset: "do the bow's the same way". The game's own list off the picture
    // meanwhile (ui_cursor_config.hpp keep; back 2 s after), and the layer keeps the HUD whole round the aim (no cluster).
    // (the action bar's selected item, as the hand holds it, through the game data's list of staffs and bows: Mock 20261026-132025 -
    // the game's bow from the shoulder had neither the mod's bow nor a bow mark)
    uint32_t inHand=0;if(controllerUiFixture::handItem){const auto h=controllerUiFixture::handItem(now);if(h.valid)inHand=h.item;}
    const uint8_t heldKind=evr::ammo::launcherKind(inHand);
    const bool bowOut=controllerUiFixture::modBowItem.load()!=0||aimMark::kindNow==1;   // the shoulder's bow over a staff in the hand
    const bool staffInHands=!bowOut&&((rangedItem&&evr::motion::rangedAimStaff(rangedItem))||heldKind==1);
    const bool bowInHands=bowOut||(!staffInHands&&heldKind==2);
    // The bow's arrows: off unless EVR_ARROW_PANEL=1 (2026-10-26) - most arrows have no icon picture in the game data (ItemInfo
    // iconImage none for 36 of 46: the game draws their icons from the arrow's model at run time, iconModel / iconScene), so the
    // list could not look as the game's (user: "the icons and the letters shape the mood"; else "the cutting way"). The bow keeps
    // the game's own list, carried with the aim by the layer's cluster as before.
    static const bool arrows=[]{char v[8]{};return GetEnvironmentVariableA("EVR_ARROW_PANEL",v,sizeof v)==1&&v[0]=='1';}();
    const uint8_t kind=staffInHands?1:(bowInHands&&arrows)?2:0;
    static uint8_t lastKind=0;if(kind!=lastKind){lastKind=kind;lastSelected=0;shownAt=0;}
    if(kind)askHidden(now);
    // The bow without the mod's list (the default): the game's own arrow list, carried with the aim by the cluster, only for a while
    // after the arrow in use (equipment slot 14) changes - off its picture else, so not at every nock nor after a shot (2026-10-26,
    // user: "after a shot the arrow's kind shows in the old UI"; "it shows at every nock - only on a change input, as the spells").
    // 4.5 s: the HUD's 3 s and 1 s fade, and its first 0.125 s.
    if(bowInHands&&!arrows){
        const auto st=liveEquipment::state(now);static uint32_t lastArrow=0;static uint64_t arrowAt=0;
        askHidden(now);
        if(st.valid){const uint32_t arrow=st.arrow.stack.item;
            if(arrow!=lastArrow){if(lastArrow&&arrow)arrowAt=now;lastArrow=arrow;
                if(logs<300){++logs;logFile->event("arrow_list_chosen","\"frame\":"+std::to_string(liveFrame.frame)+",\"arrow\":"+std::to_string(arrow)+",\"shown\":"+(arrowAt==now?"true":"false"));}}}
        // Only while the bow aims, the first draw after the change (within 15 s of it) - 2026-10-26, user after the headset: "after
        // changing the arrow and shooting, the choice shows in the old UI": the layer carries the list with the aim mark (the cluster);
        // once the shot ends the aim, the list is not drawn (a frame's margin only, as the HUD is drawn a frame after this).
        static bool aimedSince=false;
        if(arrowAt&&(now<arrowAt||now-arrowAt>15000))arrowAt=0;
        if(arrowAt&&markShown){aimedSince=true;showGameList(now+40);}
        else if(arrowAt&&aimedSince){arrowAt=0;aimedSince=false;}   // that draw ended
        if(!arrowAt)aimedSince=false;
        hide(false);return;
    }
    if(!kind){hide(false);return;}
    const auto st=liveEquipment::state(now);
    if(!st.valid){hide(true);return;}
    Look look;const auto& inUse=kind==1?st.spell:st.arrow;const uint32_t selected=inUse.stack.item;
    const auto countOf=[](const liveEquipment::Stack& s){uint32_t c=0;std::memcpy(&c,s.data.data(),4);return c;};
    // the game's own list (game_ammo_list.hpp: with what nearby storage holds, in the HUD's order) when it holds this kind; the
    // player's rows without it (the HUD fills it for the staff's spells; for the bow it is not known yet)
    std::vector<gameAmmoList::Entry> game;
    if(gameAmmoList::snapshot(game,now))for(const auto& g:game){const auto* n=evr::ammo::find(g.item);if(n&&n->kind==kind)look.rows.push_back({g.item,g.count,n->stack<=1});}
    if(look.rows.empty())for(const auto& [worn,category]:st.rows){
        const auto* n=evr::ammo::find(worn.stack.item);if(!worn.stack.item||!n||n->kind!=kind)continue;
        auto found=std::find_if(look.rows.begin(),look.rows.end(),[&](const evr::ammopanel::Row& r){return r.item==worn.stack.item;});
        if(found==look.rows.end())look.rows.push_back({worn.stack.item,countOf(worn.stack),n->stack<=1});else if(!found->eternal)found->count+=countOf(worn.stack);}
    if(selected&&std::none_of(look.rows.begin(),look.rows.end(),[&](const evr::ammopanel::Row& r){return r.item==selected;}))
        if(const auto* n=evr::ammo::find(selected);n&&n->kind==kind)look.rows.insert(look.rows.begin(),{selected,countOf(inUse.stack),n->stack<=1});
    for(size_t i=0;i<look.rows.size();++i)if(look.rows[i].item==selected)look.chosen=i;
    if(look.rows.empty()){hide(true);return;}
    painter.ask(look);   // painted ahead (the fonts load the first time), so the list is ready when the spell changes
    // shown only after the spell in use changes. Diagnostic: EVR_SPELL_PANEL_SHOW_AT=<frame>/<frame>... shows it as a change would
    // there (the Mock cannot switch the spell: its keys 1-8 choose the action bar's slots)
    {static const std::vector<uint64_t> at=[]{std::vector<uint64_t> v;char t[256]{};const auto n=GetEnvironmentVariableA("EVR_SPELL_PANEL_SHOW_AT",t,sizeof t);
        if(n&&n<sizeof t){std::string s(t);size_t p=0;while(p<s.size()){auto e=s.find('/',p);if(e==std::string::npos)e=s.size();v.push_back(std::strtoull(s.substr(p,e-p).c_str(),nullptr,10));p=e+1;}}return v;}();
     static size_t next=0;while(next<at.size()&&liveFrame.frame>=at[next]){shownAt=now;++next;logFile->event("spell_panel_shown_by_test","\"frame\":"+std::to_string(liveFrame.frame));}}
    if(selected!=lastSelected){if(lastSelected&&selected)shownAt=now;lastSelected=selected;
        if(logs<300){++logs;logFile->event("spell_panel_chosen","\"frame\":"+std::to_string(liveFrame.frame)+",\"selected\":"+std::to_string(selected)+",\"shown\":"+(shownAt==now?"true":"false"));}}
    const uint64_t since=shownAt&&now>=shownAt?now-shownAt:UINT64_MAX;
    const float strength=since<fadeInMs?float(since)/fadeInMs:since<lingerMs?1.f:since<lingerMs+fadeOutMs?1.f-float(since-lingerMs)/fadeOutMs:0.f;
    (void)markShown;   // without the mark (a bow not drawn) the layer puts it where the game's list stood on the HUD plate
    // the picture last painted for this list's rows and choice (its counts may lag a moment)
    evr::ammopanel::Picture picture;uint64_t serial=0;bool current=false;
    {std::lock_guard<std::mutex> l(painter.lock);serial=painter.paintedSerial;current=painter.painted.chosen==look.chosen&&painter.painted.rows.size()==look.rows.size();
     if(serial!=sentSerial)picture=painter.picture;}
    if(strength<=0||!serial||!current){hide(true);return;}
    if(serial!=sentSerial&&!picture.rgba.empty()){sentSerial=serial;sentDegrees=float(picture.width)/hudPixelsPerDegree;sentAnchor=picture.anchor;
        setPanel(picture.rgba.data(),picture.width,picture.height,sentDegrees,sentAnchor,picture.left/hudPixelsPerDegree,strength);}
    else setPanel(nullptr,0,0,sentDegrees,sentAnchor,evr::ammopanel::leftOfPicture/hudPixelsPerDegree,strength);
    sentShown=true;
}
// When the world is left (view_resource_probe.cpp): the painter's thread ends.
inline void finish(){painter.finish();sentSerial=0;sentShown=false;lastSelected=0;shownAt=0;}
}
