// Explicit read-only resident item registry survey at an existing local ECS callback.
namespace itemCatalogObserver {
inline bool requested=false;
inline std::atomic<bool> started{false},completed{false},failed{false};
inline std::string hexBytes(const std::vector<uint8_t>& bytes){const char* digit="0123456789abcdef";std::string out;out.reserve(bytes.size()*2);for(auto v:bytes){out+=digit[v>>4];out+=digit[v&15];}return out;}
inline void capture(uintptr_t registry,uint32_t entity){
 if(!requested||started.exchange(true))return;
 try{
  const auto start=GetTickCount64();auto reader=inputRanged::readItemBytes;const auto data=game::snapshotItemCatalog(reader,registry);
  std::ofstream out(calibrationDirectory/"item-catalog.jsonl",std::ios::binary);if(!out)throw std::runtime_error("catalog output unavailable");
  out<<"{\"format\":\"EVR_ITEM_CATALOG_1\",\"entity\":"<<entity<<",\"capacity\":"<<data.header.capacity<<",\"count\":"<<data.header.count<<",\"read_only\":true,\"source_unchanged\":true}\n";
  unsigned selected=0;
  for(const auto& item:data.items){
   out<<"{\"id\":"<<item.id<<",\"bucket\":"<<item.bucket<<",\"category\":"<<unsigned(item.category)<<",\"slot\":"<<unsigned(item.slot)<<",\"selected\":"<<(item.selected?"true":"false");
   if(item.selected){++selected;out<<",\"item_bytes\":"<<jsonString(nativeImpactComponents::asHex(item.data))<<",\"name_bytes\":"<<jsonString(hexBytes(item.name))
     <<",\"models_bytes\":"<<jsonString(hexBytes(item.models))<<",\"weapon_status\":"<<jsonString(game::itemLookupName(item.weapon.status))
     <<",\"weapon_type\":"<<unsigned(item.weapon.type)<<",\"weapon_bytes\":"<<jsonString(nativeImpactComponents::asHex(item.weapon.resource));}
   out<<"}\n";
  }
  out.flush();if(!out)throw std::runtime_error("catalog output incomplete");
  completed=true;logFile->event("item_catalog_complete","\"items\":"+std::to_string(data.items.size())+",\"selected\":"+std::to_string(selected)+",\"capacity\":"+std::to_string(data.header.capacity)+
    ",\"elapsed_ms\":"+std::to_string(GetTickCount64()-start)+",\"read_only\":true,\"source_unchanged\":true,\"all_runtime_assets_loaded\":false");
 }catch(const std::exception& e){failed=true;logFile->event("item_catalog_failure","\"reason\":"+jsonString(e.what())+",\"read_only\":true");}
}
}
