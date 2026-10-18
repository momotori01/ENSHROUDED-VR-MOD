#include <evr/motion_collider_frame.hpp>
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace evr::motion;
int main(){unsigned count=0;try{
 auto check=[&](bool v){++count;if(!v)throw std::runtime_error("coherent frame check "+std::to_string(count));};
 ColliderFrameChannel channel;ColliderPose p;p.valid=true;p.frame=522;p.serial=2834;p.session=1;p.origin=1;p.predictedTime=5800;p.milliseconds=1000;
 HandFrame h;h.frame=p.frame;h.sessionGeneration=p.session;h.originGeneration=p.origin;h.predictedTime=p.predictedTime;
 check(channel.publish(p,h,1000));check(channel.snapshot().coherent());
 // Reproduce the actual 522 hand / 523 weapon publication window. A staged
 // successor must never affect the public current pair or invalidate it.
 auto pending=p;++pending.frame;++pending.predictedTime;++pending.serial;
 check(channel.snapshot().pose.frame==522&&channel.snapshot().hands.frame==522);
 check(!channel.publish(pending,h,1001));check(!channel.snapshot().pose.valid);
 ++h.frame;++h.predictedTime;check(channel.publish(pending,h,1001));
 for(unsigned i=0;i<7;++i){auto q=pending;auto hands=h;uint64_t ms=1001;
  switch(i){case 0:++hands.sessionGeneration;break;case 1:++hands.originGeneration;break;case 2:--hands.frame;break;case 3:++hands.predictedTime;break;case 4:q.valid=false;break;case 5:ms=999;break;case 6:q.serial=0;break;}
  check(!channel.publish(q,hands,ms));check(!channel.snapshot().pose.valid);
 }
 check(channel.publish(pending,h,1001));h.focused=false;h.hands[1].gripActive=false;h.hands[0].grab=false;
 check(channel.publish(pending,h,1001));check(!channel.snapshot().hands.focused&&!channel.snapshot().hands.hands[0].grab);
 channel.invalidate();check(!channel.snapshot().pose.valid);
 std::atomic<bool> finished{false};std::atomic<unsigned> reads{0},failures{0};
 std::vector<std::thread> readers;
 for(unsigned i=0;i<3;++i)readers.emplace_back([&]{do{auto s=channel.snapshot();if(s.pose.valid&&!s.coherent())++failures;++reads;std::this_thread::yield();}while(!finished.load());});
 for(unsigned i=1;i<=20000;++i){p.frame=i;p.serial=i;p.milliseconds=i;p.predictedTime=i;h.frame=i;h.predictedTime=i;channel.publish(p,h,i);if(i%13==0)channel.invalidate();if(i%101==0)std::this_thread::yield();}
 finished=true;for(auto& t:readers)t.join();check(reads>0&&failures==0);channel.invalidate();check(!channel.snapshot().coherent());
 std::cout<<"PASS "<<count<<" boundary checks / 20000 publications / "<<reads<<" concurrent snapshots\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
