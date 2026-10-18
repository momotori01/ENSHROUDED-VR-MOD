#include <evr/room_sneak.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 // Crouching: under 70 % of 1.65 m (a drop of more than 0.495 m) crouched, back over 73.5 % (a drop under 0.43725 m).
 {RoomCrouch c;
  ck(!c.update(0)&&!c.update(-.3f)&&!c.update(-.49f));
  ck(c.update(-.5f));                                   // 0.6970
  ck(c.update(-.45f)&&c.update(-.44f));                 // between the two: stays crouched
  ck(!c.update(-.43f));                                 // 0.7394: stood up
  ck(!c.update(-.45f));                                 // between the two again: stays standing
  ck(c.update(-1.f)&&c.update(std::nanf("")));          // nonsense keeps the state
  c.reset();ck(!c.crouched());
  RoomCrouchSettings tall{1.8f};ck(!c.update(-.5f,tall)&&c.update(-.55f,tall));}   // 0.7222 / 0.6944
 // The sneak key, switching (toggle): one press to crouch, one to stand, none for a sneak chosen with the stick.
 {SneakPresser p;
  auto r=p.update(true,false,true,true,1000);ck(r.pressed&&r.hold&&r.entering&&p.bodySneak());
  r=p.update(true,false,true,true,1050);ck(r.hold&&!r.pressed);        // the press lasts 100 ms
  r=p.update(true,false,true,true,1150);ck(!r.hold&&!r.pressed);       // released; the state has not answered yet
  r=p.update(true,true,true,true,1700);ck(!r.hold);                    // sneaking now: nothing more
  r=p.update(false,true,true,true,3000);ck(r.pressed&&r.hold&&!r.entering);   // stood up: press to leave
  r=p.update(false,true,true,true,3200);ck(!r.pressed);                // waits for the state (600 ms)
  r=p.update(false,false,true,true,3700);ck(!r.pressed&&!p.bodySneak());
  r=p.update(false,true,true,true,5000);ck(!r.pressed&&!r.hold);       // the player's own sneak (the stick) stays
  r=p.update(true,true,true,true,5100);ck(!r.pressed);                 // crouched into it: already sneaking
  r=p.update(true,false,true,false,6000);ck(!r.pressed&&!r.hold);      // not allowed (swimming, a menu)
  r=p.update(true,false,true,true,6100);ck(r.pressed);                 // allowed again: crouching wins
 }
 // The sneak key held (the player's settings): down while crouched.
 {SneakPresser p;
  auto r=p.update(true,false,false,true,1000);ck(r.hold&&r.pressed&&r.entering);
  r=p.update(true,true,false,true,1100);ck(r.hold&&!r.pressed);
  r=p.update(false,true,false,true,1200);ck(!r.hold);
  r=p.update(true,false,false,false,1300);ck(!r.hold);
 }
 // The view reset pose: both hands 0.1 m ahead and 0.1 m to their side of the headset, 3 s, nothing pressed.
 {RecenterPose pose;HandFrame f;f.focused=true;const Pose head{{0,0,0,1},{0,1.6f,0}};
  for(unsigned h=0;h<2;++h){auto& s=f.hands[h];s.gripActive=s.triggerActive=s.grabActive=true;s.gripFlags=15;s.grip={{0,0,0,1},{h?.1f:-.1f,1.6f,-.1f}};}
  ck(pose.inPose(f,head));
  ck(!pose.update(f,head,true,1000)&&!pose.update(f,head,true,3999)&&pose.held(3999)==2999);
  ck(pose.update(f,head,true,4000)&&!pose.update(f,head,true,4500));   // once
  f.hands[0].grip.position.x=-.35f;ck(!pose.update(f,head,true,5000)&&pose.held(5000)==0);   // the left hand away: ended
  f.hands[0].grip.position.x=-.25f;ck(pose.inPose(f,head));                                // 0.15 m off: still in the pose
  ck(!pose.update(f,head,true,6000)&&pose.update(f,head,true,9000));                         // counts again
  f.hands[1].trigger=.8f;ck(!pose.inPose(f,head));f.hands[1].trigger=0;
  f.hands[0].grab=true;ck(!pose.inPose(f,head));f.hands[0].grab=false;
  f.hands[1].gripFlags=0;ck(!pose.inPose(f,head));f.hands[1].gripFlags=15;
  ck(!pose.update(f,head,false,10000)&&pose.held(10000)==0);                                 // not allowed
  // The head turned 90 degrees to the left (looking along -X): the points turn with it.
  const Pose turned{evr::euler(1.5707963f,0,0),{0,1.6f,0}};
  ck(!pose.inPose(f,turned));
  f.hands[0].grip.position={-.1f,1.6f,.1f};f.hands[1].grip.position={-.1f,1.6f,-.1f};ck(pose.inPose(f,turned));
 }
 std::cout<<"room_sneak "<<n<<" checks PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"room_sneak FAIL at "<<e.what()<<"\n";return 1;}}
