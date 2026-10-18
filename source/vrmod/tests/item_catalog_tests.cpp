#include <evr/item_catalog.hpp>
#include <iostream>
using namespace evr::game;
int main(){try{
 constexpr uint64_t base=0x6200000000,registry=base+0x100,bits=base+0x1000,keys=base+0x2000,values=base+0x3000,item=base+0x4000;
 std::vector<uint8_t> mem(0x10000);auto put=[&](uint64_t at,const auto& v){std::memcpy(mem.data()+at-base,&v,sizeof(v));};
 auto reader=[&](uint64_t at,void* out,size_t n){if(at<base||at-base>mem.size()||n>mem.size()-(at-base))return false;std::memcpy(out,mem.data()+at-base,n);return true;};
 auto reset=[&](){std::fill(mem.begin(),mem.end(),0);put(registry+0x40,bits);put(registry+0x48,uint64_t(8));put(registry+0x60,keys);put(registry+0x78,values);put(registry+0x8c,uint32_t(1));put(bits,uint64_t(2));put(keys+4,uint32_t(1));put(values+8,item);put(item,uint32_t(1));put(item+itemCategory,uint8_t(2));put(item+itemEquipmentSlot,uint8_t(4));};
 auto require=[](bool b){if(!b)throw std::runtime_error("catalog test invariant");};
 auto rejects=[&](const auto& source){bool bad=false;auto read=source;try{snapshotItemCatalog(read,registry);}catch(const std::runtime_error&){bad=true;}require(bad);};
 reset();auto before=mem;auto r=snapshotItemCatalog(reader,registry);require(mem==before&&r.items.size()==1&&r.items[0].selected&&r.items[0].data[itemEquipmentSlot]==4);
 // Relative blob offsets are relative to their own fields, not the item base.
 put(item+0x790,uint32_t(0x100));put(item+0x794,uint32_t(3));put(item+0x890,uint32_t(0x00636261));
 put(item+0x49c,uint32_t(0x500));put(item+0x4a0,uint32_t(1));put(item+0x99c,uint32_t(123));
 r=snapshotItemCatalog(reader,registry);require(r.items[0].name==std::vector<uint8_t>{'a','b','c'}&&r.items[0].models.size()==20&&r.items[0].models[0]==123);
 for(uint64_t capacity:{0ull,3ull,262144ull}){reset();put(registry+0x48,capacity);rejects(reader);}
 reset();put(registry+0x8c,uint32_t(2));rejects(reader);
 reset();put(values+8,uint64_t(0));rejects(reader);
 reset();put(item+itemCategory,uint8_t(255));rejects(reader);
 reset();put(bits,uint64_t(6));put(keys+8,uint32_t(1));put(values+16,item);put(registry+0x8c,uint32_t(2));rejects(reader);
 reset();put(item+0x794,uint32_t(600));rejects(reader);
 reset();auto unreadable=[&](uint64_t at,void* out,size_t n){return n!=1944&&reader(at,out,n);};rejects(unreadable);
 reset();unsigned copies=0;auto changing=[&](uint64_t at,void* out,size_t n){bool ok=reader(at,out,n);if(at==item&&n==1944&&++copies==2)static_cast<uint8_t*>(out)[100]^=1;return ok;};rejects(changing);
 reset();unsigned tables=0;auto tableChange=[&](uint64_t at,void* out,size_t n){bool ok=reader(at,out,n);if(at==keys&&n==32&&++tables==2)static_cast<uint8_t*>(out)[4]^=1;return ok;};rejects(tableChange);
 reset();put(item+itemCategory,uint8_t(1));r=snapshotItemCatalog(reader,registry);require(!r.items[0].selected);
 std::cout<<"PASS catalog bounds, identities, relative blobs, snapshots, race rejection and source preservation\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
