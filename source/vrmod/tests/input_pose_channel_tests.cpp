#include <evr/input_pose_channel.hpp>
#include <atomic>
#include <iostream>
#include <thread>
using namespace evr::game;
int passed=0,failed=0;
void check(bool ok,const char* name){std::cout<<(ok?"PASS ":"FAIL ")<<name<<'\n';ok?++passed:++failed;}
HeadInputSnapshot sample(uint64_t frame,uint64_t epoch=1){
    HeadInputSnapshot h;h.xrFrame=frame;h.originEpoch=epoch;h.predictedDisplayTime=int64_t(frame)*11111111;
    h.flags=15;h.anchor.root.x=int64_t(frame)*100;return h;
}
int main(){
    InputPoseChannel c;const InputPoseToken a{1,1},b{1,2},d{2,3};
    check(!c.read(7,100,200).head.current&&!c.publish(a,7,sample(1),100,99),"uninitialized channel cannot supply or accept pose");
    c.reset(a);auto h=sample(1);check(c.publish(a,7,h,100,99),"first generation accepts coherent sample");
    h.anchor.root.x=9999;auto r=c.read(7,100,200);
    check(r.head.current&&r.head.anchor.root.x==100&&r.token==a&&r.headAgeMs==0&&r.rootAgeMs==1,"publication owns an immutable value copy");
    check(c.read(8,100,200).status==InputPoseStatus::WrongEntity&&!c.read(8,100,200).head.current&&
          c.read(7,98,200).status==InputPoseStatus::ClockReversed&&!c.read(7,98,200).head.current,"entity changes and clock reversal reject pose");
    check(c.read(7,299,200).head.current&&c.read(7,300,200).status==InputPoseStatus::StaleRoot&&
          c.read(7,301,200).status==InputPoseStatus::StaleHead&&!c.read(7,100,0).head.current,"independent head/root age including exact boundary");
    check(!c.publish(a,7,sample(1),101,100)&&!c.publish(a,7,sample(2),99,98)&&
          !c.publish(a,7,sample(2),100,101)&&!c.publish(a,0,sample(2),100,99),"replayed frames, backwards clock, future root and zero entity rejected");
    c.invalidate();check(!c.read(7,101,200).head.current&&c.publish(a,7,sample(2),102,101),"tracking attempt invalidates prior sample until new frame published");
    NativeInputState s;s.enabled=s.known=s.local=s.ready=true;s.inputMode=1;s.playerFlags=1;
    bool lost=true;
    for(uint64_t flags=0;flags<15;flags++){
        auto p=sample(flags+3);p.flags=flags;lost&=c.publish(a,7,p,110+flags,109+flags);
        r=c.read(7,110+flags,200);lost&=r.head.flags==flags&&inputGate(s,r.head)==InputFallback::TrackingUnavailable;
    }
    check(lost,"all fifteen incomplete tracking flag combinations remain unavailable to gameplay");
    c.reset(b);check(!c.read(7,150,200).head.current&&!c.publish(a,7,sample(50),150,149)&&
          !c.publish(b,7,sample(1,1),150,149)&&c.publish(b,7,sample(1,2),150,149),"recenter rejects prior origin even within same session");
    c.reset(d);check(!c.publish(b,7,sample(100,2),160,159)&&c.publish(d,7,sample(1,3),160,159),"new session accepts reset frame counter and rejects late old publisher");
    c.close();check(!c.read(7,161,200).head.current&&!c.publish(d,7,sample(2,3),161,160),"closed session cannot be revived by a late publish");
    c.reset({3,4});std::atomic<bool> done{false},coherent{true};std::atomic<unsigned> reads{0};
    c.publish({3,4},7,sample(1,4),100,100);
    std::thread consumer([&]{while(!done.load()){
        const auto v=c.read(7,100,200);if(!v.head.current)continue;++reads;
        if(v.head.anchor.root.x!=int64_t(v.head.xrFrame)*100||v.head.predictedDisplayTime!=int64_t(v.head.xrFrame)*11111111||v.head.originEpoch!=4||v.token.session!=3)
            coherent=false;
    }});
    while(!reads.load())std::this_thread::yield();
    for(unsigned i=2;i<=50000;i++)if(!c.publish({3,4},7,sample(i,4),100,100))coherent=false;
    done=true;consumer.join();
    check(coherent.load()&&reads.load()>0&&c.read(7,100,200).head.xrFrame==50000,"50000 concurrent publications never tear frame, time, root or generation");
    std::cout<<passed<<" PASS / "<<failed<<" FAIL\n";return failed?1:0;
}
