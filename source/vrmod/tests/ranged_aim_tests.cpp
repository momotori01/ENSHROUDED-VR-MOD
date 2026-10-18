#include <evr/ranged_aim.hpp>
#include <iostream>
#include <string>
using namespace evr;using namespace evr::motion;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 const Vec3 axes[]{{1,0,0},{0,1,0},{0,0,1}};
 ck(rangedAimModel(1255239784u)&&rangedAimModel(1255239784u)->emitterY==.63f);ck(rangedAimModel(3851016100u)&&rangedAimModel(3851016100u)->emitterY==.5228f);
 ck(!rangedAimModel(327157552u)&&!rangedAimModel(0)&&!rangedAimModel(1940764111u)); // bow, nothing, melee
 // Every catalog staff and wand: the group's shared emitter. The wand is the one whose trigger becomes the attack press.
 // (1.0: 49 staffs and 58 wands; June 40 and 43.)
 {unsigned staffs=0,wands=0;
  for(const auto& a:catalogRangedAssets){const auto* m=rangedAimModel(a.item);const auto type=catalogRangedRigs[a.rig].type;
   ck(type==4?!m:m&&m->type==type&&m->emitterY==(type==5?.63f:.5228f));ck(rangedAimWand(a.item)==(type==6));staffs+=type==5;wands+=type==6;}
  ck(staffs==49&&wands==58);ck(rangedAimModel(3657818170u)->type==5&&!rangedAimWand(1255239784u)&&!rangedAimWand(0));}
 // lookAlong: +Z on the direction, +Y in the plane of direction and hint, on the hint's side.
 for(Vec3 d:{Vec3{0,0,1},Vec3{0,0,-1},Vec3{1,0,0},Vec3{.3f,.2f,-.9f},Vec3{-.5f,.8f,.1f},Vec3{0,-.6f,.8f}}){
  const auto q=lookAlong(d,{0,1,0});const auto f=rotate(q,{0,0,1}),u=rotate(q,{0,1,0});const auto unit=d*(1/length(d));
  ck(length(f-unit)<2e-5f);ck(std::abs(dot(u,cross(Vec3{0,1,0},unit)))<2e-5f);ck(u.y>0);
 }
 {const auto q=lookAlong({0,2,0},{0,1,0});ck(length(rotate(q,{0,0,1})-Vec3{0,1,0})<2e-5f);} // straight up: any roll, still on the line
 for(Vec3 bad:{Vec3{0,0,0},Vec3{NAN,0,1}}){bool threw=false;try{lookAlong(bad,{0,1,0});}catch(const std::invalid_argument&){threw=true;}ck(threw);}

 // The shot line is carried from tracking space into the native world through the model:
 // angles to the model axes and distances from the model origin are preserved (Z reflected).
 unsigned cases=0;
 for(float yaw:{-2.f,-.4f,.9f})for(float pitch:{-.7f,.2f})for(float roll:{-.5f,.6f})for(float worldYaw:{-1.3f,.5f,2.8f}){
  const Pose model{euler(yaw,pitch,roll),{.4f,1.3f,-.2f}};const Quat world=euler(worldYaw,.3f,-.2f);const WorldPositionQ32 at{150000000000ll,3628173623194ll,170000000000ll};
  for(Vec3 localOrigin:{Vec3{0,.63f,0},Vec3{.1f,.5f,-.2f}})for(Vec3 localDirection:{Vec3{0,1,0},Vec3{.2f,.1f,-.9f},Vec3{-.6f,.3f,.5f}}){
   const auto origin=model.position+rotate(model.orientation,localOrigin),direction=rotate(model.orientation,localDirection);
   const auto aim=rangedWorldAim(model,at,world,origin,direction);
   const Vec3 offset{float(relativeQ32(aim.origin.x,at.x)),float(relativeQ32(aim.origin.y,at.y)),float(relativeQ32(aim.origin.z,at.z))};
   ck(std::abs(length(offset)-length(localOrigin))<3e-5f);ck(std::abs(length(aim.direction)-1)<2e-5f);
   const auto unit=localDirection*(1/length(localDirection));
   for(unsigned axis=0;axis<3;axis++){
    const float sign=axis==2?-1.f:1.f;const auto worldAxis=rotate(world,axes[axis]);
    ck(std::abs(dot(aim.direction,worldAxis)-sign*dot(unit,axes[axis]))<3e-5f);
    ck(std::abs(dot(offset,worldAxis)-sign*dot(localOrigin,axes[axis]))<3e-5f);
   }
   ck(length(rotate(aim.orientation,{0,0,1})-aim.direction)<3e-5f);++cases;
  }
 }
 ck(cases==3*2*2*3*2*3);
 // Pointer aim: the emitter on the weapon's +Y axis, the direction from the aim pose only.
 {const Pose model{euler(.3f,-.2f,.1f),{0,1,0}};const Quat world{0,0,0,1};const WorldPositionQ32 at{1000000000000ll,2000000000000ll,3000000000000ll};
  const Pose pointer{euler(1.1f,.2f,0),{5,5,5}};const auto weapon=*rangedAimModel(3851016100u);
  const auto aim=rangedPointerAim(weapon,model,{1,1.001f,1},at,world,pointer);
  const Vec3 offset{float(relativeQ32(aim.origin.x,at.x)),float(relativeQ32(aim.origin.y,at.y)),float(relativeQ32(aim.origin.z,at.z))};
  ck(length(offset-Vec3{0,.5228f*1.001f,0})<3e-5f);
  const auto local=rotate(inverse(model.orientation),rotate(pointer.orientation,{0,0,-1}));ck(length(aim.direction-Vec3{local.x,local.y,-local.z})<3e-5f);
  const auto turned=rangedPointerAim(weapon,model,{1,1.001f,1},at,world,{euler(-.4f,0,0),{9,9,9}});
  const Vec3 same{float(relativeQ32(turned.origin.x,aim.origin.x)),float(relativeQ32(turned.origin.y,aim.origin.y)),float(relativeQ32(turned.origin.z,aim.origin.z))};
  ck(length(same)<1e-6f&&length(turned.direction-aim.direction)>.5f);
  for(unsigned bad=0;bad<4;bad++){bool threw=false;try{
   if(bad==0)rangedPointerAim(weapon,model,{1,2,1},at,world,pointer);if(bad==1)rangedPointerAim(weapon,{model.orientation,{NAN,0,0}},{1,1,1},at,world,pointer);
   if(bad==2)rangedWorldAim(model,at,world,model.position,{0,0,0});if(bad==3)rangedWorldAim(model,at,world,model.position+Vec3{0,4,0},{0,0,-1});
  }catch(const std::invalid_argument&){threw=true;}ck(threw);}}
 // Axis aim (staff held with both hands): the same emitter, the direction along the weapon whatever the pointer does.
 {const Pose model{euler(.3f,-.2f,.1f),{0,1,0}};const Quat world=euler(.7f,0,0);const WorldPositionQ32 at{1000000000000ll,2000000000000ll,3000000000000ll};
  const auto weapon=*rangedAimModel(1255239784u);const Vec3 scale{1,1.002f,1};
  const auto aim=rangedAxisAim(weapon,model,scale,at,world);
  const Vec3 offset{float(relativeQ32(aim.origin.x,at.x)),float(relativeQ32(aim.origin.y,at.y)),float(relativeQ32(aim.origin.z,at.z))};
  const auto worldAxis=rotate(world,{0,1,0});
  ck(length(aim.direction-worldAxis)<3e-5f);ck(length(offset-worldAxis*(.63f*1.002f))<3e-5f);
  const auto same=rangedPointerAim(weapon,model,scale,at,world,{multiply(model.orientation,alignDirections({0,0,-1},{0,1,0})),{3,3,3}}); // a pointer along the weapon
  const Vec3 apart{float(relativeQ32(same.origin.x,aim.origin.x)),float(relativeQ32(same.origin.y,aim.origin.y)),float(relativeQ32(same.origin.z,aim.origin.z))};
  ck(length(same.direction-aim.direction)<3e-5f&&length(apart)<1e-6f);
  bool threw=false;try{rangedAxisAim(weapon,model,{1,2,1},at,world);}catch(const std::invalid_argument&){threw=true;}ck(threw);}
 // The staff cast event has no projectile flag at +0x148; the camera math is the same.
 {BowSpawnEvent event{};event[0x90]=3;event[0x148]=121;const float identity[4]{0,0,0,1};std::memcpy(event.data()+0xd8,identity,16);
  const BowWorldAim aim{{1000000000000ll,2000000000000ll,3000000000000ll},euler(.4f,.1f,0),{0,0,1}};
  bool threw=false;try{bowAimCamera(aim,event);}catch(const std::invalid_argument&){threw=true;}ck(threw);
  const auto camera=bowAimCamera(aim,event,false);WorldPositionQ32 p;Quat q;float distance;std::memcpy(&p,camera.data(),24);std::memcpy(&q,camera.data()+24,16);std::memcpy(&distance,camera.data()+40,4);
  ck(p.x==aim.origin.x&&p.y==aim.origin.y&&p.z==aim.origin.z&&distance==0);ck(std::abs(q.x-aim.orientation.x)<1e-6f&&std::abs(q.w-aim.orientation.w)<1e-6f);
  event[0x90]=7;threw=false;try{bowAimCamera(aim,event,false);}catch(const std::invalid_argument&){threw=true;}ck(threw);}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
