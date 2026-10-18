#pragma once
// Read-only reconstruction of fe5fe0 -> fe5f30, checked on all304 native records
// and collision/missing-key cases in Unicorn. Never invokes a native helper.
namespace nativeAttributes {
struct Attribute {std::array<uint8_t,32> bytes{};std::string name;};
inline Attribute lookup(uintptr_t registry,uint32_t key){
 if(!registry)throw std::runtime_error("attribute registry absent");
 const auto table=registry+0x68;const auto header=read<std::array<uint8_t,88>>(table);
 uintptr_t bits=0,capacity=0,keys=0,values=0;uint32_t count=0;
 std::memcpy(&bits,header.data()+8,8);std::memcpy(&capacity,header.data()+16,8);std::memcpy(&keys,header.data()+40,8);std::memcpy(&values,header.data()+64,8);std::memcpy(&count,header.data()+84,4);
 if(!bits||!keys||!values||!count||count>=capacity||capacity>16384||(capacity&(capacity-1)))throw std::runtime_error("attribute map bounds");
 auto bucket=uintptr_t(key)&(capacity-1);bool found=false;
 for(uintptr_t n=0;n<capacity;++n){if(!(read<uint64_t>(bits+8*(bucket/64))&(1ull<<(bucket%64))))break;if(read<uint32_t>(keys+4*bucket)==key){found=true;break;}bucket=(bucket+1)&(capacity-1);}
 if(!found)throw std::runtime_error("attribute key missing");
 Attribute result;const auto at=values+32*bucket;result.bytes=read<std::array<uint8_t,32>>(at);
 uintptr_t text=0,length=0;std::memcpy(&text,result.bytes.data()+16,8);std::memcpy(&length,result.bytes.data()+24,8);
 if(!text||!length||length>256)throw std::runtime_error("attribute name bounds");
 result.name.reserve(length);for(size_t i=0;i<length;++i)result.name.push_back(char(read<uint8_t>(text+i)));
 for(size_t i=0;i<length;++i)if(result.name[i]!=char(read<uint8_t>(text+i)))throw std::runtime_error("attribute name changed");
 if(header!=read<std::array<uint8_t,88>>(table)||result.bytes!=read<std::array<uint8_t,32>>(at)||read<uint32_t>(keys+4*bucket)!=key||!(read<uint64_t>(bits+8*(bucket/64))&(1ull<<(bucket%64))))throw std::runtime_error("attribute map changed");
 return result;
}
}
