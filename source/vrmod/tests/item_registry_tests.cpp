#include <evr/item_registry.hpp>
#include <iostream>
#include <vector>
#include <string>
using namespace evr::game;
int main(int argc,char** argv){
    const bool vectors=argc==2&&std::string(argv[1])=="--vectors";unsigned pass=0,fail=0,cases=0;
    auto check=[&](bool ok,const char* name){ok?++pass:++fail;if(!vectors)std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';};
    constexpr uint64_t base=0x6100000000,registry=base+0x100,bitmap=base+0x1000,keys=base+0x2000,values=base+0x4000,info=base+0x6000;
    std::vector<uint8_t> memory(0x10000);
    auto put=[&](uint64_t at,const auto& value){std::memcpy(memory.data()+at-base,&value,sizeof(value));};
    auto reader=[&](uint64_t at,void* out,size_t n){if(at<base||at-base>memory.size()||n>memory.size()-(at-base))return false;std::memcpy(out,memory.data()+at-base,n);return true;};
    auto setup=[&](uint64_t capacity,uint32_t count){std::fill(memory.begin(),memory.end(),0);put(registry+0x40,bitmap);put(registry+0x48,capacity);put(registry+0x60,keys);put(registry+0x78,values);put(registry+0x8c,count);};
    if(vectors)std::cout<<'[';
    for(uint64_t capacity:{1,2,8,64,128})for(uint32_t occupied:{0u,1u,3u,7u}){
        if(occupied>=capacity&&occupied)continue;
        setup(capacity,occupied);std::vector<uint32_t> ids;
        for(uint32_t j=0;j<occupied;++j){uint32_t id=uint32_t(capacity-1+capacity*(j+1));uint64_t index=(capacity-1+j)&(capacity-1),word=0;
            reader(bitmap+(index/64)*8,&word,8);word|=1ull<<(index%64);put(bitmap+(index/64)*8,word);put(keys+index*4,id);put(values+index*8,info+j*0x200);ids.push_back(id);}
        ids.push_back(uint32_t(capacity-1+capacity*20));ids.push_back(0);
        const auto before=memory;
        for(uint32_t id:ids){const auto r=lookupItem(reader,registry,id);auto it=std::find(ids.begin(),ids.end()-2,id);
            const uint64_t expected=it==ids.end()-2?0:info+uint64_t(it-ids.begin())*0x200;
            check(r.pointer==expected&&(r.status==(id==0?ItemLookupStatus::NoItem:expected?ItemLookupStatus::Found:ItemLookupStatus::Missing)),"collision/wrap/word-boundary/empty lookup");
            if(vectors){if(cases)std::cout<<',';std::cout<<"{\"capacity\":"<<capacity<<",\"occupied\":"<<occupied<<",\"id\":"<<id<<",\"pointer\":"<<r.pointer<<",\"probes\":"<<r.probes<<'}';}++cases;
        }check(memory==before,"source table unchanged");
    }
    setup(8,1);check(lookupItem(reader,0,0).status==ItemLookupStatus::NoItem,"zero id never reads registry");
    check(lookupItem(reader,0,1).status==ItemLookupStatus::Unreadable,"bad pointer rejected");
    for(uint64_t capacity:{0ull,3ull,1ull<<21}){put(registry+0x48,capacity);check(lookupItem(reader,registry,1).status==ItemLookupStatus::Invalid,"invalid capacity rejected");}
    setup(8,9);check(lookupItem(reader,registry,1).status==ItemLookupStatus::Invalid,"invalid count rejected");
    setup(8,8);put(bitmap,~uint64_t(0));check(lookupItem(reader,registry,1).status==ItemLookupStatus::Budget,"full missing-key map has bounded termination");
    setup(8,1);uint64_t word=2;put(bitmap,word);uint32_t id=1;put(keys+4,id);put(values+8,info);put(info,id);
    unsigned headers=0;auto changing=[&](uint64_t p,void* out,size_t n){bool ok=reader(p,out,n);if(p==registry+0x38&&++headers==2)static_cast<uint8_t*>(out)[0x54]^=1;return ok;};
    check(lookupItem(changing,registry,1).status==ItemLookupStatus::Changed,"changed table rejected");
    auto r=lookupItem(reader,registry,1);ItemMetadata m;
    memory[info-base+itemCategory]=2;memory[info-base+itemEquipmentSlot]=4;
    check(readItemMetadata(reader,r,1,m)&&m.rangedWeapon(),"weapon category and ranged slot required");
    bool exclusions=true;for(uint8_t category:{0,1,3,4,5,6,7,8,9,10,11,12,13,14,15,16}){memory[info-base+itemCategory]=category;exclusions &= readItemMetadata(reader,r,1,m)&&!m.rangedWeapon();}
    check(exclusions,"build tools/fishing/ammo/nonweapon categories excluded");
    memory[info-base+itemCategory]=2;bool slots=true;for(uint8_t slot:{0,1,2,3,6,7,8,29}){memory[info-base+itemEquipmentSlot]=slot;slots &= readItemMetadata(reader,r,1,m)&&!m.rangedWeapon();}
    check(slots,"other equipment slots excluded");
    check(!readItemMetadata(reader,r,999,m),"item identity mismatch rejected");
    memory[info-base+itemCategory]=255;check(!readItemMetadata(reader,r,1,m),"unknown metadata enum rejected");
    if(vectors)std::cout<<"]\n";else std::cout<<pass<<" PASS / "<<fail<<" FAIL\n";
    return fail?1:0;
}
