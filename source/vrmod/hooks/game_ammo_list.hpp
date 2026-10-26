#pragma once
// The game's own list of the ammunition it offers for the weapon in the hands - the spells of the staff's spell list beside the
// crosshair, in the HUD's order (2026-10-26, for the mod's own list, spell_panel.hpp). Found in the UI's memory (Mock 20261026-111318,
// copies taken while the list showed, scratchpad spells/find_in_scan.py): in the UI controller (frontend state +0x1b8, as
// input_ui_observer.hpp reads it) at +0x15808 a small array {u64 data, u64 count, u64 capacity 64, ...} whose data is its own
// storage at +0x15830, 24 bytes an entry: u32 item, u32 the item's entity, u64 its slot in its row, u64 the count (the copies
// for an eternal spell). It holds what the player carries and what nearby storage holds (Chain Lightning I x76, in no row of the
// player's: a chest, presumably). Read on the game's thread inside the frontend's update; only an array that holds to that shape
// is taken.
namespace gameAmmoList {
struct Entry {uint32_t item=0,entity=0,slot=0,count=0;};
constexpr uintptr_t arrayOffset=0x15808,storageOffset=0x15830;constexpr uint64_t capacity=64;
inline std::mutex mutex;inline std::vector<Entry> latest;inline uint64_t latestAt=0;inline bool shapeLogged=false;inline unsigned changes=0;
inline void observe(uintptr_t controller,uint64_t now){
    try{
        const auto data=read<uint64_t>(controller+arrayOffset),count=read<uint64_t>(controller+arrayOffset+8),cap=read<uint64_t>(controller+arrayOffset+16);
        if(data!=controller+storageOffset||count>capacity||cap!=capacity){
            if(!shapeLogged){shapeLogged=true;logFile->event("game_ammo_list_shape","\"controller\":"+hex(controller)+",\"data\":"+hex(uintptr_t(data))+",\"count\":"+std::to_string(count)+",\"capacity\":"+std::to_string(cap)+",\"taken\":false");}
            return;}
        std::vector<Entry> list;list.reserve(size_t(count));
        for(uint64_t i=0;i<count;++i){const auto at=controller+storageOffset+uintptr_t(i)*24;const auto raw=read<std::array<uint32_t,6>>(at);list.push_back({raw[0],raw[1],raw[2],raw[4]});}
        bool changed=false;
        {std::lock_guard<std::mutex> lock(mutex);
         changed=list.size()!=latest.size()||!std::equal(list.begin(),list.end(),latest.begin(),[](const Entry& a,const Entry& b){return a.item==b.item&&a.count==b.count&&a.slot==b.slot;});
         latest=std::move(list);latestAt=now;}
        if(changed&&changes<300){++changes;std::ostringstream o;o<<"\"entries\":[";std::lock_guard<std::mutex> lock(mutex);
            for(size_t i=0;i<latest.size();++i)o<<(i?",":"")<<'['<<latest[i].item<<','<<latest[i].entity<<','<<latest[i].slot<<','<<latest[i].count<<']';
            o<<']';logFile->event("game_ammo_list",o.str());}
    }catch(const std::exception&){}
}
// The list as last read, if read within maxAge milliseconds.
inline bool snapshot(std::vector<Entry>& out,uint64_t now,uint64_t maxAge=500){
    std::lock_guard<std::mutex> lock(mutex);if(!latestAt||now<latestAt||now-latestAt>maxAge)return false;out=latest;return true;
}
}
