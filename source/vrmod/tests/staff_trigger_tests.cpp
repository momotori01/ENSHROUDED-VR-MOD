#include <evr/staff_trigger.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 // held: LT (charge) only, never RT; let go: LT and RT for the tap, then nothing
 {StaffTrigger s;auto o=s.update(.9f,true,true,1000);ck(o.leftTrigger&&!o.rightTrigger&&!o.cast);
  o=s.update(1.f,true,true,3000);ck(o.leftTrigger&&!o.rightTrigger);   // held long: still no cast by itself
  o=s.update(.1f,true,true,3010);ck(o.cast&&o.leftTrigger&&o.rightTrigger);
  o=s.update(0,true,true,3100);ck(o.leftTrigger&&o.rightTrigger&&!o.cast);
  o=s.update(0,true,true,3120);ck(o.leftTrigger&&!o.rightTrigger);   // LT a while after the tap (the game's shoot tap on RT's release)
  o=s.update(0,true,true,3500);ck(o.leftTrigger&&!o.rightTrigger);
  o=s.update(0,true,true,3520);ck(!o.leftTrigger&&!o.rightTrigger);}
 // the hysteresis: half a pull keeps a held trigger, does not start one
 {StaffTrigger s;ck(!s.update(.5f,true,true,0).leftTrigger);ck(s.update(.85f,true,true,10).leftTrigger);ck(s.update(.5f,true,true,20).leftTrigger&&!s.update(.5f,true,true,30).cast);}
 // a pull begun while not ready (the shoulder's bow, a menu) is not the staff's: no charge, no cast when it is let go
 {StaffTrigger s;ck(!s.update(.9f,true,false,0).leftTrigger);auto o=s.update(.9f,true,true,10);ck(!o.leftTrigger&&!o.rightTrigger);
  o=s.update(0,true,true,20);ck(!o.cast&&!o.rightTrigger);
  o=s.update(.9f,true,true,30);ck(o.leftTrigger);}   // the next pull is
 // no longer ready while charging (the bow taken from the shoulder): cancelled, the release casts nothing
 {StaffTrigger s;s.update(.9f,true,true,0);auto o=s.update(.9f,true,false,10);ck(o.cancelled&&!o.leftTrigger&&!o.rightTrigger);
  o=s.update(.9f,true,true,20);ck(!o.leftTrigger);o=s.update(0,true,true,30);ck(!o.cast&&!o.rightTrigger);}
 // no longer ready during the cast: its RT stops at once
 {StaffTrigger s;s.update(.9f,true,true,0);ck(s.update(0,true,true,10).cast);auto o=s.update(0,true,false,20);ck(!o.rightTrigger&&!o.leftTrigger);
  o=s.update(0,true,true,30);ck(!o.rightTrigger);}
 // the trigger not read (the controller lost): cancelled, nothing cast
 {StaffTrigger s;s.update(.9f,true,true,0);auto o=s.update(.9f,false,true,10);ck(o.cancelled&&!o.cast&&!o.rightTrigger&&!o.leftTrigger);}
 std::cout<<"RESULT "<<n<<" PASS 0 FAIL\n";return 0;}
 catch(const std::exception& e){std::cout<<"FAIL "<<e.what()<<"\n";return 1;}}
