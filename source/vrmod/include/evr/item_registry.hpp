#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>

namespace evr::game {
// Build-23966345 read-only adapter for cb4b50 -> ca5bc0. No game calls,
// allocations, pointer retention, or writes. The reader must safely reject
// unreadable memory. Bounds are diagnostic budgets, not inferred game limits.
enum class ItemLookupStatus { NoItem, Missing, Found, Unreadable, Invalid, Changed, Budget };
inline const char* itemLookupName(ItemLookupStatus s){
    switch(s){
    case ItemLookupStatus::NoItem:return "no_item";case ItemLookupStatus::Missing:return "missing";
    case ItemLookupStatus::Found:return "found";case ItemLookupStatus::Unreadable:return "unreadable";
    case ItemLookupStatus::Invalid:return "invalid";case ItemLookupStatus::Changed:return "changed";
    case ItemLookupStatus::Budget:return "budget";
    }return "invalid";
}
// The item as the registry holds it. 1.0 (2026-10-16): unchanged at runtime - the item of the main hand read in Mock run
// game-20261016-083055 has its category 2 at +0xcc and its weapon category GUID at +0xd0 (the reflection's keen::ItemInfo
// with category +0xec is another form of it).
constexpr uint64_t itemCategory=0xcc,itemAmmunition=0xcd,itemWeaponCategoryReference=0xd0,itemEquipmentSlot=0x104,itemMetadataBytes=0x108;
constexpr uint64_t itemCapacityBudget=1ull<<20;
constexpr uint32_t itemProbeBudget=4096;
struct ItemMapHeader {
    uint64_t bitmap=0,capacity=0,keys=0,values=0;uint32_t count=0;
    bool operator==(const ItemMapHeader& b)const{return bitmap==b.bitmap&&capacity==b.capacity&&keys==b.keys&&values==b.values&&count==b.count;}
};
inline bool itemAddressRange(uint64_t p,uint64_t n){return p>=0x10000&&p<=0x00007fffffffffffULL&&n&&n<=0x0000800000000000ULL-p;}
template<class Reader> bool itemRead(Reader& read,uint64_t p,void* out,size_t n){return itemAddressRange(p,n)&&read(p,out,n);}
template<class T,class Reader> bool itemRead(Reader& read,uint64_t p,T& out){return itemRead(read,p,&out,sizeof(out));}
template<class Reader> bool readItemMap(Reader& read,uint64_t registry,ItemMapHeader& h){
    std::array<uint8_t,0x58> b{};
    if(!itemAddressRange(registry,0x90)||!itemRead(read,registry+0x38,b))return false;
    std::memcpy(&h.bitmap,b.data()+8,8);std::memcpy(&h.capacity,b.data()+0x10,8);
    std::memcpy(&h.keys,b.data()+0x28,8);std::memcpy(&h.values,b.data()+0x40,8);std::memcpy(&h.count,b.data()+0x54,4);
    return true;
}
inline bool validItemMap(const ItemMapHeader& h){
    return h.capacity&&h.capacity<=itemCapacityBudget&&!(h.capacity&(h.capacity-1))&&h.count<=h.capacity&&
        itemAddressRange(h.bitmap,((h.capacity+63)/64)*8)&&itemAddressRange(h.keys,h.capacity*4)&&itemAddressRange(h.values,h.capacity*8);
}
struct ItemLookup {
    ItemLookupStatus status=ItemLookupStatus::Invalid;uint64_t pointer=0,index=0;uint32_t probes=0;ItemMapHeader map{};
};
template<class Reader> ItemLookup lookupItem(Reader& read,uint64_t registry,uint32_t id){
    ItemLookup r;if(!id){r.status=ItemLookupStatus::NoItem;return r;}
    if(!readItemMap(read,registry,r.map)){r.status=ItemLookupStatus::Unreadable;return r;}
    auto finish=[&](ItemLookupStatus s){ItemMapHeader again;
        r.status=!readItemMap(read,registry,again)?ItemLookupStatus::Unreadable:!(again==r.map)?ItemLookupStatus::Changed:s;
        if(r.status!=ItemLookupStatus::Found)r.pointer=0;return r;};
    if(!r.map.count)return finish(ItemLookupStatus::Missing);
    if(!validItemMap(r.map))return finish(ItemLookupStatus::Invalid);
    auto i=uint64_t(id)&(r.map.capacity-1);
    for(uint64_t n=0;n<std::min<uint64_t>(r.map.capacity,itemProbeBudget);++n,i=(i+1)&(r.map.capacity-1)){
        r.index=i;++r.probes;uint64_t word=0;uint32_t key=0;
        if(!itemRead(read,r.map.bitmap+(i/64)*8,word))return finish(ItemLookupStatus::Unreadable);
        if(!(word&(1ull<<(i%64))))return finish(ItemLookupStatus::Missing);
        if(!itemRead(read,r.map.keys+i*4,key))return finish(ItemLookupStatus::Unreadable);
        if(key!=id)continue;
        if(!itemRead(read,r.map.values+i*8,r.pointer))return finish(ItemLookupStatus::Unreadable);
        if(!r.pointer)return finish(ItemLookupStatus::Missing); // cb4b50 dereferences the slot
        if(!itemAddressRange(r.pointer,itemMetadataBytes))return finish(ItemLookupStatus::Invalid);
        // Detect a replaced bucket. This is a race check, not an ECS read lock.
        uint64_t word2=0;uint32_t key2=0;
        if(!itemRead(read,r.map.bitmap+(i/64)*8,word2)||!itemRead(read,r.map.keys+i*4,key2))return finish(ItemLookupStatus::Unreadable);
        if(word2!=word||key2!=key)return finish(ItemLookupStatus::Changed);
        return finish(ItemLookupStatus::Found);
    }
    return finish(ItemLookupStatus::Budget); // never hang on a full/corrupt table
}
struct ItemMetadata {
    uint32_t id=0;uint8_t category=0,ammunition=0,slot=0;
    bool rangedWeapon()const{return category==2&&(slot==4||slot==5);}
};
template<class Reader> bool readItemMetadata(Reader& read,const ItemLookup& lookup,uint32_t id,ItemMetadata& out){
    if(lookup.status!=ItemLookupStatus::Found)return false;
    std::array<uint8_t,itemMetadataBytes> first{},second{};
    if(!itemRead(read,lookup.pointer,first)||!itemRead(read,lookup.pointer,second)||first!=second)return false;
    ItemMetadata m;std::memcpy(&m.id,first.data(),4);m.category=first[itemCategory];m.ammunition=first[itemAmmunition];m.slot=first[itemEquipmentSlot];
    // Unknown enum values remain unreadable to policy; callers must fall back.
    if(m.id!=id||m.category>16||m.ammunition>3||m.slot>40)return false;
    out=m;return true;
}
}
