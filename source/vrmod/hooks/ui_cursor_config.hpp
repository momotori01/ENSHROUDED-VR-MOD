#pragma once
// The game's own settings for the HUD round the crosshair (2026-10-26, the staff's spell list - user: "cutting the UI always goes
// wrong somewhere; take the spell switching out of the UI and draw what is needed at the reticle"). The UI bundle
// (keen::FbUiBundle, docs/analysis/special_ability_binding_20261021/fbuibundle.json) holds at +55192 keen::FbUiHUD, which holds at
// +48 keen::FbUiCursors (276 bytes, 1.0 reflection):
//   +72 staff_directional {layout 0 LeftRight, partTexture, +20 partHeight 50, +24 idleDistance 80,0}
//   +104 staff_singleTarget {1, .., 30, 60,60}   +168 staff_meteor {2, .., 70, 120,10}
//   +200 spellsListOffsetX 120 (the spell list's distance right of the crosshair)   +204 screenshot {2, .., 30, 640,360}
// In use: keep() below - the frontend's pointer to the bundle, the offset written while the mod draws its own spell list (Mock
// 20261026-112907: 4000 takes the game's list off the UI picture). Diagnostic: a search of the whole memory on a thread of its own
// (the game does not stop; ReadProcessMemory, so a page that goes away is only skipped; about 60 s, and the cursors are only there
// once the HUD has loaded them): EVR_SPELL_LIST_OFFSET=<pixels> with EVR_UI_CURSOR_SCAN_FRAME writes the offset into every copy
// found, EVR_UI_CURSOR_SCAN=1 only looks, EVR_UI_CURSOR_KEYS=<hex>/... logs other byte strings with what is round them.
namespace uiCursorConfig {
inline std::thread worker;inline std::atomic<bool> stop{false},started{false};inline bool wanted=false,write=false;inline float offset=0;
inline uint64_t startFrame=0,failedBytes=0,bigBytes=0;
inline void install(){
    char v[32]{};const auto n=GetEnvironmentVariableA("EVR_SPELL_LIST_OFFSET",v,sizeof v);
    if(n&&n<sizeof v){offset=std::strtof(v,nullptr);write=std::isfinite(offset);wanted=write;}
    char s[8]{};if(GetEnvironmentVariableA("EVR_UI_CURSOR_SCAN",s,sizeof s)==1&&s[0]=='1')wanted=true;
    char f[32]{};const auto m=GetEnvironmentVariableA("EVR_UI_CURSOR_SCAN_FRAME",f,sizeof f);startFrame=m&&m<sizeof f?std::strtoull(f,nullptr,10):1500;
    if(write&&!(m&&m<sizeof f)&&!(s[0]=='1')){write=false;wanted=false;}   // EVR_SPELL_LIST_OFFSET alone: only the quick way's value (keep)
    if(wanted)logFile->event("ui_cursor_config_armed","\"write\":"+std::string(write?"true":"false")+",\"offset\":"+std::to_string(offset)+",\"frame\":"+std::to_string(startFrame));
}
inline bool readBack(uintptr_t at,void* out,size_t n){SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(at),out,n,&got)&&got==n;}
inline float floatAt(const std::vector<char>& b,size_t at){float f=0;std::memcpy(&f,b.data()+at,4);return f;}
// The cursors' own values about a candidate (the bytes from cursors+0 to +205). Mock 20261026-110253: the bundle's own copy (read
// only, in the archive's allocation 0x20000000000) and two copies in the heap (read-write, 0x200 apart) that agree with it up to
// +216 only (the screenshot crosshair's texture and what follows differ) - so only the fields up to the screenshot's layout.
inline bool cursorsLook(const std::vector<char>& c){
    if(c.size()<205)return false;
    return uint8_t(c[72])==0&&floatAt(c,92)==50.f&&floatAt(c,96)==80.f&&floatAt(c,100)==0.f&&
           uint8_t(c[104])==1&&floatAt(c,124)==30.f&&floatAt(c,128)==60.f&&floatAt(c,132)==60.f&&
           uint8_t(c[168])==2&&floatAt(c,188)==70.f&&floatAt(c,192)==120.f&&floatAt(c,196)==10.f&&uint8_t(c[204])==2;
}
template<class F> inline void forEachRegion(F&& f){
    uintptr_t at=0x10000;std::vector<char> buffer;
    while(at<0x7ffffffeffffull&&!stop.load()){
        MEMORY_BASIC_INFORMATION q{};if(!VirtualQuery(reinterpret_cast<void*>(at),&q,sizeof q))break;
        const auto base=reinterpret_cast<uintptr_t>(q.BaseAddress);const auto size=size_t(q.RegionSize);
        const bool readable=q.State==MEM_COMMIT&&!(q.Protect&(PAGE_GUARD|PAGE_NOACCESS))&&
            (q.Protect&(PAGE_READONLY|PAGE_READWRITE|PAGE_WRITECOPY|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY));
        if(readable&&size>(1ull<<30))bigBytes+=size;
        if(readable&&size<=(1ull<<30))
            for(size_t off=0;off<size&&!stop.load();off+=(16ull<<20)){const size_t len=std::min<size_t>(16ull<<20,size-off);buffer.resize(len);
                if(readBack(base+off,buffer.data(),len))f(base+off,buffer,q);else failedBytes+=len;}
        at=base+size;if(!size)break;
    }
}
// EVR_UI_CURSOR_KEYS=<hex>/<hex>/...: each byte string is looked for too, and up to 24 finds of each are logged with the 256
// bytes from 128 before (ui_cursor_config_key_hit) - for when the cursors are not laid out as in the bundle file.
inline std::vector<std::string> extraKeys(){
    std::vector<std::string> keys;char h[1024]{};const auto m=GetEnvironmentVariableA("EVR_UI_CURSOR_KEYS",h,sizeof h);if(!m||m>=sizeof h)return keys;
    std::string text(h);size_t at=0;
    while(at<text.size()){auto end=text.find('/',at);if(end==std::string::npos)end=text.size();const auto hex=text.substr(at,end-at);std::string w;
        for(size_t i=0;i+1<hex.size();i+=2)w.push_back(char(std::strtoul(hex.substr(i,2).c_str(),nullptr,16)));if(w.size()>=4)keys.push_back(w);at=end+1;}
    return keys;
}
inline void run(){
    try{
        const auto began=GetTickCount64();
        {const auto keys=extraKeys();
         if(!keys.empty()){std::vector<unsigned> hits(keys.size(),0);uint64_t mb=0;
          forEachRegion([&](uintptr_t base,const std::vector<char>& b,const MEMORY_BASIC_INFORMATION& q){mb+=b.size();
            for(size_t k=0;k<keys.size();++k){auto it=b.begin();
                while(hits[k]<24){it=std::search(it,b.end(),std::boyer_moore_horspool_searcher(keys[k].begin(),keys[k].end()));if(it==b.end())break;
                    const auto hit=base+uintptr_t(it-b.begin());++hits[k];++it;std::vector<char> around(256);std::string text;
                    if(hit>=128&&readBack(hit-128,around.data(),around.size())){std::ostringstream o;for(const char c:around)o<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(uint8_t(c));text=o.str();}
                    logFile->event("ui_cursor_config_key_hit","\"key\":"+std::to_string(k)+",\"address\":"+hex(hit)+",\"allocation\":"+hex(reinterpret_cast<uintptr_t>(q.AllocationBase))+
                        ",\"type\":"+std::to_string(q.Type)+",\"protect\":"+std::to_string(q.Protect)+",\"bytes_from_minus_128\":\""+text+"\"");}}});
          std::string counts;for(const auto n:hits)counts+=(counts.empty()?"":",")+std::to_string(n);
          logFile->event("ui_cursor_config_keys_done","\"hits\":["+counts+"],\"scanned_mb\":"+std::to_string(mb>>20)+",\"ms\":"+std::to_string(GetTickCount64()-began));}}
        // staff_meteor's partHeight and idleDistance, then spellsListOffsetX: 70, 120, 10, 120
        static const char key[16]={0,0,char(0x8c),0x42,0,0,char(0xf0),0x42,0,0,0x20,0x41,0,0,char(0xf0),0x42};
        // The cursors are loaded when the HUD first needs them (Mock 20261026-105308 and -111318: a scan from frame 1300 found nothing,
        // one from 1610 found them) - so up to eight scans, 15 s apart, until they are there.
        std::vector<uintptr_t> found;uint64_t scanned=0;unsigned candidates=0,attempts=0;
        while(found.empty()&&attempts<8&&!stop.load()){
            if(attempts++)for(int i=0;i<150&&!stop.load();++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
            forEachRegion([&](uintptr_t base,const std::vector<char>& b,const MEMORY_BASIC_INFORMATION&){scanned+=b.size();
                auto it=b.begin();
                while(found.size()<16){it=std::search(it,b.end(),std::boyer_moore_horspool_searcher(std::begin(key),std::end(key)));if(it==b.end())break;
                    const auto hit=base+uintptr_t(it-b.begin());++candidates;++it;
                    std::vector<char> c(205);if(hit>=188&&readBack(hit-188,c.data(),c.size())&&cursorsLook(c))found.push_back(hit-188);}});
            logFile->event("ui_cursor_config_attempt","\"attempt\":"+std::to_string(attempts)+",\"found\":"+std::to_string(found.size())+",\"candidates\":"+std::to_string(candidates)+",\"frame\":"+std::to_string(liveFrame.frame));
        }
        for(const auto cursors:found){
            MEMORY_BASIC_INFORMATION q{};VirtualQuery(reinterpret_cast<void*>(cursors),&q,sizeof q);float was=0;readBack(cursors+200,&was,4);
            std::string wrote="false";
            if(write){
                DWORD old=0;const bool writable=(q.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE))!=0;
                if(writable||VirtualProtect(reinterpret_cast<void*>(cursors+200),4,PAGE_READWRITE,&old)){
                    *reinterpret_cast<volatile float*>(cursors+200)=offset;wrote=writable?"\"in place\"":"\"after VirtualProtect\"";}
                else wrote="\"not writable\"";
            }
            logFile->event("ui_cursor_config_found","\"cursors\":"+hex(cursors)+",\"hud\":"+hex(cursors-48)+",\"bundle\":"+hex(cursors-48-55192)+
                ",\"allocation\":"+hex(reinterpret_cast<uintptr_t>(q.AllocationBase))+",\"region\":"+hex(reinterpret_cast<uintptr_t>(q.BaseAddress))+",\"region_size\":"+std::to_string(q.RegionSize)+
                ",\"type\":"+std::to_string(q.Type)+",\"protect\":"+std::to_string(q.Protect)+",\"spells_list_offset_x\":"+std::to_string(was)+",\"wrote\":"+wrote+",\"offset\":"+std::to_string(offset));
        }
        logFile->event("ui_cursor_config_scan_done","\"found\":"+std::to_string(found.size())+",\"candidates\":"+std::to_string(candidates)+",\"scanned_mb\":"+std::to_string(scanned>>20)+",\"unread_mb\":"+std::to_string(failedBytes>>20)+",\"over_1gb_mb\":"+std::to_string(bigBytes>>20)+",\"ms\":"+std::to_string(GetTickCount64()-began));
        // Who points at them (the bundle's start, the HUD's, the cursors'): the way there without a scan, later.
        if(found.empty()||stop.load())return;
        std::unordered_set<uint64_t> targets;for(const auto c:found){targets.insert(c);targets.insert(c-48);targets.insert(c-48-55192);}
        unsigned pointers=0;const auto exe=baseAddress;
        forEachRegion([&](uintptr_t base,const std::vector<char>& b,const MEMORY_BASIC_INFORMATION& q){
            const auto* p=reinterpret_cast<const uint64_t*>(b.data());const size_t n=b.size()/8;
            for(size_t i=0;i<n&&pointers<200;++i)if(targets.count(p[i])){++pointers;const auto at=base+i*8;
                logFile->event("ui_cursor_config_pointer","\"at\":"+hex(at)+",\"to\":"+hex(uintptr_t(p[i]))+",\"region\":"+hex(reinterpret_cast<uintptr_t>(q.BaseAddress))+
                    ",\"allocation\":"+hex(reinterpret_cast<uintptr_t>(q.AllocationBase))+",\"exe_rva\":"+hex(at>=exe&&at<exe+0x4000000?at-exe:0));}});
        logFile->event("ui_cursor_config_pointers_done","\"pointers\":"+std::to_string(pointers)+",\"ms\":"+std::to_string(GetTickCount64()-began));
    }catch(const std::exception& e){logFile->event("ui_cursor_config_failure","\"reason\":"+jsonString(e.what()));}
}
// The quick way (2026-10-26, Mock 20261026-111318's copies of the UI's memory): the frontend holds the bundle at +0x710 (and +0x738);
// its cursors are at +55192+48. The game's own spell list is moved off the picture while the mod draws its own (spell_panel.hpp asks
// with wantHidden, every frame it shows), and put back at 120 otherwise - the controller VR (pad style) keeps the game's list.
// shownUntil: the game's own list on its picture until then even while asked off (the bow: its arrow list only after a change of
// arrow, spell_panel.hpp - 2026-10-26, user: "the arrows show at every nock; only on a change input, as the spells").
inline std::atomic<uint64_t> hiddenAskedAt{0},shownUntil{0};inline uintptr_t heldCursors=0;inline float heldWas=120.f;inline bool heldWritten=false;inline unsigned quickLogs=0;
inline float hiddenOffset(){static const float v=[]{char t[32]{};const auto n=GetEnvironmentVariableA("EVR_SPELL_LIST_OFFSET",t,sizeof t);
    const float f=n&&n<sizeof t?std::strtof(t,nullptr):4000.f;return std::isfinite(f)?f:4000.f;}();return v;}
