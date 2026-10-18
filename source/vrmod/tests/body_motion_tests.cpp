#include <evr/body_motion.hpp>
#include <iostream>
using namespace evr::game;
int main(){unsigned pass=0,fail=0;auto check=[&](bool v,const char* name){std::cout<<(v?"PASS ":"FAIL ")<<name<<'\n';v?++pass:++fail;};
 BodyMotionSample s{1,1000,1,42,true};
 auto go=[&](const BodyMotionSample& a,uint32_t entity=42,uint64_t ms=1000,bool fp=true,bool synthetic=false){return bodyMotionOverride(a,entity,ms,fp,synthetic);};
 check(!go(s),"ordinary grounded posture keeps base distance");
 for(unsigned bit:{1,3,32,37}){s.state=1ull<<bit;check(go(s),"jump/dodge/fall/glide uses owned motion distance");s.state|=1;check(go(s),"ground contact does not prematurely cancel an active posture");}
 s.state=1ull<<37;check(go(s,42,1200)&&!go(s,42,1201)&&!go(s,42,999),"freshness inclusive boundary and future sample rejection");
 check(!go(s,43)&&!go(s,0),"different owner rejected");check(!go(s,42,1000,false)&&!go(s,42,1000,true,true),"event and synthetic presentation preserve native/base policy");
 s.state|=1ull<<7;check(!go(s),"dead motion is unvalidated");s.state=1ull<<32;
 auto a=s;a.known=false;check(!go(a),"unknown Actor rejected");a=s;a.number=0;check(!go(a),"missing sample rejected");a=s;a.milliseconds=0;check(!go(a),"missing timestamp rejected");
 std::cout<<pass<<" PASS / "<<fail<<" FAIL\n";return fail?1:0;
}
