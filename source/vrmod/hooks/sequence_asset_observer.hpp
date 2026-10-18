// Bounded native ActionSequenceRegistry reader; no native function invocation.
// Lookup is independently reconstructed from RVA caae10 / caaf80 / 1d9250.
namespace sequenceAssetObserver {
inline std::mutex mutex;inline std::set<uint32_t> captured;
inline unsigned failures=0;
inline uintptr_t relative(uintptr_t p){const auto delta=read<int32_t>(p);return delta?uintptr_t(int64_t(p)+delta):0;}
template<size_t N> std::string blob(uintptr_t p){const auto b=read<std::array<uint8_t,N>>(p);const char* d="0123456789abcdef";std::string s;s.reserve(N*2);for(auto v:b){s+=d[v>>4];s+=d[v&15];}return s;}
inline void capture(uintptr_t registry,const actionSequenceObserver::Status& actor){
 if(!actor.active||!actor.sequence||!actor.usedItem)return;
 std::lock_guard<std::mutex> guard(mutex);if(captured.count(actor.sequence)||captured.size()>=8||failures>=3)return;
 try{
  const auto bitmap=read<uintptr_t>(registry+0x98),capacity=read<uint64_t>(registry+0xa0),keys=read<uintptr_t>(registry+0xb8),values=read<uintptr_t>(registry+0xd0);
  const auto count=read<uint32_t>(registry+0xe4);
  if(!bitmap||!keys||!values||!capacity||capacity>0x100000||(capacity&(capacity-1))||count>capacity)throw std::runtime_error("sequence registry map bounds");
  uintptr_t resource=0;uint64_t bucket=actor.sequence&(capacity-1);
  for(uint64_t n=0;n<capacity;++n){if(!(read<uint64_t>(bitmap+(bucket/64)*8)&(1ull<<(bucket%64))))break;
   if(read<uint32_t>(keys+bucket*4)==actor.sequence){resource=read<uintptr_t>(values+bucket*8);break;}bucket=(bucket+1)&(capacity-1);
  }
  if(!resource)throw std::runtime_error("active sequence absent from native registry");
  const auto header=read<std::array<uint8_t,24>>(resource);const auto subs=relative(resource+16);const auto number=read<uint32_t>(resource+20);
  if(!subs||!number||number>32)throw std::runtime_error("native subsequence bounds");
  std::ostringstream o;o<<"\"sequence\":"<<actor.sequence<<",\"item\":"<<actor.usedItem<<",\"entity\":"<<actor.entity<<",\"trigger\":"<<actor.triggerTime<<",\"native_length\":"<<actor.length<<",\"registry\":"<<hex(registry)<<",\"bucket\":"<<bucket<<",\"capacity\":"<<capacity<<",\"resource\":"<<hex(resource)<<",\"resource_header\":"<<jsonString(blob<24>(resource))<<",\"subsequences\":[";
  for(uint32_t s=0;s<number;++s){if(s)o<<',';const auto sub=subs+s*72;const auto events=relative(sub);const auto total=read<uint32_t>(sub+4);
   if(total>256||(!events&&total))throw std::runtime_error("sequence events bounds");
   o<<"{\"index\":"<<s<<",\"address\":"<<hex(sub)<<",\"bytes\":"<<jsonString(blob<72>(sub))<<",\"events\":[";
   for(uint32_t e=0;e<total;++e){if(e)o<<',';const auto v=events+e*12;const auto type=read<uint32_t>(v);const auto payload=relative(v+4);
    o<<"{\"index\":"<<e<<",\"type_hash\":"<<type<<",\"variant_bytes\":"<<jsonString(blob<12>(v))<<",\"address\":"<<hex(payload);
    if(payload){o<<",\"base_event\":"<<jsonString(blob<96>(payload));
     if(type==0x64b5dc20u){o<<",\"spawn_impact\":"<<jsonString(blob<488>(payload));const auto shapes=relative(payload+0x19c);const auto shapeCount=read<uint32_t>(payload+0x1a0);
      if(shapeCount>16||(!shapes&&shapeCount))throw std::runtime_error("spawn collider shape bounds");
      o<<",\"shapes\":[";for(uint32_t i=0;i<shapeCount;++i){if(i)o<<',';const auto shape=shapes+i*12;const auto data=relative(shape+4);o<<"{\"type_hash\":"<<read<uint32_t>(shape)<<",\"variant_bytes\":"<<jsonString(blob<12>(shape))<<",\"address\":"<<hex(data)<<",\"base_data\":"<<jsonString(data?blob<80>(data):std::string{});if(data&&read<uint32_t>(shape)==0x88154c34u)o<<",\"box_data\":"<<jsonString(blob<96>(data));o<<'}';}o<<']';
     }
    }o<<'}';
   }o<<"]}";
  }
  const bool stable=header==read<std::array<uint8_t,24>>(resource)&&bitmap==read<uintptr_t>(registry+0x98)&&capacity==read<uint64_t>(registry+0xa0)&&keys==read<uintptr_t>(registry+0xb8)&&values==read<uintptr_t>(registry+0xd0)&&read<uint32_t>(keys+bucket*4)==actor.sequence&&read<uintptr_t>(values+bucket*8)==resource;
  o<<"],\"stable_read\":"<<(stable?"true":"false")<<",\"read_only\":true";logFile->event("sequence_asset_snapshot",o.str());
  if(!stable)throw std::runtime_error("sequence registry changed while snapshotting");captured.insert(actor.sequence);
 }catch(const std::exception& e){++failures;logFile->event("sequence_asset_failure","\"reason\":"+jsonString(e.what()));}
}
}
