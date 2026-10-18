#include <evr/head_gesture_feedback.hpp>
#include <evr/vertical_stick_mapping.hpp>
#include <evr/test_renderer.hpp>
#include <iostream>
using namespace evr;using namespace evr::motion;
int main(){unsigned checks=0,currentFrame=0;try{
 auto check=[&](bool v,const char* message){++checks;if(!v)throw std::runtime_error(std::string(message)+" frame="+std::to_string(currentFrame));};
 Log log("head-gesture-xr-client.jsonl",true);OpenXrClient xr(log);xr.init(1);Actions actions;actions.open(&xrGetInstanceProcAddr,xr.instance,xr.session,1);
 XrReferenceSpaceCreateInfo ci{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};ci.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;ci.poseInReferenceSpace=identityPose();XrSpace view=XR_NULL_HANDLE,recentered=XR_NULL_HANDLE;
 check(XR_SUCCEEDED(xrCreateReferenceSpace(xr.session,&ci,&view)),"view space");
 ci.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;ci.poseInReferenceSpace.position={.25f,0,-.1f};check(XR_SUCCEEDED(xrCreateReferenceSpace(xr.session,&ci,&recentered)),"local recentered space");
 auto timeout=std::chrono::steady_clock::now()+std::chrono::seconds(10);
 while(!xr.active()){check(xr.poll(),"session state");if(std::chrono::steady_clock::now()>timeout)throw std::runtime_error("session timeout");}
 HeadGesture mapper;VerticalStickMapping vertical;GestureFeedback feedback;unsigned entries=0,exits=0;
 {TestRenderer renderer(xr);
 for(unsigned n=1;n<=120;n++){
  currentFrame=n;check(xr.beginFrame(),"begin stereo frame");const auto frame=xr.pendingFrame();auto base=n<112?xr.local:recentered;uint64_t origin=n<112?1:2;
  auto hands=actions.sample(base,frame.predictedDisplayTime,frame.frame,origin);
  XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};check(XR_SUCCEEDED(xrLocateSpace(view,base,frame.predictedDisplayTime,&location)),"head pose at same predicted time");
  GestureHead head{location.pose,location.locationFlags,frame.frame,1,origin,frame.predictedDisplayTime};GesturePad raw;
  for(unsigned h=0;h<2;h++){
   const auto& v=hands.hands[h];if(v.stickActive)(h?raw.right:raw.left)=v.stick;
   if(v.stickClickActive&&v.stickClick)raw.buttons|=h?0x80:0x40;
   (h?raw.rightTrigger:raw.leftTrigger)=v.trigger;
  }
  auto out=mapper.update(hands,head,raw);bool expectedActive=(14<=n&&n<=34)||(48<=n&&n<=49)||(67<=n&&n<=68)||(90<=n&&n<=91)||(110<=n&&n<=111);
  check(out.active==expectedActive,"gesture active interval from CSV and dwell");entries+=out.entered;exits+=out.left;
  check(out.entered==(n==14||n==48||n==67||n==90||n==110),"one entry pulse per valid approach");
  uint16_t expected=0;if(18<=n&&n<=20)expected=1;if(21<=n&&n<=23)expected=8;if(24<=n&&n<=26)expected=4;if(27<=n&&n<=29)expected=2;if(30<=n&&n<=31)expected=0x30;if(n==33||n==34)expected=9;
  check(out.mappedButtons==expected,"D-pad/Start/Select exact mapping");
  check(out.pad.buttons==((n==38||(6<=n&&n<=13))?0xc0:expected),"no duplicate/stuck/entry-held buttons");
  if(expectedActive||n==35||n==36)check(out.pad.right.x==0&&out.pad.right.y==0,"mapped/exit-held stick cannot turn");
  if(n==38)check(out.pad.right.x==1&&std::abs(out.pad.right.y-.3f)<1e-6f,"normal stick after release");
  if(n!=50)check(out.pad.left.x==.25f&&out.pad.left.y==-.5f&&std::abs(out.pad.rightTrigger-.3f)<1e-6f,"left movement and trigger unchanged");
  const auto mapped=vertical.update(n*10,n*10,1,VerticalStickMode::Gameplay,hands,out);
  if(out.active)check(mapped.valid&&mapped.pad.buttons==out.pad.buttons&&!mapped.mappedButtons,"temple buttons have priority over vertical gameplay mapping");
  else check(mapped.pad.right.y==0,"ordinary controller pitch stays suppressed after mapping");
  if(!out.valid)check(!mapped.valid&&mapped.pad.buttons==0,"tracking failure releases mapped VR controls");
  Feedback attack;if(n>=14&&n<=18)attack={frame.frame,1,origin,n*10,{.2f,.4f}}; // synthetic recovery overlap, not native combat proof
  auto command=feedback.update(hands,out,attack,n*10);check(command.fresh,"one haptic command per frame");
  if(n>=14&&n<=18)check(command.amplitude[1]==.4f,"pulse end does not cancel recovery haptic");
  if(n==50||n==69||n==92||n==112)check(command.amplitude[1]==0,"tracking/focus/profile/origin invalidation stops pulse");
  for(unsigned h=0;h<2;h++){
   if(command.amplitude[h]>0)check(actions.vibrate(h,command.amplitude[h],command.duration[h]),"runtime receives bounded haptic");else actions.stop(h);
  }
  log.event("gesture_test_frame","\"frame\":"+std::to_string(n)+",\"active\":"+(out.active?"true":"false")+",\"entered\":"+(out.entered?"true":"false")+",\"buttons\":"+std::to_string(out.pad.buttons)+",\"right_amplitude\":"+std::to_string(command.amplitude[1])+",\"right_duration\":"+std::to_string(command.duration[1]));
  for(unsigned eye=0;eye<2;eye++){renderer.draw(eye,frame.imageIndices[eye],frame.views[eye],"");xr.markEyeRendered(eye,frame.frame);}xr.finishFrame();
 }
 check(xr.draws[0]==120&&xr.draws[1]==120,"same-frame stereo continues during gestures");}
 check(entries==5&&exits==5,"gesture lifecycle totals");check(actions.close(),"release actions/haptics");
 check(XR_SUCCEEDED(xrDestroySpace(view))&&XR_SUCCEEDED(xrDestroySpace(recentered)),"release spaces");xr.shutdown();check(!xr.session&&!xr.instance,"session shutdown");
 std::cout<<"PASS "<<checks<<" OpenXR head gesture checks (mock protocol only, no game UI writes)\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