inline bool writeOffset(uintptr_t cursors,float value){
    MEMORY_BASIC_INFORMATION q{};if(!VirtualQuery(reinterpret_cast<void*>(cursors+200),&q,sizeof q))return false;
    DWORD old=0;const bool writable=(q.Protect&(PAGE_READWRITE|PAGE_EXECUTE_READWRITE))!=0;
    if(!writable&&!VirtualProtect(reinterpret_cast<void*>(cursors+200),4,PAGE_READWRITE,&old))return false;
    *reinterpret_cast<volatile float*>(cursors+200)=value;return true;
}
inline void keep(uintptr_t frontend,uint64_t now){
    const auto asked=hiddenAskedAt.load();const bool want=asked&&now>=asked&&now-asked<=2000&&!(now<shownUntil.load());
    if(want==heldWritten)return;
    try{
        if(want){
            const auto bundle=read<uintptr_t>(frontend+0x710);if(!bundle)return;const auto cursors=bundle+55192+48;
            std::vector<char> c(205);if(!readBack(cursors,c.data(),c.size()))return;
            if(!cursorsLook(c)){if(quickLogs++<4)logFile->event("ui_cursor_config_quick","\"bundle\":"+hex(bundle)+",\"cursors_look\":false");return;}
            heldWas=floatAt(c,200);if(!std::isfinite(heldWas)||heldWas>1000.f)heldWas=120.f;
            const bool ok=writeOffset(cursors,hiddenOffset());heldCursors=ok?cursors:0;heldWritten=ok;
            if(quickLogs++<40)logFile->event("ui_cursor_config_quick","\"bundle\":"+hex(bundle)+",\"cursors\":"+hex(cursors)+",\"was\":"+std::to_string(heldWas)+",\"offset\":"+std::to_string(hiddenOffset())+",\"wrote\":"+(ok?"true":"false"));
            if(!ok)hiddenAskedAt=0;   // not again until asked again
        }else{
            const bool ok=heldCursors&&writeOffset(heldCursors,heldWas);heldWritten=false;
            if(quickLogs++<40)logFile->event("ui_cursor_config_quick","\"put_back\":"+std::string(ok?"true":"false")+",\"offset\":"+std::to_string(heldWas));heldCursors=0;
        }
    }catch(const std::exception&){}
}
// When the world is left: the game's own offset back.
inline void putBack(){if(heldWritten&&heldCursors){writeOffset(heldCursors,heldWas);heldWritten=false;heldCursors=0;hiddenAskedAt=0;}}
// From the frontend's update (input_ui_observer.hpp): once, at the frame asked for.
inline void maybe(){
    if(!wanted||started.load()||liveFrame.frame<startFrame)return;
    started=true;stop=false;worker=std::thread(run);
}
// When the world is left (input_ui_observer.hpp disable): the scan stops at its next 16 MB.
inline void finish(){stop=true;if(worker.joinable())worker.join();started=false;putBack();}
}
