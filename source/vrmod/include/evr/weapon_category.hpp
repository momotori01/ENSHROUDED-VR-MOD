#pragma once
#include <evr/item_registry.hpp>
namespace evr::game {
// Build23966345: ItemInfo+d0 Guid -> 1e4600 -> cc0200/ca5ce0.
// Caller supplies a read-only, lifetime-valid registry; no game calls or writes.
using WeaponCategoryGuid=std::array<uint8_t,16>;
inline uint32_t weaponCategoryHash(const WeaponCategoryGuid& guid){
 if(std::all_of(guid.begin(),guid.end(),[](uint8_t b){return b==0;}))return 0;
 uint32_t h=0x811c9dc5u;for(auto b:guid){h^=b;h*=0x1000193u;}return h;
}
struct WeaponCategoryLookup {
 ItemLookupStatus status=ItemLookupStatus::Invalid;
 WeaponCategoryGuid guid{};uint32_t hash=0,probes=0;uint64_t pointer=0,index=0;
 uint8_t type=255,alignment=255;ItemMapHeader map{};
 std::array<uint8_t,88> resource{};
 bool twoHandMelee()const{return status==ItemLookupStatus::Found&&type>=7&&type<=10;}
 bool oneHandMelee()const{return status==ItemLookupStatus::Found&&(type<=3||type==13);}
};
template<class Reader> bool readWeaponCategoryMap(Reader& read,uint64_t registry,ItemMapHeader& h){
 // 1.0 (2026-10-16): the weapon category map is at +0x248 (+0x2c0 before; Mock run game-20261016-083055: the one map of
 // the registry with 16 entries whose values hash by their GUID to their keys, a type byte <= 15 at +84).
 if(!itemAddressRange(registry,0x2a0))return false;
 std::array<uint8_t,0x58> b{};if(!itemRead(read,registry+0x248,b))return false;
 std::memcpy(&h.bitmap,b.data()+8,8);std::memcpy(&h.capacity,b.data()+0x10,8);
 std::memcpy(&h.keys,b.data()+0x28,8);std::memcpy(&h.values,b.data()+0x40,8);std::memcpy(&h.count,b.data()+0x54,4);return true;
}
template<class Reader> WeaponCategoryLookup lookupWeaponCategory(Reader& read,uint64_t registry,const WeaponCategoryGuid& guid){
 WeaponCategoryLookup r;r.guid=guid;r.hash=weaponCategoryHash(guid);
 if(std::all_of(guid.begin(),guid.end(),[](uint8_t b){return b==0;})){r.status=ItemLookupStatus::NoItem;return r;}
 if(!readWeaponCategoryMap(read,registry,r.map)){r.status=ItemLookupStatus::Unreadable;return r;}
 auto finish=[&](ItemLookupStatus status){ItemMapHeader again;if(!readWeaponCategoryMap(read,registry,again))status=ItemLookupStatus::Unreadable;
  else if(!(r.map==again))status=ItemLookupStatus::Changed;r.status=status;return r;};
 if(!validItemMap(r.map))return finish(ItemLookupStatus::Invalid);
 if(!r.map.count)return finish(ItemLookupStatus::Missing);
 auto i=uint64_t(r.hash)&(r.map.capacity-1);
 for(uint32_t n=0;n<std::min<uint64_t>(r.map.capacity,itemProbeBudget);n++,i=(i+1)&(r.map.capacity-1)){
  r.probes++;uint64_t word=0;uint32_t key=0;
  if(!itemRead(read,r.map.bitmap+(i/64)*8,word))return finish(ItemLookupStatus::Unreadable);
  if(!(word&(1ull<<(i%64))))return finish(ItemLookupStatus::Missing);
  if(!itemRead(read,r.map.keys+i*4,key))return finish(ItemLookupStatus::Unreadable);
  if(key!=r.hash)continue;
  r.index=i;if(!itemRead(read,r.map.values+i*8,r.pointer))return finish(ItemLookupStatus::Unreadable);
  if(!r.pointer)return finish(ItemLookupStatus::Missing);
  std::array<uint8_t,88> again{};
  if(!itemRead(read,r.pointer,r.resource)||!itemRead(read,r.pointer,again))return finish(ItemLookupStatus::Unreadable);
  if(r.resource!=again)return finish(ItemLookupStatus::Changed);
  // Native table is hash-keyed. Verify full Guid before enabling weapon policy,
  // so a hash collision or stale pointer cannot classify another resource.
  if(!std::equal(guid.begin(),guid.end(),r.resource.begin())||r.resource[84]>15)return finish(ItemLookupStatus::Invalid);
  uint64_t word2=0,pointer2=0;uint32_t key2=0;
  if(!itemRead(read,r.map.bitmap+(i/64)*8,word2)||!itemRead(read,r.map.keys+i*4,key2)||!itemRead(read,r.map.values+i*8,pointer2))return finish(ItemLookupStatus::Unreadable);
  if(word!=word2||key!=key2||r.pointer!=pointer2)return finish(ItemLookupStatus::Changed);
  r.type=r.resource[84];r.alignment=r.resource[85];return finish(ItemLookupStatus::Found);
 }
 return finish(ItemLookupStatus::Budget);
}
template<class Reader> WeaponCategoryLookup itemWeaponCategory(Reader& read,uint64_t registry,const ItemLookup& item,uint32_t id){
 WeaponCategoryLookup r;if(item.status!=ItemLookupStatus::Found){r.status=item.status;return r;}
 uint32_t firstId=0,lastId=0;WeaponCategoryGuid guid{},again{};
 if(!itemRead(read,item.pointer,firstId)||!itemRead(read,item.pointer+itemWeaponCategoryReference,guid)){r.status=ItemLookupStatus::Unreadable;return r;}
 if(firstId!=id){r.status=ItemLookupStatus::Invalid;return r;}
 r=lookupWeaponCategory(read,registry,guid);
 if(!itemRead(read,item.pointer,lastId)||!itemRead(read,item.pointer+itemWeaponCategoryReference,again))r.status=ItemLookupStatus::Unreadable;
 else if(lastId!=firstId||again!=guid)r.status=ItemLookupStatus::Changed;
 return r;
}
}
