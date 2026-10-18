#include <evr/quick_slot.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 const Pose head{{0,0,0,1},{0,1.6f,0}};                         // looking along -Z: the user's right is +X
 HandFrame f;QuickSlotSettings byHand;byHand.roll=false;QuickSlot t{byHand};uint64_t now=1000;bool allowed=true;Pose seen=head;const Vec3 start{.25f,1.2f,-.3f};
 auto reset=[&](){t=QuickSlot{byHand};f={};f.focused=true;f.sessionGeneration=f.originGeneration=1;f.predictedTime=1000000000;now=1000;allowed=true;seen=head;
  auto& h=f.hands[1];h.gripActive=h.secondaryActive=true;h.gripFlags=15;h.profile=1;h.grip.position=start;h.secondaryPressed=false;};
 auto step=[&](bool down,Vec3 offset={0,0,0},uint64_t ms=11){++f.frame;f.predictedTime+=XrDuration(ms)*1000000;now+=ms;
  f.hands[1].secondaryPressed=down;f.hands[1].grip.position=start+offset;return t.update(f,seen,allowed,now);};
 // Button up, down, and held in 20 ms frames until the wheel opens (250 ms).
 auto open=[&](){step(false);step(true);QuickSlotResult r;unsigned frames=0;for(;frames<30&&!r.opened;frames++)r=step(true,{},20);ck(r.opened&&frames>=12&&frames<=14);return r;};
 // A short press is the game's Y button: nothing while down, one tap of about 100 ms after release.
 reset();step(false);auto r=step(true);ck(!r.buttons&&!r.steer);r=step(true,{},100);ck(!r.buttons);
 r=step(false);ck(r.tapped&&r.buttons==QuickSlot::tap&&!r.steer);ck(step(false,{},90).buttons==QuickSlot::tap);ck(!step(false,{},20).buttons);ck(!t.busy());
 // Held 250 ms: the wheel opens at the hand; near the centre the stick is neutral.
 reset();step(false);step(true);r=step(true,{},240);ck(!r.buttons&&t.busy()&&!t.open());
 r=step(true,{},11);ck(r.opened&&r.buttons==QuickSlot::wheel&&r.steer&&r.x==0&&r.y==0&&t.open());
 r=step(true,{.05f,.03f,0});ck(r.buttons==QuickSlot::wheel&&r.x==0&&r.y==0);       // 5.8 cm: inside VHVR's 7 cm
 // Past 7 cm the stick points where the hand went: right and up of the centre, whatever the depth.
 r=step(true,{.06f,.08f,.2f});ck(r.steer&&std::abs(r.x-.6f)<1e-4f&&std::abs(r.y-.8f)<1e-4f&&r.buttons==QuickSlot::wheel);
 r=step(true,{-.1f,0,0});ck(std::abs(r.x+1)<1e-5f&&std::abs(r.y)<1e-5f);
 r=step(true,{-.06f,0,0});ck(std::abs(r.x+1)<1e-5f);r=step(true,{-.04f,0,0});ck(r.x==0&&r.y==0); // hysteresis down to 5 cm
 // Release on an item: the wheel button comes up, the stick stays on the item for the confirm time.
 r=step(true,{0,.12f,0});ck(std::abs(r.y-1)<1e-5f);
 r=step(false,{0,.12f,0});ck(r.chosen&&!r.buttons&&r.steer&&std::abs(r.y-1)<1e-5f&&!t.open()&&t.busy());
 r=step(false,{0,0,0},100);ck(!r.buttons&&r.steer&&std::abs(r.y-1)<1e-5f);            // the hand may already move away
 r=step(false,{0,0,0},30);ck(!r.steer&&!r.buttons&&!t.busy());
 // Release at the centre: nothing is chosen.
 reset();open();r=step(false);ck(r.cancelled&&!r.chosen&&!r.buttons&&r.x==0&&r.y==0);
 // The sideways direction is the head's at the moment of opening, and stays so.
 reset();seen={euler(pi/2,0,0),{0,1.6f,0}};open();seen=head;                          // head turned 90 degrees to its left: right is then -Z
 r=step(true,{0,0,-.1f});ck(std::abs(r.x-1)<1e-4f&&std::abs(r.y)<1e-4f);
 // Scope loss lets go at once; the button must come up before it works again.
 for(unsigned bad=0;bad<5;bad++){reset();open();
  if(bad==0)allowed=false;if(bad==1)f.focused=false;if(bad==2)f.hands[1].gripFlags=0;if(bad==3)f.originGeneration++;if(bad==4)f.hands[1].secondaryActive=false;
  r=step(true);ck(r.cancelled&&!r.buttons&&!r.steer);allowed=true;f.focused=true;f.hands[1].gripFlags=15;f.hands[1].secondaryActive=true;
  for(unsigned i=0;i<20;i++){r=step(true,{},20);ck(!r.buttons&&!r.opened);}open();}
 // Held at start-up: nothing until the button has been up once.
 reset();for(unsigned i=0;i<20;i++){r=step(true,{},20);ck(!r.buttons&&!r.opened&&!t.busy());}open();
 // A tracking gap ends an open wheel.
 reset();open();r=step(true,{},400);ck(r.cancelled&&!r.buttons);
 // By the wrist's roll (2026-10-18, the default): the twist about the controller's forward axis, twice over, from the top
 // clockwise; 12 degrees to point, under 8 back at the centre; the hand may move meanwhile.
 {QuickSlot d;HandFrame g;g.focused=true;g.sessionGeneration=g.originGeneration=1;g.predictedTime=1000000000;uint64_t ms=1000;
  auto& h=g.hands[1];h.gripActive=h.secondaryActive=true;h.gripFlags=15;h.profile=1;h.grip={{0,0,0,1},start};
  // The wrist from the centre (45 degrees counter-clockwise from level): twistDegrees to the right = roll 45 - twist.
  auto go=[&](bool down,float twistDegrees,Vec3 move={0,0,0},uint64_t dt=20){++g.frame;g.predictedTime+=XrDuration(dt)*1000000;ms+=dt;
   h.secondaryPressed=down;h.grip.position=start+move;h.grip.orientation=euler(.2f,.2f,(45-twistDegrees)*pi/180);return d.update(g,head,true,ms);};
  ck(d.settings().roll&&!d.settings().rollFromOpen);go(false,0);go(true,0);QuickSlotResult q;for(unsigned i=0;i<15&&!q.opened;++i)q=go(true,0);ck(q.opened&&q.x==0&&q.y==0);
  q=go(true,10,{.2f,.1f,0});ck(q.x==0&&q.y==0&&std::abs(d.twist()-10*pi/180)<1e-3f);     // under 12 degrees: centre, whatever the hand's place
  q=go(true,22.5f);ck(std::abs(q.x-std::sin(pi/4))<1e-3f&&std::abs(q.y-std::cos(pi/4))<1e-3f);   // 22.5 to the right: the top-right slot
  q=go(true,45);ck(std::abs(q.x-1)<1e-3f&&std::abs(q.y)<1e-3f);                                // a quarter of the wheel
  q=go(true,-90);ck(std::abs(q.x)<1e-3f&&std::abs(q.y+1)<1e-3f);                               // to the left, a quarter turn: the bottom
  q=go(true,-30);ck(std::abs(q.x+std::sin(pi/3))<1e-3f&&std::abs(q.y-.5f)<1e-3f);
  q=go(true,-10);ck(q.x!=0);q=go(true,-7);ck(q.x==0&&q.y==0);                                  // hysteresis: still at 10, centre under 8
  q=go(true,30);q=go(false,30);ck(q.chosen&&q.steer&&std::abs(q.x-std::sin(pi/3))<1e-3f);}     // let go on a slot: chosen
 // The centre is fixed, not the opening's: a wheel opened with the wrist already 30 degrees to the right points at once.
 {QuickSlot d;HandFrame g;g.focused=true;g.sessionGeneration=g.originGeneration=1;g.predictedTime=1000000000;uint64_t ms=1000;
  auto& h=g.hands[1];h.gripActive=h.secondaryActive=true;h.gripFlags=15;h.profile=1;h.grip={euler(-.3f,.1f,15*pi/180),start};
  auto step=[&](bool down){++g.frame;g.predictedTime+=20000000;ms+=20;h.secondaryPressed=down;return d.update(g,head,true,ms);};
  step(false);QuickSlotResult q;for(unsigned i=0;i<20&&!q.opened;++i)q=step(true);ck(q.opened);
  q=step(true);ck(std::abs(q.x-std::sin(pi/3))<1e-3f&&std::abs(q.y-.5f)<1e-3f&&std::abs(d.twist()-pi/6)<1e-3f);}
 // levelRoll: the Euler roll for a level or pitched controller, whatever its heading; none pointing straight up.
 for(float yaw:{0.f,1.f,-2.5f})for(float pitch:{0.f,.4f,-.6f})for(float r:{0.f,.7854f,-1.2f,2.5f})ck(std::abs(QuickSlot::levelRoll(euler(yaw,pitch,r))-r)<1e-3f);
 ck(QuickSlot::levelRoll(euler(0,pi/2,.5f))==0);
 // From the opening (rollFromOpen, 14:40): the same twist as before counts from where the wheel opened.
 {QuickSlotSettings fromOpen;fromOpen.rollFromOpen=true;QuickSlot d{fromOpen};HandFrame g;g.focused=true;g.sessionGeneration=g.originGeneration=1;g.predictedTime=1000000000;uint64_t ms=1000;
  auto& h=g.hands[1];h.gripActive=h.secondaryActive=true;h.gripFlags=15;h.profile=1;h.grip={euler(0,0,1.f),start};
  auto step=[&](bool down,float roll){++g.frame;g.predictedTime+=20000000;ms+=20;h.secondaryPressed=down;h.grip.orientation=euler(0,0,roll);return d.update(g,head,true,ms);};
  step(false,1.f);QuickSlotResult q;for(unsigned i=0;i<20&&!q.opened;++i)q=step(true,1.f);ck(q.opened&&q.x==0);
  q=step(true,1.f);ck(q.x==0&&q.y==0);q=step(true,1.f-pi/4);ck(std::abs(q.x-1)<1e-3f);}
 // The roll is about the controller's own axis, not the room's: the same twist with the hand pointed elsewhere.
 {const Quat pointed=euler(1.f,.5f,0);ck(std::abs(QuickSlot::rollBetween(pointed,multiply(pointed,euler(0,0,-.5f)))-.5f)<1e-4f);
  ck(std::abs(QuickSlot::rollBetween(pointed,multiply(pointed,euler(.3f,0,0))))<1e-4f);}    // turning the hand sideways is no roll
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
