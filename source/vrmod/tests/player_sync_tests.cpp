#include <evr/player_sync.hpp>
#include "player_skeleton_fixture.hpp"
#include <iostream>
#include <limits>
#include <string>
using namespace evr;using namespace evr::sync;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 auto angle=[](Quat a,Quat b){const auto d=multiply(inverse(normalized(a)),normalized(b));return 2*std::atan2(std::sqrt(d.x*d.x+d.y*d.y+d.z*d.z),std::abs(d.w))*180/pi;};
 auto turnY=[](float degrees){const float h=degrees*pi/360;return Quat{0,std::sin(h),0,std::cos(h)};};
 auto pitch=[](float degrees){const float h=degrees*pi/360;return Quat{std::sin(h),0,0,std::cos(h)};};
 auto same=[&](const Pose& a,const Pose& b,float metres=.0006f,float degrees=.02f){return length(a.position-b.position)<=metres&&angle(a.orientation,b.orientation)<=degrees;};
 const Place somewhere{3676712,843955,-1455377};

 // A body goes through the datagram: half a millimetre, a hundredth of a degree, the flags and fingers as they were.
 Body body;body.head={normalized(multiply(turnY(40),pitch(25))),{.031f,1.652f,-.0415f}};
 body.hand[0]={normalized(Quat{.3f,-.2f,.5f,.79f}),{-.25f,1.21f,.33f}};body.hand[1]={normalized(Quat{-.6f,.1f,.2f,-.76f}),{.27f,1.4f,.45f}};
 body.pelvis={turnY(38),{.02f,.97f,-.06f}};body.fingers[0].amount={.15f,1,.5f,.5f,.25f};body.fingers[1].amount={1,.15f,0,.75f,1};
 body.swim=true;body.bowDrawn=true;body.reverseGrip=true;body.twoHand=2;
 {const auto bytes=encode(body,0xbeef,somewhere);ck(bytes.size()==fullBytes+stampBytes&&fullBytes==80&&bytes[0]==magic&&bytes[1]==version);
  const auto r=decode(bytes);ck(r&&r->sequence==0xbeef&&!r->hello&&r->place==somewhere);const auto& b=r->body;
  ck(same(b.head,body.head)&&same(b.hand[0],body.hand[0])&&same(b.hand[1],body.hand[1])&&same(b.pelvis,body.pelvis));
  ck(b.ik&&b.swim&&b.bowDrawn&&!b.leftHanded&&b.reverseGrip&&b.twoHand==2&&!b.item);
  for(unsigned h=0;h<2;h++)for(unsigned f=0;f<hand::FingerCount;f++)ck(std::abs(b.fingers[h].amount[f]-body.fingers[h].amount[f])<=.002f);
  // With a held item: thirteen bytes more.
  auto held=body;held.item=true;held.itemInHand={pitch(-70),{.01f,.12f,-.03f}};held.leftHanded=true;held.swim=false;
  const auto more=encode(held,7,{-1,0,2147483647});ck(more.size()==fullBytes+itemBytes+stampBytes&&more.size()==95);const auto h=decode(more);
  ck(h&&h->body.item&&same(h->body.itemInHand,held.itemInHand)&&h->body.leftHanded&&!h->body.swim&&h->sequence==7&&h->place==Place{-1,0,2147483647});
  ck(!h->body.offhandItem);
  // The off hand's item too (version 2, 2026-10-15): thirteen bytes more again, by bit 2 of the two-hand byte; alone as well.
  auto both=held;both.offhandItem=true;both.offhandInHand={pitch(20),{-.02f,.05f,.01f}};const auto two=encode(both,3,somewhere);ck(two.size()==fullBytes+2*itemBytes+stampBytes&&two.size()==108&&(two[5]&4));
  const auto t2=decode(two);ck(t2&&t2->body.item&&t2->body.offhandItem&&same(t2->body.itemInHand,held.itemInHand)&&same(t2->body.offhandInHand,both.offhandInHand)&&t2->body.twoHand==2);
  auto only=body;only.offhandItem=true;only.offhandInHand=both.offhandInHand;const auto one=encode(only,4,somewhere);ck(one.size()==fullBytes+itemBytes+stampBytes&&!(one[4]&32)&&(one[5]&4));
  const auto o1=decode(one);ck(o1&&!o1->body.item&&o1->body.offhandItem&&same(o1->body.offhandInHand,both.offhandInHand));
  auto bad5=two;bad5[5]|=8;ck(!decode(bad5));bad5=two;bad5[5]&=~4;ck(!decode(bad5));bad5=one;bad5[5]&=~4;ck(!decode(bad5));   // an unknown bit; the length does not fit
  {const Pose hand{pitch(-30),{.3f,1.2f,-.1f}},item{pitch(45),{.35f,1.3f,-.2f}};const auto rel=inHand(hand,item);
   ck(same(fromHand(hand,rel),item,1e-5f,.01f)&&length(rel.position)<.2f&&same(inHand(identityPose(),item),item,1e-5f,.01f));}
  // The IK paused: the place and the head alone.
  auto paused=body;paused.ik=false;paused.item=true;const auto few=encode(paused,8,somewhere);ck(few.size()==pausedBytes+stampBytes&&few.size()==33);
  const auto p=decode(few);ck(p&&!p->body.ik&&!p->body.item&&!p->body.offhandItem&&same(p->body.head,body.head)&&p->body.bowDrawn&&p->place==somewhere);
  auto pausedOff=paused;pausedOff.offhandItem=true;ck(encode(pausedOff,8,somewhere).size()==pausedBytes+stampBytes);   // paused: no item travels
  // Version 3 (2026-10-20): the sender's clock in milliseconds at the end; version 2 (without it) is still read.
  {const auto stamped=decode(encode(body,5,somewhere,54321));ck(stamped&&stamped->timed&&stamped->sentMs==54321&&same(stamped->body.hand[1],body.hand[1]));
   const auto pausedStamp=decode(encode(paused,6,somewhere,7));ck(pausedStamp&&pausedStamp->timed&&pausedStamp->sentMs==7&&!pausedStamp->body.ik);
   auto v2=bytes;v2[1]=oldVersion;v2.resize(v2.size()-stampBytes);const auto old=decode(v2);ck(old&&!old->timed&&same(old->body.hand[0],body.hand[0]));
   auto v2long=bytes;v2long[1]=oldVersion;ck(!decode(v2long));auto v3short=bytes;v3short.resize(v3short.size()-stampBytes);ck(!decode(v3short));
   auto oldHello=encodeHello(3);oldHello[1]=oldVersion;ck(decode(oldHello)&&decode(oldHello)->hello);}
  // A hello: "a player with this mod is here", nothing else.
  const auto hi=encodeHello(9);ck(hi.size()==helloBytes&&hi.size()==6);const auto g=decode(hi);ck(g&&g->hello&&!g->body.ik&&g->sequence==9);
  auto wrong=hi;wrong.push_back(0);ck(!decode(wrong));wrong=hi;wrong[4]|=1;ck(!decode(wrong));wrong=hi;wrong[5]=1;ck(!decode(wrong));
  // The roster (2026-10-15): a hello with the ids this player knows; the count must fit, ids must be real, at most sixteen.
  const auto ro=encodeRoster(12,{76561198000000001ull,76561198000000002ull});ck(ro.size()==helloBytes+1+16&&ro[5]==8);const auto rr=decode(ro);
  ck(rr&&rr->hello&&rr->sequence==12&&rr->roster.size()==2&&rr->roster[0]==76561198000000001ull&&rr->roster[1]==76561198000000002ull);
  auto badRoster=ro;badRoster.push_back(0);ck(!decode(badRoster));badRoster=ro;badRoster[6]=3;ck(!decode(badRoster));badRoster=ro;badRoster[5]=4;ck(!decode(badRoster));
  badRoster=ro;for(unsigned b=0;b<8;b++)badRoster[7+b]=0;ck(!decode(badRoster));ck(!decode(encodeRoster(1,{})));
  std::vector<uint64_t> many;for(uint64_t i=1;i<=20;i++)many.push_back(76561198000000000ull+i);const auto capped=decode(encodeRoster(2,many));ck(capped&&capped->roster.size()==rosterMost);
  ck(decode(hi)->roster.empty());
  // Every orientation, also where a part other than w is the largest and where it is negative.
  for(const auto q:{Quat{1,0,0,0},Quat{0,-1,0,0},Quat{0,0,1,0},Quat{0,0,0,-1},Quat{.5f,.5f,.5f,.5f},Quat{-.7071f,0,.7071f,0},Quat{.1f,-.9f,.2f,-.3f}}){
   auto one=body;one.head.orientation=normalized(q);const auto back=decode(encode(one,1,somewhere));ck(back&&angle(back->body.head.orientation,q)<.02f);}
  // Far from the character: held at the range's end, not wrapped round.
  auto far=body;far.hand[1].position={40,-40,3};const auto f=decode(encode(far,1,somewhere));ck(f&&std::abs(f->body.hand[1].position.x-16.3835f)<1e-3f&&std::abs(f->body.hand[1].position.y+16.3835f)<1e-3f);
  // Bytes from another machine: anything that is not exactly one datagram of this version gives nothing.
  ck(!decode(nullptr,82)&&!decode(bytes.data(),0)&&!decode(bytes.data(),79)&&!decode(bytes.data(),81));auto bad=bytes;bad.push_back(0);ck(!decode(bad));
  bad=bytes;bad[0]^=1;ck(!decode(bad));bad=bytes;bad[1]=1;ck(!decode(bad));bad=bytes;bad[1]=version+1;ck(!decode(bad));bad=bytes;bad[4]|=0x80;ck(!decode(bad));bad=bytes;bad[4]|=0x40;ck(!decode(bad));bad=bytes;bad[5]=3;ck(!decode(bad));
  bad=bytes;bad[bodyAt+6]=4;ck(!decode(bad));                                        // no such largest part
  bad=bytes;for(unsigned at=7;at<13;at+=2){bad[bodyAt+at]=0xff;bad[bodyAt+at+1]=0x7f;}ck(!decode(bad));   // three parts of 0.707: longer than a unit
  bad=bytes;bad[4]|=32;ck(!decode(bad));bad=more;bad[4]&=~32;ck(!decode(bad));bad=few;bad[4]|=1;ck(!decode(bad));   // the length does not fit the flags
  for(size_t cut=0;cut<more.size();cut++)ck(!decode(more.data(),cut));
  bool threw=false;auto nan=body;nan.hand[0].position.x=NAN;try{encode(nan,1,somewhere);}catch(const std::invalid_argument&){threw=true;}ck(threw);
  nan.ik=false;ck(encode(nan,1,somewhere).size()==pausedBytes+stampBytes);}                       // paused: the hands do not count

 // Model space and the world's axes: there and back, with a model that faces another way and is not of unit size.
 {const Model m{normalized(multiply(turnY(-110),pitch(12))),{.9f,.9f,.9f}};const Pose p{normalized(Quat{.2f,.4f,-.1f,.88f}),{.3f,1.5f,-.2f}};
  ck(same(toModel(m,toWorld(m,p)),p,1e-5f,.01f));const auto w=toWorld(Model{turnY(90),{1,1,1}},Pose{{0,0,0,1},{0,1,1}});
  ck(length(w.position-Vec3{1,1,0})<1e-5f&&angle(w.orientation,turnY(90))<.01f);                 // +Z toward +X, as the model faces
  // The nearest twelve get the body (2.5): here the nearest two of four within sight, the far and the unknown left out.
  {std::vector<std::array<double,3>> at{{1,0,0},{300,0,0},{2,0,0},{0.5,0,0},{3,0,0}};std::vector<char> known{1,1,1,0,1};
   const auto r=bodyRecipients({0,0,0},at,known,250,2);ck(r.size()==2&&r[0]==0&&r[1]==2);
   const auto all=bodyRecipients({0,0,0},at,known,250);ck(all.size()==3&&all[2]==4);ck(bodyRecipients({0,0,0},{},{},250).empty());}
  ck(place({3676.7124,843.9551,-1455.3766})==Place{3676712,843955,-1455377}&&std::abs(metres(somewhere)[2]+1455.377)<1e-9&&place({1e12,0,-1e12})==Place{2100000000,0,-2100000000});}

 // The whole way: the owner's solve -> body -> datagram -> receiver -> the same solve on the receiver's copy of the
 // character, which faces another way and whose animation is at another place: head, chest, arms and pelvis stand
 // where the owner's do, in the world.
 {std::vector<hand::Joint> rest;std::vector<uint16_t> parents;fixture::playerSkeleton(rest,parents);const auto R=bodyik::globals(rest,parents);
  const bodyik::Settings s;const Vec3 eye=R[17].position+s.eyeInHead;
  auto walk=rest;walk[1].orientation=normalized(multiply(normalized(Quat{.1f,.05f,.12f,1}),rest[1].orientation));walk[1].position.y-=.03f;
  bodyik::Targets t{{turnY(30),{eye.x+.04f,eye.y-.15f,eye.z}},{R[64],R[94]}};t.socket[1].position={.22f,1.25f,.32f};
  for(bool water:{false,true}){t.swim=water;
   bodyik::Report report;const auto own=bodyik::globals(bodyik::solve(walk,rest,parents,t,{},s,&report),parents);
   const Model owner{turnY(75),{1,1,1}},seenAs{turnY(60),{1,1,1}};            // the receiver's copy lags 15 degrees behind
   std::array<hand::FingerCurl,2> curls{};curls[1].amount={1,1,.5f,.5f,.5f};
   const auto sent=fromSolve(owner,t,report.hips,curls);ck(sent.swim==water&&sent.ik&&same(sent.pelvis,toWorld(owner,own[1]),1e-5f,.01f));
   Receiver receiver;ck(!receiver.update(0)&&!receiver.known());
   const auto datagram=decode(encode(sent,1,somewhere));ck(datagram&&receiver.offer(*datagram,10));const auto shown=receiver.update(10);ck(shown&&receiver.known());
   const auto theirs=targets(seenAs,*shown);ck(theirs.hipsGiven&&theirs.swim==water);
   const auto seen=bodyik::globals(bodyik::solve(rest,rest,parents,theirs,{},s),parents);
   for(auto j:{1,15,16,17,39,69,40,70,42,72,44,74,64,94}){const auto a=toWorld(owner,own[size_t(j)]),b=toWorld(seenAs,seen[size_t(j)]);ck(length(a.position-b.position)<.003f);}
   for(auto j:{1,17,64,94})ck(angle(toWorld(owner,own[size_t(j)]).orientation,toWorld(seenAs,seen[size_t(j)]).orientation)<.2f);
   ck(shown->fingers[1].amount[0]==1&&std::abs(shown->fingers[1].amount[2]-.5f)<.003f);}}

 // VHVR's way (buffered=false): the receiver steps toward the newest data: a fifth of the way per 20 ms, whatever the
 // frame rate; orientations too.
 {ReceiverSettings s;s.buffered=false;Body at;at.hand[1].position={0,1,0};Receiver r;ck(r.offer({at,1},0,s));ck(r.update(0,s)->hand[1].position.x==0);
  Body next=at;next.hand[1].position.x=.1f;next.head.orientation=turnY(40);next.pelvis.position.z=.05f;ck(r.offer({next,2},0,s));
  auto o=*r.update(.02,s);ck(std::abs(o.hand[1].position.x-.02f)<1e-5f&&std::abs(angle(o.head.orientation,{0,0,0,1})-8)<.01f&&std::abs(o.pelvis.position.z-.01f)<1e-5f);
  o=*r.update(.04,s);ck(std::abs(o.hand[1].position.x-.036f)<1e-5f);
  Receiver fine;fine.offer({at,1},0,s);fine.update(0,s);fine.offer({next,2},0,s);Body f;for(int i=1;i<=10;i++)f=*fine.update(i*.004,s);     // 250 frames a second
  ck(std::abs(f.hand[1].position.x-.036f)<1e-4f);
  Receiver coarse;coarse.offer({at,1},0,s);coarse.update(0,s);coarse.offer({next,2},0,s);ck(std::abs(coarse.update(.04,s)->hand[1].position.x-.036f)<1e-5f);
  // Half of the way after about three ticks, a tenth left after about ten (VHVR 4.2).
  Receiver half;half.offer({at,1},0,s);half.update(0,s);half.offer({next,2},0,s);ck(std::abs(half.update(.0621,s)->hand[1].position.x-.05f)<5e-4f);ck(half.update(.2063,s)->hand[1].position.x>.0899f);
  // Less than a millimetre, less than a thousandth of a degree: there at once.
  Body close=o;close.hand[1].position.x+=.0009f;Receiver near;near.offer({o,1},0,s);near.update(0,s);near.offer({close,2},0,s);ck(near.update(.001,s)->hand[1].position.x==close.hand[1].position.x);
  // The same time again: no step.
  ck(std::abs(r.update(.04,s)->hand[1].position.x-.036f)<1e-5f);
  // Fingers, flags and the held item are taken as they come.
  Body flags=next;flags.fingers[0].amount[1]=1;flags.bowDrawn=true;flags.item=true;flags.itemInHand.position.y=.1f;r.offer({flags,3},.04,s);o=*r.update(.05,s);
  ck(o.fingers[0].amount[1]==1&&o.bowDrawn&&o.item&&o.itemInHand.position.y==.1f&&o.hand[1].position.x<.1f);
  // A hello is not a body.
  Received hello;hello.hello=true;hello.sequence=9;ck(!r.offer(hello,.06)&&r.sequence()==3);}

 // Datagrams out of order: the older one is dropped; the count wraps round; after the time limit anything is new.
 {Receiver r;Body a,b;a.head.position.x=1;b.head.position.x=2;const ReceiverSettings s;
  ck(r.offer({a,100},0)&&!r.offer({b,99},.01)&&!r.offer({b,100},.01)&&r.sequence()==100&&r.update(.01)->head.position.x==1);
  ck(r.offer({b,101},.02)&&r.offer({a,65535},.03)==false);
  Receiver wrap;ck(wrap.offer({a,65535},0)&&wrap.offer({b,0},.01)&&wrap.offer({a,3},.02)&&!wrap.offer({b,65535},.03)&&wrap.sequence()==3);
  Receiver gone;ck(gone.offer({a,500},0)&&gone.update(1));ck(gone.update(2.9)&&!gone.update(3.1)&&!gone.known()&&!gone.update(3.2));
  ck(gone.offer({b,2},9)&&gone.update(9)->head.position.x==2);                                  // the owner started again from a low count: taken, and as it is
  Receiver late;late.offer({a,500},0);late.update(0);ck(late.offer({b,7},3.5));}                // nothing for longer than the limit, not yet noticed by update()

 // The IK paused: nothing of the body counts; back again: taken as it is, no sweep from where the hands were.
 {ReceiverSettings s;s.buffered=false;Receiver r;Body a;a.hand[0].position={-.3f,1,0};r.offer({a,1},0,s);r.update(0,s);Body paused=a;paused.ik=false;paused.head.position.x=.5f;r.offer({paused,2},.02,s);
  auto o=*r.update(.04,s);ck(!o.ik&&o.head.position.x==.5f);Body back=a;back.hand[0].position.x=.4f;back.head.position.x=.6f;r.offer({back,3},.06,s);
  o=*r.update(.08,s);ck(o.ik&&o.hand[0].position.x==.4f&&o.head.position.x==.6f);
  Body move=back;move.hand[0].position.x=.5f;r.offer({move,4},.08,s);o=*r.update(.1,s);ck(std::abs(o.hand[0].position.x-.42f)<1e-5f);}
 // Buffered (the default since 2026-10-20): shown a little late, between two received poses - the full reach of a swing.
 // A hand swinging 0.5 m back and forth 2.5 times a second, sent 30 times a second (each with the sender's clock), arriving
 // 40 +-8 ms later, shown 90 times a second: the buffered way keeps over 94 % of the reach, VHVR's stepping about 58 %.
 {auto swing=[](bool buffered,bool timed,double& lagOut){ReceiverSettings s;s.buffered=buffered;Receiver r;double reach=0;
    std::vector<std::pair<double,float>> shown;uint32_t noise=12345;size_t next=0;std::vector<std::pair<double,Received>> flight;
    for(int n=0;n<120;n++){const double t=n/30.;Body b;b.hand[1].position.x=float(.5*std::sin(2*pi*2.5*t));
      noise=noise*1103515245u+12345u;const double jitter=(double((noise>>16)&1023)/1023.-.5)*.016;
      Received d{b,uint16_t(n+1)};d.timed=timed;d.sentMs=uint16_t(uint32_t(std::lround(t*1000+63000))&0xffff);   // wraps round at 2.5 s
      flight.push_back({t+.04+jitter,d});}
    for(int k=0;k<360;k++){const double now=k/90.;
      while(next<flight.size()&&flight[next].first<=now){r.offer(flight[next].second,flight[next].first,s);++next;}
      const auto o=r.update(now,s);if(o&&now>1){reach=std::max(reach,double(std::abs(o->hand[1].position.x)));shown.push_back({now,o->hand[1].position.x});}}
    double best=1e9;lagOut=0;for(int ms=0;ms<=200;ms+=5){double e=0;for(const auto& [t,x]:shown)e+=std::abs(x-.5*std::sin(2*pi*2.5*(t-ms/1000.)));if(e<best){best=e;lagOut=ms/1000.;}}
    return reach;};
  double lag=0,lagStep=0,lagOld=0;const double buffered=swing(true,true,lag),stepped=swing(false,true,lagStep),untimed=swing(true,false,lagOld);
  ck(buffered>.47&&lag<=.12);ck(stepped<.33);ck(untimed>.45&&lagOld<=.14);}
 // Buffered: a pause shows at once, and the pose after it is taken as it is (no sweep across the pause).
 {Receiver r;Body a;a.hand[0].position={-.3f,1,0};r.offer({a,1},0);r.update(0);Body paused=a;paused.ik=false;paused.head.position.x=.5f;r.offer({paused,2},.03);
  ck(!r.update(.031)->ik);Body back=a;back.hand[0].position.x=.4f;r.offer({back,3},.06);
  for(double t=.07;t<.3;t+=.01){const auto o=*r.update(t);ck(!o.ik||o.hand[0].position.x==.4f||o.hand[0].position.x==-.3f);}
  ck(r.update(.3)->hand[0].position.x==.4f);}

 // Whose body: the character that stands where the sender says, none while two are about as near, and the one taken
 // before for as long as the sender stays about it.
 {const std::vector<std::array<double,3>> characters{{100,20,50},{104,20,50},{100.5,20,50.4}};
  ck(nearestCharacter({104.3,20,50.2},characters)==1&&nearestCharacter({100.2,20,50.1},characters)==-1);       // two within a metre of each other's distance
  ck(nearestCharacter({108.5,20,50},characters)==-1&&nearestCharacter({108.5,20,50},characters,6)==1);          // too far; near enough with a wider limit
  ck(nearestCharacter({0,0,0},{})==-1&&nearestCharacter({1,0,0},{{0,0,0}})==0&&nearestCharacter({1,0,0},{{0,0,0}},.5)==-1);
  ck(boundCharacter(2,{100.2,20,50.1},characters)==2&&boundCharacter(0,{100.2,20,50.1},characters)==0&&boundCharacter(-1,{100.2,20,50.1},characters)==-1);
  // Bound and then beyond 6 m of that character (2.7): nothing this time, even with another character near the place;
  // the next datagram, without the binding, finds that one.
  ck(boundCharacter(0,{104.3,20,50.2},characters)==0&&boundCharacter(0,{110,20,50},characters)==-1&&boundCharacter(0,{107,20,50},characters)==-1&&boundCharacter(-1,{107,20,50},characters)==1&&boundCharacter(7,{104.3,20,50.2},characters)==1);}

 // Who is sent what: a hello a second to the listed; the body, at the frame rate, to those that were heard from.
 {Peers peers;const LinkSettings s;peers.list({11,22},0);ck(peers.all().size()==2&&peers.find(11)&&peers.find(22)&&!peers.find(33));
  auto& a=*peers.find(11);using D=Peers::Due;
  ck(peers.due(a,0,true)==D::Hello&&peers.due(a,.5,true)==D::Nothing&&peers.due(a,1.01,true)==D::Hello);          // not heard from: hello only
  Received hello;hello.hello=true;ck(peers.heard(11,hello,1.1)&&!peers.heard(33,hello,1.1));                       // not listed: not counted
  ck(peers.due(a,1.2,true)==D::Body&&peers.due(a,1.205,true)==D::Nothing&&peers.due(a,1.22,true)==D::Body);         // heard: the body, 75 a second at the most
  ck(peers.due(a,1.3,false)==D::Nothing&&peers.due(a,2.25,false)==D::Hello);                                        // no body to send: hello at its pace
  ck(peers.due(a,6.2,true)==D::Hello);                                                                              // not heard for five seconds: hello again
  // A body from a listed player reaches its receiver with its place; an older one is dropped.
  Body b;b.head.position.x=1;Received r{b,5,false,{1000,2000,3000}};ck(peers.heard(22,r,2)&&peers.find(22)->placed&&peers.find(22)->place==Place{1000,2000,3000}&&peers.find(22)->receiver.update(2)->head.position.x==1);
  Received old{b,4,false,{9,9,9}};ck(!peers.heard(22,old,2.1)&&peers.find(22)->place==Place{1000,2000,3000}&&peers.find(22)->dropped==1&&peers.find(22)->taken==1);
  // Off the list: nothing is sent and nothing counts; forgotten a minute later; back on the list: as new.
  peers.list({11},10);auto& gone=*peers.find(22);ck(!gone.listed&&peers.due(gone,11,true)==D::Nothing&&!peers.heard(22,r,11));
  peers.list({11},69);ck(peers.find(22));peers.list({11},71);ck(!peers.find(22)&&peers.all().size()==1);
  peers.list({11,22},72);ck(peers.find(22)&&!peers.find(22)->placed);
  // No more than sixteen; an id of zero is nobody.
  std::vector<uint64_t> many;for(uint64_t i=0;i<40;i++)many.push_back(i);Peers crowd;crowd.list(many,0);ck(crowd.all().size()==16&&!crowd.find(0));
  // A flood (2.7): the 201st datagram of a player within a second is ignored, and so is the rest of that second; the next second counts again.
  Peers flood;flood.list({5},100);Received f{b,1,false,{0,0,0}};unsigned taken=0;
  for(unsigned i=0;i<250;i++){f.sequence=uint16_t(i+1);if(flood.heard(5,f,100.+i*.001))++taken;}
  ck(taken==200&&flood.find(5)->ignored==50&&flood.find(5)->taken==200);
  f.sequence=300;ck(flood.heard(5,f,101.0)&&flood.find(5)->ignored==50);
  ReceiverSettings few;few.mostPerSecond=2;Peers tight;tight.list({6},0);Received g{b,1,false,{0,0,0}};
  ck(tight.heard(6,g,0,few));g.sequence=2;ck(tight.heard(6,g,.1,few));g.sequence=3;ck(!tight.heard(6,g,.2,few)&&tight.find(6)->heard==.1);g.sequence=4;ck(tight.heard(6,g,1.0,few));}
 // Actions (2026-10-20): a cast's strength, 7 bytes; not a body datagram, and a body datagram is no action.
 {const auto c=encodeCast(513,.75f);ck(c.size()==castActionBytes&&c[0]==actionMagic);const auto a=decodeAction(c.data(),c.size());
  ck(a&&a->kind==castAction&&a->id==513&&std::abs(a->strength-.75f)<1e-4f);ck(!decode(c.data(),c.size()));
  ck(!decodeAction(c.data(),c.size()-1));auto bad=c;bad[1]=2;ck(!decodeAction(bad.data(),bad.size()));bad=c;bad[2]=9;ck(!decodeAction(bad.data(),bad.size()));
  const auto low=decodeAction(encodeCast(1,-3.f).data(),castActionBytes);ck(low&&low->strength==0);
  const auto body=encodeHello(4);ck(!decodeAction(body.data(),body.size()));}
 // version 4: a leaning owner's lean follows the items (7 bytes); without it the sizes stay; a lean on a paused body refused
 {Body b;b.head={normalized(Quat{0,.3f,0,1}),{0,1.6f,0}};b.pelvis={identityPose().orientation,{0,1,0}};b.swim=true;b.leaning=true;b.lean=normalized(Quat{.6f,0,0,.8f});
  const auto e=encode(b,77,Place{1,2,3},1234);ck(e.size()==fullBytes+leanBytes+stampBytes&&e[1]==version&&(e[4]&0x80));
  const auto d=decode(e.data(),e.size());ck(d&&d->body.leaning&&d->body.swim&&std::abs(std::abs(d->body.lean.x*b.lean.x+d->body.lean.w*b.lean.w)-1)<1e-4f);
  b.leaning=false;const auto f=encode(b,78,Place{},0);ck(f.size()==fullBytes+stampBytes&&!(f[4]&0x80));const auto g=decode(f.data(),f.size());ck(g&&!g->body.leaning);
  auto bad=e;bad[1]=3;ck(!decode(bad.data(),bad.size()));   // version 3 never had the lean flag
  auto three=f;three[1]=3;ck(decode(three.data(),three.size()).has_value());}   // version 3 still read
 // a shot: the camera as it was sent, an orientation that is not a unit one refused
 {std::array<uint8_t,40> camera{};const int64_t at[3]{int64_t(12)<<32,-(int64_t(3)<<31),int64_t(700)<<32};std::memcpy(camera.data(),at,24);
  const Quat q=normalized(Quat{.1f,.7f,0,.7f});std::memcpy(camera.data()+24,&q,16);
  const auto s=encodeShot(65535,camera);ck(s.size()==shotActionBytes&&s[2]==shotAction);const auto a=decodeAction(s.data(),s.size());
  ck(a&&a->kind==shotAction&&a->id==65535&&a->aim==camera);
  {const std::array<int64_t,3> at{int64_t(5)<<32,-(int64_t(77)<<32),int64_t(1)<<40};const auto e=encodeHit(9,at,true);const auto d=decodeAction(e.data(),e.size());
   ck(e.size()==hitActionBytes&&d&&d->kind==hitAction&&d->id==9&&d->at==at&&d->character);auto bad=e;bad[29]=4;ck(!decodeAction(bad.data(),bad.size()));ck(!d->shot);
   const auto sh=encodeHit(11,at,true,true);const auto sd=decodeAction(sh.data(),sh.size());ck(sd&&sd->shot&&sd->character);
   const auto o=encodeHit(10,at,false);const auto od=decodeAction(o.data(),o.size());ck(od&&!od->character);
   const auto en=encodeHit(12,at,true,false,0x12345678u);const auto ed=decodeAction(en.data(),en.size());ck(en.size()==hitEntityActionBytes&&ed&&ed->entity==0x12345678u&&ed->at==at&&ed->character&&!ed->shot);
   ck(d->entity==0);auto cut=en;cut.pop_back();ck(!decodeAction(cut.data(),cut.size()));}
  const std::array<int64_t,3> enemy{int64_t(3)<<32,int64_t(4)<<32,-(int64_t(5)<<32)};
  const auto p=encodeParry(300,enemy);const auto pa=decodeAction(p.data(),p.size());ck(p.size()==parryActionBytes&&pa&&pa->kind==parryAction&&pa->id==300&&pa->at==enemy);
  auto longer=p;longer.push_back(0);ck(!decodeAction(longer.data(),longer.size()));
  const auto h=encodeShot(7,camera,true);const auto ha=decodeAction(h.data(),h.size());ck(ha&&ha->kind==heldAimAction&&ha->id==7&&ha->aim==camera);ck(!decodeAction(s.data(),s.size()-1));ck(!decode(s.data(),s.size()));
  auto bad=camera;const Quat zero{0,0,0,0};std::memcpy(bad.data()+24,&zero,16);const auto b=encodeShot(1,bad);ck(!decodeAction(b.data(),b.size()));
  const float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(bad.data()+24,&nan,4);const auto c=encodeShot(1,bad);ck(!decodeAction(c.data(),c.size()));}
 // The clocks (2026-10-22): the sender's clock, the echo and the hold come back as they were; a wrong size or a clock of 0 is refused.
 {const auto c=encodeClock(9,123456789012345LL,98765432109876LL,4321u);ck(c.size()==clockActionBytes&&c[2]==clockAction);const auto a=decodeAction(c.data(),c.size());
  ck(a&&a->kind==clockAction&&a->id==9&&a->clockSent==123456789012345LL&&a->clockEcho==98765432109876LL&&a->clockHold==4321u);
  ck(!decodeAction(c.data(),c.size()-1));const auto z=encodeClock(1,0,0,0);ck(!decodeAction(z.data(),z.size()));const auto first=encodeClock(1,5,0,0);ck(decodeAction(first.data(),first.size()).has_value());}
 // A guard decided on the guest (2026-10-22): the place and the flag come back; another size or flag is refused.
 {const std::array<int64_t,3> at{11,-22,33};const auto g=encodeGuard(4,at,1);ck(g.size()==guardActionBytes&&g[2]==guardAction);const auto a=decodeAction(g.data(),g.size());
  ck(a&&a->kind==guardAction&&a->id==4&&a->at==at&&a->character);ck(!decodeAction(g.data(),g.size()-1));auto bad=g;bad[29]=2;ck(!decodeAction(bad.data(),bad.size()));}
 std::cout<<"player_sync "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"player_sync FAIL "<<e.what()<<"\n";return 1;}}
