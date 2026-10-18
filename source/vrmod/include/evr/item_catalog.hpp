#pragma once
#include <evr/weapon_category.hpp>
#include <set>
#include <vector>
#include <stdexcept>
namespace evr::game {
// Read-only diagnostic of the resident registry, not proof every asset is loaded.
struct CatalogItem {uint32_t id=0,bucket=0;uint8_t category=0,slot=0;std::array<uint8_t,1944> data{};bool selected=false;WeaponCategoryLookup weapon;std::vector<uint8_t> name,models;};
struct ItemCatalog {ItemMapHeader header;std::vector<uint64_t> bitmap,values;std::vector<uint32_t> keys;std::vector<CatalogItem> items;};
template<class Reader> ItemCatalog snapshotItemCatalog(Reader& read,uint64_t registry){
 ItemCatalog out;
 if(!readItemMap(read,registry,out.header)||!validItemMap(out.header)||!out.header.count||out.header.count>32768||out.header.capacity>131072)throw std::runtime_error("catalog table bounds");
 const auto& h=out.header;out.bitmap.resize((h.capacity+63)/64);out.keys.resize(h.capacity);out.values.resize(h.capacity);
 auto copy=[&](uint64_t at,auto& v){if(!itemRead(read,at,v.data(),v.size()*sizeof(v[0])))throw std::runtime_error("catalog unreadable array");};
 copy(h.bitmap,out.bitmap);copy(h.keys,out.keys);copy(h.values,out.values);
 std::set<uint32_t> seen;unsigned selected=0;
 for(uint32_t i=0;i<h.capacity;++i){
  if(!(out.bitmap[i/64]&(1ull<<(i%64))))continue;
  const auto id=out.keys[i];const auto address=out.values[i];
  if(!id||!address||!seen.insert(id).second)throw std::runtime_error("catalog null or duplicate bucket");
  const auto lookup=lookupItem(read,registry,id);ItemMetadata meta;
  if(lookup.status!=ItemLookupStatus::Found||lookup.index!=i||lookup.pointer!=address||!readItemMetadata(read,lookup,id,meta))throw std::runtime_error("catalog item lookup/source changed");
  CatalogItem item;item.id=id;item.bucket=i;item.category=meta.category;item.slot=meta.slot;
  item.selected=meta.category==2||meta.slot==18||meta.slot==2||meta.slot==3;
  if(item.selected){
   if(++selected>4096)throw std::runtime_error("catalog equipment budget");
   std::array<uint8_t,1944> again{};
   if(!itemRead(read,address,item.data)||!itemRead(read,address,again)||item.data!=again)throw std::runtime_error("catalog equipment descriptor changed/unreadable");
   uint32_t storedId=0;std::memcpy(&storedId,item.data.data(),4);
   if(storedId!=id||item.data[itemCategory]!=meta.category||item.data[itemEquipmentSlot]!=meta.slot)throw std::runtime_error("catalog descriptor identity mismatch");
   if(meta.category==2)item.weapon=itemWeaponCategory(read,registry,lookup,id);
   auto blob=[&](unsigned offset,unsigned stride,unsigned limit){
    uint32_t relative=0,count=0;std::memcpy(&relative,item.data.data()+offset,4);std::memcpy(&count,item.data.data()+offset+4,4);
    std::vector<uint8_t> bytes;
    if(!count)return bytes;
    if(!relative||relative>64*1024*1024||count>limit||!itemAddressRange(address,uint64_t(offset)+relative+uint64_t(count)*stride))throw std::runtime_error("catalog relative data bounds");
    bytes.resize(size_t(count)*stride);auto second=bytes;
    if(!itemRead(read,address+offset+relative,bytes.data(),bytes.size())||!itemRead(read,address+offset+relative,second.data(),second.size())||bytes!=second)throw std::runtime_error("catalog relative data changed/unreadable");
    return bytes;
   };
   item.name=blob(0x790,1,512);item.models=blob(0x49c,20,64);
   if(!itemRead(read,address,again)||item.data!=again)throw std::runtime_error("catalog descriptor changed during dependencies");
  }
  out.items.push_back(std::move(item));
 }
 if(out.items.size()!=h.count)throw std::runtime_error("catalog occupied count differs");
 auto bits=out.bitmap,values=out.values;auto keys=out.keys;copy(h.bitmap,bits);copy(h.keys,keys);copy(h.values,values);ItemMapHeader last;
 if(bits!=out.bitmap||keys!=out.keys||values!=out.values||!readItemMap(read,registry,last)||!(last==h))throw std::runtime_error("catalog table changed during snapshot");
 return out;
}
}
