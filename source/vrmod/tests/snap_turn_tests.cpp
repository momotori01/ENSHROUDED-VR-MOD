#include <evr/snap_turn.hpp>
#include <evr/vr_settings.hpp>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 // One step a push: pushed to the side over 0.7 it turns once; held, nothing more; back under 0.35 and out again, the next.
 {SnapTurn s;ck(s.update(0,0,true)==0&&s.armed());
  ck(s.update(.5f,0,true)==0);ck(s.update(.8f,0,true)==1);ck(s.update(1,0,true)==0&&s.update(.9f,0,true)==0);
  ck(s.update(.5f,0,true)==0);ck(s.update(1,0,true)==0);   // not back far enough: still none
  ck(s.update(.3f,0,true)==0&&s.update(-.9f,0,true)==-1&&s.update(-1,0,true)==0);
  ck(s.update(0,0,true)==0&&s.update(-.75f,.2f,true)==-1);}
 // More up or down than to the side (sprint, dodge): no turn, and none after it either while the stick stays out.
 {SnapTurn s;s.update(0,0,true);ck(s.update(.75f,.9f,true)==0&&s.update(.8f,.5f,true)==1);
  SnapTurn t;t.update(0,0,true);ck(t.update(-.8f,-.85f,true)==0);}
 // A stick already pushed when the turn starts to apply (a menu closed, the snap turn switched on) waits for its return.
 {SnapTurn s;ck(s.update(1,0,true)==0);ck(s.update(.2f,0,true)==0&&s.update(1,0,true)==1);
  SnapTurn t;t.update(0,0,true);ck(t.update(1,0,false)==0&&t.update(1,0,true)==0&&t.update(0,0,false)==0&&t.update(1,0,true)==1);
  SnapTurn u;ck(u.update(NAN,0,true)==0&&u.update(1,0,true)==0);}
 // The angle: 10 .. 90 in steps of 5, 30 by default; the game's input is the opposite sign, in radians.
 {ck(clampSnapAngle(30)==30&&clampSnapAngle(32)==30&&clampSnapAngle(33)==35&&clampSnapAngle(0)==10&&clampSnapAngle(200)==90&&clampSnapAngle(-5)==10);
  ck(std::abs(snapTurnInput(1,30)+.5235988f)<1e-5f&&std::abs(snapTurnInput(-1,90)-1.5707963f)<1e-5f&&snapTurnInput(0,45)==0.f);
  ck(std::abs(snapTurnInput(1,7)+.1745329f)<1e-5f);}   // held to 10
 // The settings file: off and 30 unless set ([Body] SnapTurn, SnapTurnAngle); out of range gives 30.
 {using namespace evr::settings;const std::string ini="[Mode]\r\nVR=1\r\n";
  ck(!read(ini).snapTurn&&read(ini).snapTurnAngle==30);
  ck(read(set(ini,"Body","SnapTurn","1")).snapTurn&&read(set(ini,"Body","SnapTurnAngle","45")).snapTurnAngle==45&&read(set(ini,"Body","SnapTurnAngle","47")).snapTurnAngle==45);
  ck(read(set(ini,"Body","SnapTurnAngle","95")).snapTurnAngle==30&&read(set(ini,"Body","SnapTurnAngle","x")).snapTurnAngle==30&&read(set(ini,"Body","SnapTurnAngle","10")).snapTurnAngle==10);}
 std::cout<<"snap_turn "<<n<<" PASS\n";return 0;}
 catch(const std::exception& e){std::cerr<<"snap_turn FAIL "<<e.what()<<"\n";return 1;}}
