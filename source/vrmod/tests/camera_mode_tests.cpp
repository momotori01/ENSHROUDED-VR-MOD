#include <evr/camera_mode.hpp>
#include <iostream>
#include <limits>
using namespace evr;
int main(){
 unsigned pass=0,fail=0;auto check=[&](bool ok,const char* what){std::cout<<(ok?"PASS ":"FAIL ")<<what<<'\n';ok?++pass:++fail;};
 game::StableHeading yaw;const float seed=.7f;const double initial=1234.25;
 check(std::abs(yaw.value(1,euler(seed,0,0),initial)-seed)<1e-6,"initial camera heading calibrated once");
 bool isolated=true;
 for(unsigned frame=0;frame<720;++frame){
  const float t=frame*pi/180;
  const auto q=euler(t,3*t,-2*t); // roll, flips and rotating attacks
  isolated&=std::abs(yaw.value(1,q,initial)-seed)<1e-6;
 }
 check(isolated,"720 animation orientations cannot rotate the calibrated VR anchor");
 check(std::abs(yaw.value(1,euler(-2,1,1),initial+.3)-(seed-.3f))<1e-6,"weighted user yaw changes facing (opposite sign to the orbit accumulator, real-device 2026-10-09) independently of animation");
 check(std::abs(yaw.value(1,euler(2,-1,1),initial+.3)-(seed-.3f))<1e-6,"repeated render/input/eye reads do not integrate a turn twice");
 bool wrap=true;
 for(unsigned n=0;n<1000;++n){const double turn=initial+n*.17;const float a=yaw.value(1,{0,0,0,1},turn);wrap&=std::isfinite(a)&&std::abs(a)<=pi;}
 check(wrap,"long continuous turns remain bounded and finite");
 using P=game::CameraPresentation;
 check(game::cameraPresentation(25)==P::FirstPerson&&game::cameraPresentation(43)==P::FirstPerson&&game::cameraPresentation(44)==P::FirstPerson,"glider jump and falling remain first person");
 check(game::cameraPresentation(0)==P::NativeEvent&&game::cameraPresentation(30)==P::NativeEvent&&game::cameraPresentation(31)==P::NativeEvent,"entity camera dialogue and altar preserve native event camera");
 check(game::cameraPresentation(34)==P::FirstPerson&&game::cameraPresentation(35)==P::FirstPerson,"placing a flame altar or an area prop is first-person play (2026-10-19: the headset jumped to the game's camera)");
 check(game::cameraPresentation(39)==P::FirstPerson&&game::cameraPresentation(40)==P::FirstPerson&&game::cameraPresentation(41)==P::FirstPerson&&
       game::cameraPresentation(42)==P::FirstPerson,"fishing is first-person play, its victory camera too (2026-10-19)");
 check(game::cameraPresentation(38)==P::FirstPerson&&game::cameraPresentation(36)==P::FirstPerson&&game::cameraPresentation(37)==P::FirstPerson,"the cameras of a building tool in hand, third and first person, are ordinary first-person play");
 // 1.0 renumbering (its reflection table, 2026-10-16): read back to June's values.
 check(game::legacyCameraId(4)==4&&game::legacyCameraId(7)==6&&game::legacyCameraId(16)==19&&game::legacyCameraId(17)==20,"1.0 default, sprinting, idle and sneaking keep their June ids");
 check(game::legacyCameraId(22)==25&&game::legacyCameraId(27)==30&&game::legacyCameraId(36)==38&&game::legacyCameraId(42)==42,"1.0 glider, dialogue, building third person and fishing victory read back to June's");
 check(game::legacyCameraId(1)==1&&game::legacyCameraId(3)==3&&game::legacyCameraId(32)==34&&game::legacyCameraId(33)==35,"1.0 loading cameras stay native events, the altar's building cameras native special");
 check(game::cameraPresentation(game::legacyCameraId(37))==P::FirstPerson&&game::cameraPresentation(game::legacyCameraId(43))==P::FirstPerson&&
       game::cameraPresentation(game::legacyCameraId(29))==P::NativeEvent,"new 1.0 cameras: fly spark and wand aim are play, the mother flame an event");
 check(game::legacyCameraId(255)==255&&game::cameraPresentation(game::legacyCameraId(46))==P::Unknown,"no camera stays 255, a value beyond 1.0's table is unknown");
 bool unknown=true;for(unsigned id=45;id<256;++id)unknown&=game::cameraPresentation(uint8_t(id))==P::Unknown;
 check(unknown,"all unknown camera IDs fall back to native camera");
 check(game::cameraModeFixture(99,19)==19&&game::cameraModeFixture(100,19)==30&&game::cameraModeFixture(399,19)==30&&
       game::cameraModeFixture(400,19)==34&&game::cameraModeFixture(449,19)==34&&game::cameraModeFixture(450,19)==255&&
       game::cameraModeFixture(499,19)==255&&game::cameraModeFixture(500,19)==19,"fixture boundaries cover event special unknown and first-person restoration");
 const float before=yaw.value(1,{0,0,0,1},initial+.3);
 auto native=game::nativeCameraAnchor({123,456,789},euler(-2,.4f,.6f));
 check(native.root.x==123&&native.root.y==456&&native.root.z==789&&length(native.cameraOffsetGameUnits)==0,"native event anchor uses original render camera without first-person height");
 check(length(rotate(native.orientation,{0,0,1})-rotate(euler(-2,.4f,.6f),{0,0,1}))<1e-6,"native event orientation preserved");
 check(yaw.value(1,euler(-2,.4f,.6f),initial+.3)==before,"return from event keeps preceding player heading");
 try{yaw.value(2,{0,0,0,1},initial);check(false,"changed entity needs explicit reset");}catch(const std::invalid_argument&){check(true,"changed entity needs explicit reset");}
 try{yaw.value(1,{0,0,0,1},std::numeric_limits<double>::quiet_NaN());check(false,"invalid control input rejected");}catch(const std::invalid_argument&){check(true,"invalid control input rejected");}
 yaw.reset();check(std::abs(yaw.value(2,euler(-.5f,0,0),5)+.5f)<1e-6,"new player explicit reset recalibrates");
 std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
 for(unsigned i=0;i<2;++i){views[i].pose={euler(.2f,.1f,.3f),{i?.032f:-.032f,1.2f,-.1f}};views[i].fov={-.7f,.8f,.6f,-.8f};}
 game::StableAnchor a;a.root={10000,20000,30000};a.orientation=euler(before,0,0);a.cameraOffsetGameUnits={0,1.65f,0};
 const auto root=a.root;const auto old=game::stereoCameras(a,identityPose(),views,3,100,1,a.root,.01f,1000);
 for(auto& v:views)v.pose.position.x+=.01f;
 const auto moved=game::stereoCameras(a,identityPose(),views,3,110,2,a.root,.01f,1000);
 check(a.root.x==root.x&&a.root.y==root.y&&a.root.z==root.z,"HMD translation never writes player/root position");
 check(std::abs(relativeQ32(moved.eyes[0].position.x,old.eyes[0].position.x))>.001,"small HMD translation still reaches eye position");
 // Body heading: only cameras of ordinary ground play, all of them first person; the native camera look mode is 3.
 {bool exact=true,firstPerson=true;for(unsigned id=0;id<64;++id){const bool expected=id==4||id==5||id==6||id==19||id==20||id==38;
   exact=exact&&game::bodyHeadingCamera(uint8_t(id))==expected;if(expected)firstPerson=firstPerson&&game::cameraPresentation(uint8_t(id))==game::CameraPresentation::FirstPerson;}
  check(exact&&firstPerson&&game::lookModeCamera==3&&game::lookModeByState==1,"body heading cameras and native look mode");}
 // Heading realignment: the native camera is brought back onto the VR heading after the game turned it by itself.
 {game::StableHeading h;double reference=0;
  check(!h.reference(1,reference),"no reference before the heading is calibrated");
  const float origin=h.value(7,euler(.4f,0,0),12.5);
  check(h.reference(7,reference)&&std::abs(reference-(double(origin)+12.5))<1e-9&&!h.reference(8,reference),"the reference is the calibration heading plus the turn then, for that player only");
  h.reference(7,reference);
  // The player turns by d: the native heading goes down by d, the turn up by d; no difference.
  bool none=true;for(double d:{-2.5,-.3,0.,.7,3.,40.})none=none&&std::abs(game::headingError(float(std::remainder(double(origin)-d,2*double(pi))),12.5+d,reference))<2e-6f;
  check(none,"the player's own turns leave no difference between the native and the VR heading");
  check(std::abs(game::headingError(origin+.3f,12.5,reference)-.3f)<1e-6f&&std::abs(game::headingError(origin-.3f,12.5,reference)+.3f)<1e-6f,"a turn by the game shows as native minus VR heading");
  check(std::abs(game::headingError(float(std::remainder(double(origin)+3.,2*double(pi))),12.5,reference)-3.f)<1e-5f&&
        std::abs(game::headingError(float(std::remainder(double(origin)+3.5,2*double(pi))),12.5,reference)-float(3.5-2*double(pi)))<1e-5f,"the difference wraps to the short way round");
  check(game::headingCorrection(0)==0&&game::headingCorrection(.0015f)==0&&game::headingCorrection(-.0015f)==0,"no correction inside the dead band");
  check(std::abs(game::headingCorrection(.1f)-.03f)<1e-7f&&std::abs(game::headingCorrection(-.1f)+.03f)<1e-7f,"the correction is a part of the difference, with its sign");
  check(game::headingCorrection(2)==.15f&&game::headingCorrection(-2)==-.15f&&game::headingCorrection(std::numeric_limits<float>::quiet_NaN())==0,"the correction per update is bounded; an invalid difference gives none");
  // Closed loop: the game applies the correction one, two or three updates after the measurement it came from.
  bool converge=true,bounded=true;
  for(unsigned delay=1;delay<=3;++delay)for(float start:{.45f,-.3f,3.f}){
   float error=start,late=0;std::array<float,3> queue{};
   for(unsigned update=0;update<300;++update){
    queue[2]=queue[1];queue[1]=queue[0];queue[0]=game::headingCorrection(error); // from the difference after the previous update
    error-=queue[delay-1];bounded=bounded&&std::abs(error)<=std::abs(start)+1e-4f;if(update>=200)late=std::max(late,std::abs(error));
   }
   converge=converge&&late<=.0021f;
  }
  check(converge&&bounded,"the difference dies out and never exceeds where it began, for a correction arriving one to three updates late");
  bool cameras=true;for(unsigned id=0;id<64;++id){
   const bool own=id==5||id==8||id==10||id==12||id==14||id==17||id==23||id==24||id==25||id==28||id==29;
   cameras=cameras&&game::headingRealignCamera(uint8_t(id))==(game::cameraPresentation(uint8_t(id))==P::FirstPerson&&!own);}
  check(cameras&&game::headingRealignCamera(4)&&game::headingRealignCamera(19)&&game::headingRealignCamera(26)&&game::headingRealignCamera(27)&&game::headingRealignCamera(38)&&
        !game::headingRealignCamera(5)&&!game::headingRealignCamera(24)&&!game::headingRealignCamera(25)&&!game::headingRealignCamera(30)&&game::headingRealignCamera(34),
        "ordinary play and swimming cameras are realigned; target lock, hook shot, glider, climbing, head dive and the game's event cameras are left alone");
  try{game::headingError(0,std::numeric_limits<double>::infinity(),0);check(false,"invalid heading input rejected");}catch(const std::invalid_argument&){check(true,"invalid heading input rejected");}}
 // The correction's keeper.
 {using W=game::HeadingRealignWatch;using E=W::Event;
  // A ladder: the difference is first seen small in the climbing camera, which is left alone while the game turns the
  // camera by 119 degrees; at the foot of the ladder the ordinary camera is corrected until the difference is gone.
  {W w;float error=.024f;unsigned begins=0,bad=0,ends=0;bool quiet=true;
   for(unsigned t=0;t<40;++t){const auto s=w.update(error,23,true);begins+=s.event==E::Begin;quiet=quiet&&s.correction==0;error+=.0512f;}
   for(unsigned t=0;t<300;++t){const auto s=w.update(error,4,true);bad+=s.event==E::Rest||s.event==E::Held||s.event==E::Stopped;ends+=s.event==E::End;error-=s.correction;}
   check(begins==1&&quiet&&!bad&&ends==1&&!w.stopped()&&!w.active()&&std::abs(error)<=.0041f&&std::abs(w.peak())>2,"a difference that grew in a camera left alone (a ladder) is corrected afterwards, not given up");}
  // A camera that answers the other way: the correction rests and retries, then ends for the run; the harm is bounded.
  {W w;float error=.1f,worst=0;unsigned rests=0,stops=0;
   for(unsigned t=0;t<4000;++t){const auto s=w.update(error,4,true);rests+=s.event==E::Rest;stops+=s.event==E::Stopped;error+=s.correction;worst=std::max(worst,std::abs(error));}
   const float after=error;bool silent=true;for(unsigned t=0;t<50;++t){const auto s=w.update(error,4,true);silent=silent&&s.correction==0;error+=s.correction;}
   check(rests==7&&stops==1&&w.stopped()&&silent&&error==after&&worst<8,"a correction that makes the difference grow rests, retries and then ends for the run");}
  // A camera the game holds: nothing changes however long it is corrected; no correction there until the camera changes.
  {W w;const float error=.3f;unsigned held=0,corrected=0;for(unsigned t=0;t<700;++t){const auto s=w.update(error,4,true);held+=s.event==E::Held;corrected+=s.correction!=0;}
   const bool idle=w.update(error,4,true).correction==0;const bool resumed=w.update(error,19,true).correction!=0;
   check(held==1&&corrected==599&&idle&&resumed&&!w.stopped(),"a difference that never goes away is left until the camera changes");}
  // The game swings the camera for a moment while it is corrected (leaving the glider): no rest, it settles.
  {W w;float error=.03f;unsigned bad=0;
   for(unsigned t=0;t<300;++t){const auto s=w.update(error,4,true);bad+=s.event==E::Rest||s.event==E::Held||s.event==E::Stopped;error-=s.correction;if(t<12)error+=.09f;}
   check(!bad&&!w.active()&&std::abs(error)<=.0041f&&std::abs(w.peak())>.2f,"a short swing by the game under correction is ridden out");}
  // Measuring only (the correction switched off), and a difference too small to be an episode.
  {W w;bool none=true;unsigned begins=0;for(unsigned t=0;t<20;++t){const auto s=w.update(.2f,4,false);none=none&&s.correction==0;begins+=s.event==E::Begin;}
   W small;const auto s=small.update(.005f,4,true);
   check(none&&begins==1&&w.active()&&s.correction!=0&&s.event==E::None&&!small.active(),"measuring only gives no correction; a small difference is corrected without an episode");
   W nan;check(nan.update(std::numeric_limits<float>::quiet_NaN(),4,true).correction==0&&!nan.active(),"an invalid difference gives nothing");}}
 std::cout<<"RESULT "<<pass<<" PASS "<<fail<<" FAIL\n";return fail?1:0;
}
