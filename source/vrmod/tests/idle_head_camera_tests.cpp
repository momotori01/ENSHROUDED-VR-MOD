#include <evr/idle_head_camera.hpp>
#include <iostream>
#include <limits>
using namespace evr;using namespace evr::game;
int passed=0,failed=0;
void check(bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';ok?++passed:++failed;}
int main(){
    ClientInputBytes source{},out;for(size_t i=0;i<source.size();++i)source[i]=uint8_t(i*31+7);
    source[clientMask]=source[clientBuildingCamera]=0;source[clientInputMode]=1;
    std::fill(source.begin()+clientStick,source.begin()+clientStick+8,0);std::fill(source.begin()+clientDigital,source.begin()+clientDigital+8,0);
    const auto saved=source;IdleHeadGate g{true,true,false,129};HeadInputSnapshot h;
    h.current=true;h.flags=15;h.xrFrame=1;h.originEpoch=1;h.predictedDisplayTime=1;
    h.anchor.root={0,0,0};h.anchor.cameraOffsetGameUnits={0,1.6f,0};h.head={euler(.7f,.8f,.6f),{.03f,.04f,.05f}};
    auto r=patchIdleHeadCamera(source,out,g,h,0);
    check(r.status==IdleHeadStatus::Applied&&out!=source&&source==saved&&onlyHeadCameraChanged(source,out),"only private camera pose/distance changes; native actions/clip/UI bytes preserved");
    Quat q;WorldPositionQ32 p;float distance;
    std::memcpy(&q,out.data()+0x38,sizeof(q));std::memcpy(&p,out.data()+0x20,sizeof(p));std::memcpy(&distance,out.data()+0x48,4);
    check(length(rotate(q,{0,0,1})-r.proposal.aimForward)<1e-6&&distance==0&&p.x==r.proposal.cameraPosition.x,
        "serialized camera preserves full head forward and first person position");
    bool modes=true;for(unsigned mode=0;mode<256;++mode){auto s=source;s[clientInputMode]=uint8_t(mode);auto x=patchIdleHeadCamera(s,out,g,h,0);
        modes&=mode<=1?x.status==IdleHeadStatus::Applied:x.status==IdleHeadStatus::SpecialMode&&out==s;}
    check(modes,"explicit idle diagnostic accepts two existing modes without rewriting either");
    bool states=true;for(unsigned bit=0;bit<8;++bit){auto s=source;s[clientMask]=uint8_t(1u<<bit);auto x=patchIdleHeadCamera(s,out,g,h,0);states&=x.status==IdleHeadStatus::SpecialMode&&out==s;}
    for(unsigned bit:{1u,2u}){auto f=g;f.playerFlags|=1u<<bit;auto x=patchIdleHeadCamera(source,out,f,h,0);states&=x.status==IdleHeadStatus::PlayerState&&out==source;}
    check(states,"every special client bit and UI/target-lock flag preserves native bytes");
    bool actions=true;for(unsigned bit=0;bit<64;++bit){auto s=source;s[clientDigital+bit/8]=uint8_t(1u<<(bit%8));auto x=patchIdleHeadCamera(s,out,g,h,0);actions&=x.status==IdleHeadStatus::ActiveInput&&out==s;}
    for(float value:{-.7f,.000001f,1.f})for(unsigned axis=0;axis<2;++axis){auto s=source;std::memcpy(s.data()+clientStick+4*axis,&value,4);
        auto x=patchIdleHeadCamera(s,out,g,h,0);actions&=x.status==IdleHeadStatus::ActiveInput&&out==s;}
    check(actions,"all digital inputs and even tiny stick motion immediately fall back without synthesizing input");
    bool unavailable=true;for(unsigned flags=0;flags<15;++flags){auto t=h;t.flags=flags;auto x=patchIdleHeadCamera(source,out,g,t,0);unavailable&=x.status==IdleHeadStatus::Tracking&&out==source;}
    auto t=h;t.current=false;unavailable&=patchIdleHeadCamera(source,out,g,t,0).status==IdleHeadStatus::Tracking&&out==source;
    check(unavailable,"tracking loss rejects all partial flags without a partial camera patch");
    auto f=g;f.normalUiBound=false;check(patchIdleHeadCamera(source,out,f,h,0).status==IdleHeadStatus::Ui&&out==source,"unknown/stale/other context UI rejected");
    f=g;f.playerStateCurrent=false;check(patchIdleHeadCamera(source,out,f,h,0).status==IdleHeadStatus::PlayerState&&out==source,"stale player state rejected");
    f=g;f.freeCamera=true;check(patchIdleHeadCamera(source,out,f,h,0).status==IdleHeadStatus::SpecialMode&&out==source,"native free camera preserved");
    t=h;t.head.position.x=std::numeric_limits<float>::quiet_NaN();check(patchIdleHeadCamera(source,out,g,t,0).status==IdleHeadStatus::Invalid&&out==source,"malformed pose leaves all 1392 bytes intact");
    std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}
