#include <evr/motion_collider_pose.hpp>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace evr::motion;
int main(){unsigned n=0;try{
 auto check=[&](bool v){if(++n&&!v)throw std::runtime_error("collider pose gate "+std::to_string(n));};
 ColliderPose p;p.valid=true;p.frame=30;p.session=2;p.origin=3;p.milliseconds=1000;p.serial=40;p.clientWorld=100;p.predictedTime=2000;p.root=1;p.item=123;p.child=50;
 std::array<float,7> transform{0,0,0,1,1,1,1};std::memcpy(p.transform.data()+24,transform.data(),28);
 float one=1;std::memcpy(p.attachment.data()+36,&one,4);p.attachment[41]=9;std::memcpy(p.attachment.data()+44,&p.root,4);
 ColliderEvidence e;e.now=1010;e.frame=30;e.session=2;e.origin=3;e.handMs=e.actorMs=e.visualMs=1000;
 e.clientWorld=100;e.actorWorld=e.impactWorld=200;e.root=e.actorEntity=e.impactRoot=1;e.item=e.visualItem=e.impactItem=123;e.visualChild=50;e.weaponPide=e.itemPide=4321;
 e.normalUi=e.firstPerson=e.tracked=e.active=e.alive=e.ready=e.ownerStable=e.slotStable=true;e.visualAttachment=e.impactAttachment=p.attachment;
 check(!colliderPoseRejection(p,e));
 for(unsigned bad=0;bad<36;bad++){auto a=p;auto b=e;
 switch(bad){case 0:a.valid=false;break;case 1:a.frame=0;break;case 2:a.predictedTime=0;break;case 3:a.serial=0;break;
 case 4:a.milliseconds=809;break;case 5:b.handMs=1011;break;case 6:b.actorMs=0;break;case 7:b.visualMs=509;break;
 case 8:a.session++;break;case 9:b.origin++;break;case 10:b.frame=29;break;case 11:b.frame=33;break;
 case 12:b.normalUi=false;break;case 13:b.firstPerson=false;break;case 14:b.tracked=false;break;case 15:b.active=false;break;case 16:b.alive=false;break;case 17:b.ready=false;break;
 case 18:a.clientWorld=0;break;case 19:b.clientWorld=200;break;case 20:b.actorWorld=0;break;case 21:b.impactWorld=100;break;
 case 22:b.root++;break;case 23:b.actorEntity++;break;case 24:b.impactRoot++;break;case 25:b.item++;break;case 26:b.visualItem++;break;case 27:b.impactItem++;break;case 28:b.visualChild++;break;case 29:b.weaponPide++;break;
 case 30:b.ownerStable=false;break;case 31:b.slotStable=false;break;case 32:b.impactAttachment[0]=1;break;case 33:a.transform[38]=0; a.transform[39]=0;break;
 case 34:{float f=std::numeric_limits<float>::quiet_NaN();std::memcpy(a.transform.data()+40,&f,4);break;}
 case 35:{float f=-2;std::memcpy(a.transform.data()+44,&f,4);break;}
 }
 check(colliderPoseRejection(a,b)!=nullptr);
 }
 // Freshness endpoints and independent worlds; IDs alone cannot authorize.
 for(unsigned lag=0;lag<=2;lag++){auto b=e;b.frame=p.frame+lag;b.now=1200;check(!colliderPoseRejection(p,b));b.now=1201;check(colliderPoseRejection(p,b)!=nullptr);}
 // Even mutually equal slot data require known root, right slot and neutral offsets.
 for(unsigned offset:{0u,40u,41u,44u}){auto a=p;auto b=e;a.attachment[offset]++;b.visualAttachment=b.impactAttachment=a.attachment;check(colliderPoseRejection(a,b)!=nullptr);}
 std::array<uint8_t,56> native{};for(unsigned i=0;i<56;++i)native[i]=uint8_t(i+1);
 const auto result=colliderTransform(p,native);
 check(std::memcmp(result.data(),p.transform.data(),40)==0);check(std::memcmp(result.data()+40,native.data()+40,16)==0);
 auto nonuniform=p;float assetScale=1.001f;std::memcpy(nonuniform.transform.data()+40,&assetScale,4);
 check(!colliderPoseRejection(nonuniform,e));check(colliderTransform(nonuniform,native)==result);
 check(p.transform[39]==0x3f&&p.clientWorld==100&&e.impactWorld==200);
 std::cout<<"PASS "<<n<<" collider pose isolation/lifecycle checks\n";return 0;
 }catch(const std::exception& ex){std::cerr<<"FAIL "<<ex.what()<<'\n';return 1;}}
