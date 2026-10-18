#include <evr/pointer_aim_camera.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::game;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 constexpr auto tracked=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT|XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT;
 HeadInputSnapshot head;head.anchor.root={1000000000000ll,2000000000000ll,3000000000000ll};head.anchor.orientation={0,0,0,1};head.anchor.cameraOffsetGameUnits={0,1.65f,0};
 head.anchor.worldUnitsPerMeter=1;head.flags=head.pointerFlags=tracked;head.xrFrame=7;head.originEpoch=1;head.predictedDisplayTime=100;head.current=true;
 head.head={euler(.3f,-.1f,.05f),{0,0,0}};head.pointer={euler(-.9f,.4f,.6f),{.25f,-.35f,.3f}};

 // --- The ray of the cursor (the camera in the context of client_cursor) ---
 // The game's camera: somewhere behind the player, 3.5 m of boom, a byte of the input after it.
 CursorCameraBytes source{},out{};
 auto put=[&](CursorCameraBytes& b,const WorldPositionQ32& p,const Quat& q,std::array<float,3> scale,float distance){
    std::memcpy(b.data(),&p,24);std::memcpy(b.data()+0x18,&q,16);std::memcpy(b.data()+0x28,scale.data(),12);std::memcpy(b.data()+0x38,&distance,4);};
 const WorldPositionQ32 nativePosition{999000000000ll,2005000000000ll,2990000000000ll};const Quat nativeOrientation=euler(2.1f,.35f,0);
 put(source,nativePosition,nativeOrientation,{1,1,1},3.5f);source[0x34]=0x5a;source[0x3c]=1;source[0x3d]=0x77;
 auto r=pointCursorRay(source,out,head,0);
 ck(r.applied&&std::string(r.reason)=="applied"&&onlyCursorRayChanged(source,out)&&out!=source);
 ck(r.nativePosition.x==nativePosition.x&&r.nativePosition.z==nativePosition.z&&r.nativeDistance==3.5f&&length(rotate(r.nativeOrientation,{0,0,1})-rotate(nativeOrientation,{0,0,1}))<1e-6f);
 {WorldPositionQ32 p;Quat q;float distance=9;std::array<float,3> scale;std::memcpy(&p,out.data(),24);std::memcpy(&q,out.data()+0x18,16);std::memcpy(scale.data(),out.data()+0x28,12);std::memcpy(&distance,out.data()+0x38,4);
  ck(p.x==r.pointer.cameraPosition.x&&p.y==r.pointer.cameraPosition.y&&p.z==r.pointer.cameraPosition.z);
  ck(distance==0&&scale==std::array<float,3>{1,1,1}&&out[0x34]==0x5a&&out[0x3c]==1&&out[0x3d]==0x77);   // the ray starts at the hand; the rest stays
  ck(length(rotate(q,{0,0,1})-r.pointer.aimForward)<1e-5f);                                 // along the pointer
  ck(std::abs(rotate(q,{1,0,0}).y)<1e-5f);                                                  // without roll: the piece is not tilted by the wrist
  // The hand is 0.25 m right, 0.35 m below, 0.3 m in front of the tracking origin (anchor at the eye height).
  const float units=4294967296.f;
  ck(std::abs(float(p.x-head.anchor.root.x)/units-.25f)<1e-3f&&std::abs(float(p.y-head.anchor.root.y)/units-(1.65f-.35f))<1e-3f);
  // The same pose handed over as the head gives the same camera: the pointer goes through the head's conversion.
  auto same=head;same.head=head.pointer;NativeInputState g;g.enabled=g.known=g.local=g.ready=true;g.inputMode=1;g.playerFlags=1;
  const auto expected=proposeInput(g,same,0);ck(expected.active()&&expected.cameraPosition.x==p.x&&expected.cameraPosition.z==p.z&&length(rotate(expected.aimWithoutRoll,{0,0,1})-rotate(q,{0,0,1}))<1e-6f);}
 // The head does not matter for the ray: only the hand has to be tracked.
 {auto h=head;h.flags=0;ck(pointCursorRay(source,out,h,0).applied);
  h=head;h.pointerFlags=XR_SPACE_LOCATION_POSITION_VALID_BIT;r=pointCursorRay(source,out,h,0);ck(!r.applied&&std::string(r.reason)=="tracking"&&out==source);
  h=head;h.pointerFlags=0;ck(!pointCursorRay(source,out,h,0).applied&&out==source);
  h=head;h.current=false;ck(!pointCursorRay(source,out,h,0).applied&&out==source);}
 // A swung tool aims with the headset: the same block takes the head's camera, and the hand does not matter.
 {auto q=pointCursorRay(source,out,head,0,CursorRaySource::Head);ck(q.applied&&onlyCursorRayChanged(source,out));
  NativeInputState g;g.enabled=g.known=g.local=g.ready=true;g.inputMode=1;g.playerFlags=1;const auto expected=proposeInput(g,head,0);
  WorldPositionQ32 p;Quat o;float distance=9;std::memcpy(&p,out.data(),24);std::memcpy(&o,out.data()+0x18,16);std::memcpy(&distance,out.data()+0x38,4);
  ck(expected.active()&&p.x==expected.cameraPosition.x&&p.y==expected.cameraPosition.y&&p.z==expected.cameraPosition.z&&distance==0);
  ck(length(rotate(o,{0,0,1})-expected.aimForward)<1e-5f&&length(expected.aimForward-r.pointer.aimForward)>.5f);   // the head's direction, not the hand's
  auto h=head;h.pointerFlags=0;ck(pointCursorRay(source,out,h,0,CursorRaySource::Head).applied&&!pointCursorRay(source,out,h,0).applied);
  h=head;h.flags=0;ck(!pointCursorRay(source,out,h,0,CursorRaySource::Head).applied&&out==source&&pointCursorRay(source,out,h,0).applied);}
 // A block that does not look like the camera is left alone (another build of the game).
 {auto b=source;put(b,nativePosition,nativeOrientation,{1,1,2},3.5f);r=pointCursorRay(b,out,head,0);ck(!r.applied&&std::string(r.reason)=="layout"&&out==b);
  put(b,nativePosition,{0,0,0,2},{1,1,1},3.5f);ck(!pointCursorRay(b,out,head,0).applied&&out==b);
  put(b,nativePosition,nativeOrientation,{1,1,1},-1);ck(!pointCursorRay(b,out,head,0).applied&&out==b);
  put(b,nativePosition,nativeOrientation,{1,1,1},200);ck(!pointCursorRay(b,out,head,0).applied&&out==b);
  put(b,nativePosition,nativeOrientation,{1,1,1},std::nanf(""));ck(!pointCursorRay(b,out,head,0).applied&&out==b);
  put(b,nativePosition,nativeOrientation,{1,1,1},0);ck(pointCursorRay(b,out,head,0).applied);}   // the first-person building camera has no boom

 // --- The head camera in building mode (the input that is sent) ---
 ClientInputBytes input{},sent{};
 for(size_t i=0;i<input.size();++i)if(i<0x20||(i>=0x4c&&i<clientStick)||(i>=clientStick+8&&i<clientMask)||i>=clientDigital+8)input[i]=uint8_t(i*7+3);   // recognisable filler
 input[clientMask]=buildingClientMask;input[clientBuildingCamera]=0;input[clientInputMode]=1;
 {const std::array<float,2> s{.3f,.8f};std::memcpy(input.data()+clientStick,s.data(),8);}
 PointerAimGate gate;auto& w=gate.walking;w.player.normalUiBound=false;w.player.playerStateCurrent=true;w.player.playerFlags=1;
 w.actorStateCurrent=w.gravityPresent=w.gravityActive=true;w.actorState=(1ull<<0)|(1ull<<50);w.gravityDirection={0,-1,0};gate.buildingScreenBound=true;
 // The screen of building mode with its mask: the head camera, and nothing else changes (no button, not the stick, not the mask).
 auto b=patchBuildingHeadCamera(input,sent,gate,head,0);
 ck(b.status==IdleHeadStatus::Applied&&onlyHeadCameraChanged(input,sent)&&sent!=input&&sent[clientMask]==buildingClientMask);
 {WorldPositionQ32 p;Quat q;std::memcpy(&p,sent.data()+0x20,24);std::memcpy(&q,sent.data()+0x38,16);
  NativeInputState g;g.enabled=g.known=g.local=g.ready=true;g.inputMode=1;g.playerFlags=1;const auto expected=proposeInput(g,head,0);
  ck(expected.active()&&p.x==expected.cameraPosition.x&&p.y==expected.cameraPosition.y&&length(rotate(q,{0,0,1})-expected.aimForward)<1e-5f);   // the head, not the hand
  // The same camera as on the HUD in live play.
  auto hud=gate.walking;hud.player.normalUiBound=true;auto plain=input;plain[clientMask]=0;ClientInputBytes live;
  ck(patchVrLocomotionCamera(plain,live,hud,head,0).status==IdleHeadStatus::Applied&&!std::memcmp(live.data()+0x20,sent.data()+0x20,0x2c));}
 {bool kept=true;for(unsigned bit=0;bit<64;++bit){auto s=input;s[clientDigital+bit/8]=uint8_t(1u<<(bit%8));auto q=patchBuildingHeadCamera(s,sent,gate,head,0);kept=kept&&q.status==IdleHeadStatus::Applied&&onlyHeadCameraChanged(s,sent);}ck(kept);}
 // Without the bound screen the mask keeps the game's own bytes; another mask does too.
 {auto g=gate;g.buildingScreenBound=false;ck(patchBuildingHeadCamera(input,sent,g,head,0).status==IdleHeadStatus::Ui&&sent==input);
  g.walking.player.normalUiBound=true;ck(patchBuildingHeadCamera(input,sent,g,head,0).status==IdleHeadStatus::Ui&&sent==input);   // the HUD is live play's business
  auto other=input;other[clientMask]=1;ck(patchBuildingHeadCamera(other,sent,gate,head,0).status==IdleHeadStatus::SpecialMode&&sent==other);
  auto none=input;none[clientMask]=0;ck(patchBuildingHeadCamera(none,sent,gate,head,0).status==IdleHeadStatus::Applied&&sent[clientMask]==0);}
 // What keeps the game's own bytes: keyboard play, target lock, the first-person building camera, unusual gravity, a lost head.
 {auto k=input;k[clientInputMode]=0;ck(patchBuildingHeadCamera(k,sent,gate,head,0).status==IdleHeadStatus::SpecialMode&&sent==k);
  auto g=gate;g.walking.player.playerFlags|=1u<<12;ck(patchBuildingHeadCamera(input,sent,g,head,0).status==IdleHeadStatus::PlayerState&&sent==input);
  auto f=input;f[clientBuildingCamera]=1;ck(patchBuildingHeadCamera(f,sent,gate,head,0).status==IdleHeadStatus::SpecialMode&&sent==f);
  g=gate;g.walking.gravityDirection={0,1,0};ck(patchBuildingHeadCamera(input,sent,g,head,0).status==IdleHeadStatus::SpecialMode&&sent==input);
  auto h=head;h.flags=0;ck(patchBuildingHeadCamera(input,sent,gate,h,0).status==IdleHeadStatus::Tracking&&sent==input);
  h=head;h.pointerFlags=0;ck(patchBuildingHeadCamera(input,sent,gate,h,0).status==IdleHeadStatus::Applied);}   // the hand does not matter for the walk
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
