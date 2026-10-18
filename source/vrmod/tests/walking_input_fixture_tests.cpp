#include <evr/walking_input_fixture.hpp>
#include <iostream>
using namespace evr;using namespace evr::game;
int main(){
    unsigned pass=0,fail=0;
    auto check=[&](bool value,const char* name){std::cout<<(value?"PASS ":"FAIL ")<<name<<'\n';value?++pass:++fail;};
    WorldPositionQ32 root{};
    auto start=[&](WalkingInputFixture& fixture){WalkFixtureCommand r;for(unsigned i=1;i<=16;++i)r=fixture.update(i*10,1,root,1,true,true);return r;};
    WalkingInputFixture fixture;auto first=start(fixture);
    check(first.synthetic&&first.elapsedMs==0&&first.stick==std::array<float,2>{0,0},"requires consecutive known idle state and settles before motion");
    std::array<unsigned,4> pulseTicks{};bool bounded=true;
    for(unsigned ms=10;ms<WalkingInputFixture::runMs;ms+=10){
        const auto r=fixture.update(160+ms,1,root,1,true,true);
        bounded &= r.synthetic&&std::abs(r.stick[0])<=.25f&&std::abs(r.stick[1])<=.25f;
        if(r.stick[0]||r.stick[1])++pulseTicks[r.phase-1];
    }
    check(bounded&&pulseTicks==std::array<unsigned,4>{25,25,25,25},"four bounded quarter-second pulses with zero intervals");
    auto end=fixture.update(160+WalkingInputFixture::runMs,1,root,1,true,true);
    check(end.state==WalkFixtureState::Complete&&!end.synthetic&&!fixture.update(999999,1,root,1,true,true).synthetic,"one-shot completion never replays");
    bool guards=true;
    for(unsigned n=0;n<7;++n){
        WalkingInputFixture f;start(f);auto moved=root;if(n==3)moved.x=int64_t(1ull<<32);
        auto r=f.update(n==0?100:200,n==1?2:1,moved,n==4?0:1,n!=2,n!=5,n==6?2:1);
        guards &= r.state==WalkFixtureState::Aborted&&!r.synthetic&&!f.update(300,1,root,1,true,true).synthetic;
    }
    check(guards,"clock/recenter/tracking/travel/scale/native controls latch an abort");
    WalkingInputFixture never;never.update(100,1,root,1,false,true);
    check(never.update(15100,1,root,1,false,true).state==WalkFixtureState::Aborted,"no indefinite wait for safe state");
    ClientInputBytes source{},output;output=source;
    check(nativeControlsIdle(source),"zero real input accepted as idle");
    bool actions=true;for(unsigned bit=0;bit<64;++bit){auto s=source;s[clientDigital+bit/8]=uint8_t(1u<<(bit%8));actions &= !nativeControlsIdle(s);}
    check(actions,"every native digital action blocks synthesis");
    WalkFixtureCommand command;command.synthetic=true;command.stick={.25f,-.25f};
    writeFixtureControls(output,command);check(source==ClientInputBytes{}&&output[clientInputMode]==1&&!nativeControlsIdle(output)&&onlyFixtureAndCameraChanged(source,output),"only private analog axes and mode are synthesized");
    bool prohibited=true;for(auto at:{size_t(clientDigital),size_t(clientMask),size_t(clientBuildingCamera),size_t(0x100)}){auto bad=output;bad[at]^=1;prohibited &= !onlyFixtureAndCameraChanged(source,bad);}
    check(prohibited,"actions/UI/padding cannot change through fixture whitelist");
    std::cout<<pass<<" PASS / "<<fail<<" FAIL\n";return fail?1:0;
}
