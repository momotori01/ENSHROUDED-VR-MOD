#include <evr/weapon_category.hpp>
#include <iostream>
#include <string>
#include <stdexcept>
#include <vector>
using namespace evr::game;
int main(int argc,char** argv){bool vectors=argc==2;unsigned checks=0,cases=0;try{
 auto check=[&](bool b){++checks;if(!b)throw std::runtime_error("category invariant "+std::to_string(checks));};
 constexpr uint64_t base=0x6100000000,reg=base+0x100,bits=base+0x1000,keys=base+0x2000,vals=base+0x4000,resource=base+0x6000,item=base+0x9000;
 std::vector<uint8_t> mem(0x10000);auto put=[&](uint64_t at,const auto& v){std::memcpy(mem.data()+at-base,&v,sizeof(v));};
 auto read=[&](uint64_t p,void* v,size_t n){if(p<base||p-base>mem.size()||n>mem.size()-(p-base))return false;std::memcpy(v,mem.data()+p-base,n);return true;};
 auto setup=[&](unsigned capacity,unsigned used){std::fill(mem.begin(),mem.end(),0);put(reg+0x248+8,bits);put(reg+0x248+0x10,uint64_t(capacity));put(reg+0x248+0x28,keys);put(reg+0x248+0x40,vals);put(reg+0x248+0x54,used);};   // 1.0: the map at +0x248 (+0x2c0)
 if(vectors)std::cout<<'[';
 for(unsigned capacity:{2,8,64,128})for(unsigned collisions:{0,1,5})for(uint8_t type=0;type<16;type++){
  if(collisions+1>=capacity)continue;WeaponCategoryGuid guid{};guid[0]=type+1;guid[15]=0x82;
  const uint32_t hash=weaponCategoryHash(guid);setup(capacity,collisions+1);auto bucket=hash&(capacity-1);
  for(unsigned n=0;n<=collisions;n++){
   uint64_t i=(bucket+n)&(capacity-1),word=0;read(bits+(i/64)*8,&word,8);put(bits+(i/64)*8,word|(1ull<<(i%64)));
   put(keys+i*4,n==collisions?hash:hash+uint32_t((n+1)*capacity));put(vals+i*8,resource);
  }
  std::memcpy(mem.data()+resource-base,guid.data(),16);mem[resource-base+84]=type;const auto before=mem;
  auto r=lookupWeaponCategory(read,reg,guid);check(r.status==ItemLookupStatus::Found&&r.pointer==resource&&r.type==type&&r.probes==collisions+1);check(before==mem);
  check(r.twoHandMelee()==(type>=7&&type<=10));check(r.oneHandMelee()==(type<=3||type==13));
  if(vectors){if(cases)std::cout<<',';std::cout<<"{\"capacity\":"<<capacity<<",\"collisions\":"<<collisions<<",\"type\":"<<unsigned(type)<<",\"guid\":[";for(unsigned i=0;i<16;i++){if(i)std::cout<<',';std::cout<<unsigned(guid[i]);}std::cout<<"],\"hash\":"<<hash<<",\"pointer\":"<<r.pointer<<",\"index\":"<<r.index<<'}';}cases++;
 }
 if(vectors){std::cout<<"]\n";return 0;}
 WeaponCategoryGuid guid{};check(weaponCategoryHash(guid)==0&&lookupWeaponCategory(read,0,guid).status==ItemLookupStatus::NoItem);guid[0]=1;
 setup(8,0);check(lookupWeaponCategory(read,reg,guid).status==ItemLookupStatus::Missing);
 const auto hash=weaponCategoryHash(guid);const auto i=hash&7;setup(8,1);put(bits,1ull<<i);put(keys+i*4,hash);put(vals+i*8,resource);std::memcpy(mem.data()+resource-base,guid.data(),16);
 ItemLookup info;info.status=ItemLookupStatus::Found;info.pointer=item;put(item,uint32_t(123));put(item+itemWeaponCategoryReference,guid);
 check(itemWeaponCategory(read,reg,info,123).status==ItemLookupStatus::Found);check(itemWeaponCategory(read,reg,info,124).status==ItemLookupStatus::Invalid);
 mem[resource-base]^=1;check(lookupWeaponCategory(read,reg,guid).status==ItemLookupStatus::Invalid);mem[resource-base]^=1;
 mem[resource-base+84]=16;check(lookupWeaponCategory(read,reg,guid).status==ItemLookupStatus::Invalid);mem[resource-base+84]=0;
 unsigned reads=0;auto changed=[&](uint64_t p,void* v,size_t n){bool ok=read(p,v,n);if(p==vals+i*8&&++reads==2)static_cast<uint8_t*>(v)[0]^=1;return ok;};check(lookupWeaponCategory(changed,reg,guid).status==ItemLookupStatus::Changed);
 for(uint64_t cap:{0ull,3ull,1ull<<21}){put(reg+0x248+0x10,cap);check(lookupWeaponCategory(read,reg,guid).status==ItemLookupStatus::Invalid);}
 setup(8,8);put(bits,uint64_t(255));check(lookupWeaponCategory(read,reg,guid).status==ItemLookupStatus::Budget);
 std::cout<<"PASS "<<checks<<" weapon category read-only checks\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
