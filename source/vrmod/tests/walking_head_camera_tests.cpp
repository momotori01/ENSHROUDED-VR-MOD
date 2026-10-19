#include <evr/walking_head_camera.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
using namespace evr;using namespace evr::game;
int main(int argc,char** argv){
    unsigned passed=0,failed=0;
    auto check=[&](bool ok,const char* text){std::cout<<(ok?"PASS ":"FAIL ")<<text<<'\n';ok?++passed:++failed;};
    ClientInputBytes source{},out;
    for(size_t i=0;i<source.size();++i)source[i]=uint8_t(i*31+7);
    source[clientMask]=source[clientBuildingCamera]=0;source[clientInputMode]=1;
    std::fill(source.begin()+clientDigital,source.begin()+clientDigital+8,0);
    WalkingHeadGate gate;gate.player={true,true,false,129};gate.actorStateCurrent=true;
    gate.gravityPresent=gate.gravityActive=true;gate.gravityDirection={0,-1,0};
    gate.actorState=(1ull<<0)|(1ull<<50);
    HeadInputSnapshot head;head.current=true;head.flags=15;head.xrFrame=1;head.originEpoch=1;head.predictedDisplayTime=1;
    head.anchor.cameraOffsetGameUnits={0,1.6f,0};
    bool all=true;unsigned count=0,applyFailures=0;float maxForwardError=0;std::ofstream vectors;if(argc==2){vectors.open(argv[1]);vectors<<'['<<std::setprecision(10);}
    for(float yaw:{-pi,-pi/2,-.4f,0.f,.7f,pi/2,pi})
      for(float pitch:{-pi/2,-1.55f,-.8f,0.f,.8f,1.55f,pi/2})
        for(float roll:{-.7f,0.f,.7f})
          for(auto stick:{std::array<float,2>{0,1},{1,0},{0,-1},{-1,0},{.4f,.6f}}){
            head.head={euler(yaw,pitch,roll),{.2f,-.3f,.4f}};
            std::memcpy(source.data()+clientStick,stick.data(),sizeof(stick));const auto saved=source;
            // At vertical pitch preserve the last known yaw, never physical roll.
            const auto r=patchWalkingHeadCamera(source,out,gate,head,yaw);
            applyFailures+=r.status!=IdleHeadStatus::Applied;
            const auto expected=rotate(euler(r.proposal.yaw,0,0),{stick[0],0,stick[1]});
            all &= r.status==IdleHeadStatus::Applied&&source==saved&&onlyHeadCameraChanged(source,out)&&
                std::equal(source.begin()+clientStick,source.begin()+clientStick+8,out.begin()+clientStick)&&
                length(r.proposal.worldMovement-expected)<1e-6f&&r.proposal.worldMovement.y==0;
            Quat q;std::memcpy(&q,out.data()+0x38,sizeof(q));
            const auto forwardError=length(rotate(q,{0,0,1})-r.proposal.aimForward);
            maxForwardError=std::max(maxForwardError,forwardError);all &= forwardError<2e-6f;
            if(vectors){if(count)vectors<<',';vectors<<"{\"aim_q\":["<<q.x<<','<<q.y<<','<<q.z<<','<<q.w<<"],\"stick\":["<<stick[0]<<','<<stick[1]
                <<"],\"movement\":["<<expected.x<<','<<expected.y<<','<<expected.z<<"]}";}++count;
          }
    if(vectors){vectors<<']';vectors.close();}
    check(all,"735 camera-to-native-input vectors preserve controls, horizontal movement and head forward");
    std::cout<<"Applied failures "<<applyFailures<<" maximum head forward error "<<maxForwardError<<'\n';
    bool states=true;for(unsigned bit=0;bit<64;++bit){auto g=gate;g.actorState|=1ull<<bit;
        auto r=patchWalkingHeadCamera(source,out,g,head,0);
        states &= (ordinaryWalkingStates&(1ull<<bit))?r.status==IdleHeadStatus::Applied:r.status==IdleHeadStatus::SpecialMode&&out==source;}
    check(states,"unknown/busy/climbing/flying/fishing actor states retain native input");
    bool actions=true;for(unsigned bit=0;bit<64;++bit){auto s=source;s[clientDigital+bit/8]=uint8_t(1u<<(bit%8));
        auto r=patchWalkingHeadCamera(s,out,gate,head,0);actions &= r.status==IdleHeadStatus::ActiveInput&&out==s;}
    check(actions,"every digital action remains native in this walking-only stage");
    bool modes=true;for(unsigned mode=0;mode<256;++mode){auto s=source;s[clientInputMode]=uint8_t(mode);auto r=patchWalkingHeadCamera(s,out,gate,head,0);
        modes &= mode==1?r.status==IdleHeadStatus::Applied:r.status==IdleHeadStatus::SpecialMode&&out==s;}
    check(modes,"only original gamepad mode is accepted and never synthesized");
    auto idleKeyboard=source;idleKeyboard[clientInputMode]=0;
    std::fill(idleKeyboard.begin()+clientStick,idleKeyboard.begin()+clientStick+8,0);
    check(patchWalkingHeadCamera(idleKeyboard,out,gate,head,0).status==IdleHeadStatus::Applied&&out[clientInputMode]==0&&onlyHeadCameraChanged(idleKeyboard,out),
          "idle keyboard retains established head camera without synthesizing gamepad mode");
    bool special=true;for(unsigned n=0;n<7;++n){auto g=gate;
        switch(n){case 0:g.actorStateCurrent=false;break;case 1:g.gravityPresent=false;break;case 2:g.gravityActive=false;break;
        case 3:g.gravityDirection={1,0,0};break;case 4:g.actorState&=~1ull;break;case 5:g.actorState&=~(1ull<<50);break;
        case 6:g.gravityDirection.x=std::numeric_limits<float>::quiet_NaN();break;}
        special &= patchWalkingHeadCamera(source,out,g,head,0).status==IdleHeadStatus::SpecialMode&&out==source;}
    check(special,"unproven grounded gravity state is rejected without partial writes");
    auto g=gate;g.player.normalUiBound=false;check(patchWalkingHeadCamera(source,out,g,head,0).status==IdleHeadStatus::Ui&&out==source,"UI bypasses head walking");
    bool melee=true;for(unsigned bits=0;bits<16;bits++){
        auto src=source;uint64_t digital=bits;std::memcpy(src.data()+clientDigital,&digital,8);auto mg=gate;mg.actorState|=1ull<<5;
        auto r=patchMeleeHeadCamera(src,out,mg,head,0,true);
        melee &= r.status==IdleHeadStatus::Applied&&onlyHeadCameraChanged(src,out)&&std::equal(src.begin()+clientDigital,src.begin()+clientDigital+8,out.begin()+clientDigital);
    }check(melee,"explicit melee head aim preserves every original mainhand button combination");
    bool other=true;for(unsigned bit=4;bit<64;bit++){auto src=source;uint64_t digital=1ull<<bit;std::memcpy(src.data()+clientDigital,&digital,8);other &= patchMeleeHeadCamera(src,out,gate,head,0,true).status==IdleHeadStatus::ActiveInput&&out==src;}
    check(other,"melee extension does not intercept any other native action");
    {auto src=source;src[clientDigital]=1;auto mg=gate;mg.actorState|=1ull<<5;
     check(patchMeleeHeadCamera(src,out,mg,head,0,false).status==IdleHeadStatus::SpecialMode&&out==src,"unproven melee item cannot broaden the walking state gate");
     mg.player.normalUiBound=false;check(patchMeleeHeadCamera(src,out,mg,head,0,true).status==IdleHeadStatus::Ui&&out==src,"UI bypasses melee head aim and preserves buttons");
     mg=gate;mg.actorState|=1ull<<7;check(patchMeleeHeadCamera(src,out,mg,head,0,true).status==IdleHeadStatus::SpecialMode&&out==src,"dead actor cannot enter melee head mode");
     src[clientBuildingCamera]=1;check(patchMeleeHeadCamera(src,out,gate,head,0,true).status==IdleHeadStatus::SpecialMode&&out==src,"building camera keeps native aiming");}
    // Live VR locomotion (2026-10-09): buttons and airborne states also take the head camera; only the camera changes.
    {head.head={euler(.7f,.2f,.1f),{.2f,-.3f,.4f}};bool vr=true;
     for(unsigned bit=0;bit<64;++bit){auto s=source;s[clientDigital+bit/8]=uint8_t(1u<<(bit%8));
         auto r=patchVrLocomotionCamera(s,out,gate,head,0);vr&=r.status==IdleHeadStatus::Applied&&onlyHeadCameraChanged(s,out);}
     check(vr,"live VR locomotion keeps every digital action and steers it with the head camera");
     bool air=true;for(uint64_t state:{0ull,1ull<<50,~0ull}){auto g=gate;g.actorState=state;auto r=patchVrLocomotionCamera(source,out,g,head,0);
         air&=r.status==IdleHeadStatus::Applied&&onlyHeadCameraChanged(source,out);}
     check(air,"live VR locomotion steers airborne/glide/any actor state with the head camera");
     auto k=source;k[clientInputMode]=0;check(patchVrLocomotionCamera(k,out,gate,head,0).status==IdleHeadStatus::SpecialMode&&out==k,"keyboard play keeps the native camera");
     auto g=gate;g.player.normalUiBound=false;check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::Ui&&out==source,"UI keeps native input in VR locomotion");
     g=gate;g.gravityDirection={0,1,0};check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::SpecialMode&&out==source,"unusual gravity keeps native input");
     g=gate;g.player.playerFlags|=targetLockFlag;check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::Applied,"a lock keeps the headset (lock-on movement removed)");
     g=gate;g.player.playerFlags|=1u<<12;check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::Applied,"1.0 WantsToWalk (bit 12) keeps the headset");
     auto f=source;f[clientMask]=1;check(patchVrLocomotionCamera(f,out,gate,head,0).status==IdleHeadStatus::SpecialMode&&out==f,"special camera keeps native input");
     // Swimming (2026-10-13): the game turns gravity off in the water; the head camera steers there too.
     bool swim=true;for(uint64_t water:{1ull<<43,1ull<<44,(1ull<<43)|(1ull<<44)|(1ull<<18)|(1ull<<50)}){g=gate;g.gravityActive=false;g.actorState=water;
         auto r=patchVrLocomotionCamera(source,out,g,head,0);swim&=r.status==IdleHeadStatus::Applied&&onlyHeadCameraChanged(source,out);}
     check(swim,"swimming and diving (gravity off in the water) take the head camera");
     g=gate;g.gravityActive=false;g.actorState=(1ull<<34)|(1ull<<50);check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::SpecialMode&&out==source,"gravity off out of the water (climbing) keeps native input");
     g=gate;g.gravityActive=false;g.actorState=1ull<<38;check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::SpecialMode&&out==source,"hanging on the grappling hook keeps native input");
     g=gate;g.gravityActive=false;g.actorState=1ull<<44;g.swimHead=false;check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::SpecialMode&&out==source,"swimming keeps native input when the head camera for it is switched off");
     g=gate;g.gravityActive=false;g.gravityPresent=false;g.actorState=1ull<<44;check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::SpecialMode&&out==source,"swimming without a gravity component keeps native input");
     g=gate;g.gravityActive=false;g.actorState=1ull<<44;g.gravityDirection={0,1,0};check(patchVrLocomotionCamera(source,out,g,head,0).status==IdleHeadStatus::SpecialMode&&out==source,"swimming under unusual gravity keeps native input");}
    head.current=false;check(patchWalkingHeadCamera(source,out,gate,head,0).status==IdleHeadStatus::Tracking&&out==source,"tracking loss keeps original controls");
    check(patchVrLocomotionCamera(source,out,gate,head,0).status==IdleHeadStatus::Tracking&&out==source,"tracking loss keeps original controls in VR locomotion");
    std::cout<<passed<<" PASS / "<<failed<<" FAIL; vectors "<<count<<'\n';return failed?1:0;
}
