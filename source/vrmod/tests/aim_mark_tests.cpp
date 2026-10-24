#include <evr/aim_mark.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::aim;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 auto close=[](float a,float b,float e=1e-4f){return std::abs(a-b)<e;};
 // The bow: held upright ahead of the left shoulder, the string hand 0.5 m behind it, level. The line runs from that
 // hand through the arrow rest, straight ahead.
 {Pose bow{{0,0,0,1},{-.2f,1.4f,-.5f}};
  auto l=bowLine(bow,{1,1,1},{-.2f,1.40155006f,0});
  ck(l.valid&&close(l.direction.x,0)&&close(l.direction.y,0)&&close(l.direction.z,-1)&&close(l.origin.y,1.40155006f)&&close(l.origin.z,-.5f));
  l=bowLine(bow,{1,1,1},{-.2f,1.2f,0});                       // the string hand lower: the arrow points up
  ck(l.valid&&l.direction.y>.3f&&l.direction.z<0);
  l=bowLine(bow,{1,1,1},{.3f,1.40155006f,0});                 // the string hand to the right: the arrow points left
  ck(l.valid&&l.direction.x<-.5f);
  const auto p=markPoint(bowLine(bow,{1,1,1},{-.2f,1.40155006f,0}));
  ck(close(p.x,-.2f,1e-3f)&&close(p.z,-128.5f,1e-3f));       // 128 m along it
  ck(!bowLine(bow,{1,1,1},{-.2f,1.4f,-.45f}).valid);          // the hand at the bow: no line
  ck(!bowLine(bow,{1,1,1},{-.2f,1.4f,3}).valid);              // a tracking jump
  ck(!bowLine(bow,{1,3,1},{-.2f,1.4f,0}).valid&&!bowLine(bow,{1,1,1},{NAN,1.4f,0}).valid);}
 // A wand upright in the hand: the shot leaves its tip and flies along the hand's pointer, not along the wand.
 {Pose wand{{0,0,0,1},{.3f,1.2f,-.3f}};Pose pointer{euler(0,-.2f,0),{.3f,1.2f,-.3f}};  // the pointer looks a little down
  auto l=rangedLine(wand,{1,1,1},.5228f,false,pointer);
  ck(l.valid&&close(l.origin.x,.3f)&&close(l.origin.y,1.7228f)&&close(l.origin.z,-.3f)&&l.direction.z<-.9f&&l.direction.y<-.15f&&close(length(l.direction),1));
  // A staff in both hands, tilted forwards: along the staff.
  Pose staff{euler(0,-pi/2,0),{0,1.2f,-.3f}};                 // model +Y points along -Z
  l=rangedLine(staff,{1,1.1f,1},.63f,true,pointer);
  ck(l.valid&&close(l.direction.x,0)&&close(l.direction.y,0)&&close(l.direction.z,-1)&&close(l.origin.z,-.3f-.693f)&&close(l.origin.y,1.2f));
  ck(!rangedLine(wand,{1,2,1},.5228f,false,pointer).valid&&!rangedLine(wand,{1,1,1},-1,false,pointer).valid);
  Pose broken=pointer;broken.orientation={0,0,0,0};ck(!rangedLine(wand,{1,1,1},.5228f,false,broken).valid);}
 // Where a point shows in an eye: a symmetric 90 degree view, 2000 pixels each way.
 {const float a=pi/4;Pose eye{{0,0,0,1},{0,1.6f,0}};
  auto e=eyePlace(eye,-a,a,a,-a,{0,1.6f,-128},2000,2000);
  ck(e.on&&close(e.x,1000,1e-2f)&&close(e.y,1000,1e-2f)&&close(e.radius,2,1e-3f)&&close(eyePlace(eye,-a,a,a,-a,{0,1.6f,-128},6000,6000).radius,5.1f,1e-3f));   // straight ahead: the middle; 0.0017 rad of 2 tan units is 1.7 px over 2000 px - never under 2 - and 5.1 over 6000
  e=eyePlace(eye,-a,a,a,-a,{64,1.6f+32,-128},2000,2000);
  ck(e.on&&close(e.x,1500,1e-2f)&&close(e.y,750,1e-2f));                                    // right and up: rows run downwards
  e=eyePlace(eye,-a,a,a,-a,{-128,1.6f,-128},2000,2000);ck(e.on&&close(e.x,0,1e-2f));       // on the left edge: still shown
  e=eyePlace(eye,-a,a,a,-a,{-140,1.6f,-128},2000,2000);ck(!e.on);                           // past the edge
  ck(!eyePlace(eye,-a,a,a,-a,{0,1.6f,5},2000,2000).on);                                     // behind
  // The two eyes of a head: a point 128 m away is all but in the same place in both, one 1.5 m away is not.
  Pose left=eye,right=eye;left.position.x=-.032f;right.position.x=.032f;
  const auto fl=eyePlace(left,-a,a,a,-a,{0,1.6f,-128},2000,2000),fr=eyePlace(right,-a,a,a,-a,{0,1.6f,-128},2000,2000);
  ck(fl.on&&fr.on&&fl.x>fr.x&&fl.x-fr.x<1);
  const auto nl=eyePlace(left,-a,a,a,-a,{0,1.6f,-1.5f},2000,2000),nr=eyePlace(right,-a,a,a,-a,{0,1.6f,-1.5f},2000,2000);
  ck(nl.x-nr.x>40);
  // An eye turned a quarter to the left sees a point on its left (the room's -X) in the middle.
  Pose turned{euler(pi/2,0,0),{0,1.6f,0}};
  e=eyePlace(turned,-a,a,a,-a,{-128,1.6f,0},2000,2000);ck(e.on&&close(e.x,1000,1e-1f)&&close(e.y,1000,1e-1f));
  // An asymmetric view (the usual headset eye): the middle of the image is not straight ahead.
  e=eyePlace(eye,-.9f,.7f,.8f,-.85f,{0,1.6f,-128},1800,1900);
  ck(e.on&&close(e.x,(std::tan(.9f))/(std::tan(.7f)+std::tan(.9f))*1800,1e-1f)&&close(e.y,(std::tan(.8f))/(std::tan(.8f)+std::tan(.85f))*1900,1e-1f));
  ck(!eyePlace(eye,a,a,a,-a,{0,1.6f,-128},2000,2000).on&&!eyePlace(eye,-a,a,a,-a,{0,1.6f,-128},0,2000).on&&!eyePlace(eye,-a,a,a,-a,{NAN,0,0},2000,2000).on);}
 // The cluster: the reticle's part of the HUD.
 {const auto r=clusterRect(1920,1080);ck(r.x0==760&&r.y0==370&&r.x1==1560&&r.y1==710);
  const auto half=clusterRect(960,540);ck(half.x0==380&&half.y0==185&&half.x1==780&&half.y1==355);
  const auto tiny=clusterRect(64,32);ck(tiny.x0>=1&&tiny.x1<=63&&tiny.x1>tiny.x0&&tiny.y1>tiny.y0);
  // The plate around it: four parts that cover everything but the cluster, each where its pixels were on the plate.
  const auto parts=plateAround(r,1920,1080,2.f);long area=0;
  for(const auto& p:parts){ck(p.rect.x1>p.rect.x0&&p.rect.y1>p.rect.y0&&p.width>0&&p.height>0);area+=long(p.rect.x1-p.rect.x0)*(p.rect.y1-p.rect.y0);}
  ck(area==1920L*1080-long(r.x1-r.x0)*(r.y1-r.y0));
  ck(close(parts[0].width,2)&&close(parts[0].height,370*2.f/1920)&&close(parts[0].pose.position.x,0)&&close(parts[0].pose.position.y,(540-185)*2.f/1920));   // above
  ck(close(parts[1].pose.position.y,(540-(710+1080)*.5f)*2.f/1920)&&close(parts[1].height,370*2.f/1920));                                                   // below
  ck(close(parts[2].width,760*2.f/1920)&&close(parts[2].pose.position.x,(380-960)*2.f/1920)&&close(parts[2].pose.position.y,0));                              // left
  ck(close(parts[3].width,360*2.f/1920)&&close(parts[3].pose.position.x,((1560+1920)*.5f-960)*2.f/1920));                                                    // right
  // At the aimed point: 12 m straight ahead of an eye at the origin, the quad faces back to the eye.
  auto c=clusterAt(r,1920,1080,2.f,1.5f,{0,1.6f,0},{0,1.6f,-12});
  float s=2.f/1920*(12/1.5f);
  ck(close(c.width,800*s,1e-3f)&&close(c.height,340*s,1e-3f));
  ck(close(rotate(c.pose.orientation,{0,0,1}).z,1)&&close(rotate(c.pose.orientation,{1,0,0}).x,1)&&close(rotate(c.pose.orientation,{0,1,0}).y,1));
  ck(close(c.pose.position.x,200*s,1e-3f)&&close(c.pose.position.y,1.6f,1e-3f)&&close(c.pose.position.z,-12,1e-3f));   // the rect's middle is 200 pixels right of the reticle
  // The reticle itself - the image's middle - lies on the aimed point: go back from the quad's centre.
  Vec3 reticle=c.pose.position-rotate(c.pose.orientation,{200*s,0,0});ck(length(reticle-Vec3{0,1.6f,-12})<1e-3f);
  // Seen from the eye a pixel there is as large as on the plate: 800 pixels of 1920 on 2 m at 1.5 m.
  ck(close(c.width/12,800*2.f/1920/1.5f,1e-4f));
  // A point 128 m out: the quad stands 20 m out on the line to it, as large to the eye as before.
  c=clusterAt(r,1920,1080,2.f,1.5f,{0,1.6f,0},{0,1.6f,-128});s=2.f/1920*(clusterFarthest/1.5f);
  ck(close(c.width,800*s,1e-3f)&&close(c.pose.position.z,-clusterFarthest,1e-3f)&&close(c.width/clusterFarthest,800*2.f/1920/1.5f,1e-4f));
  reticle=c.pose.position-rotate(c.pose.orientation,{200*s,0,0});ck(length(reticle-Vec3{0,1.6f,-clusterFarthest})<1e-3f);
  // To the left and above: still facing the eye, upright.
  c=clusterAt(r,1920,1080,2.f,1.5f,{0,1.6f,0},{-90,60,-70});
  const Vec3 z=rotate(c.pose.orientation,{0,0,1}),x=rotate(c.pose.orientation,{1,0,0});
  const Vec3 want=Vec3{90,1.6f-60,70}*(1/length(Vec3{90,1.6f-60,70}));
  ck(length(z-want)<1e-3f&&std::abs(x.y)<1e-4f&&rotate(c.pose.orientation,{0,1,0}).y>0);
  ck(clusterAt(r,1920,1080,2.f,1.5f,{0,1.6f,0},{0,1.6f,-.2f}).width==0&&clusterAt(r,1920,1080,2.f,1.5f,{0,1.6f,0},{NAN,0,0}).width==0);
  c=clusterAt(r,1920,1080,2.f,1.5f,{0,0,0},{0,100,0});ck(c.width>0&&close(rotate(c.pose.orientation,{0,0,1}).y,-1));}   // straight above: a place all the same
 {// partAt: the anchor pixel lies on the line from the eye to the point; with the image's middle as the anchor it is clusterAt.
  const PixelRect r{800,300,1100,450};const Vec3 eye{0,1.6f,0},point{2,2.5f,-6};
  const auto a=partAt(r,960,540,1920,1080,2.f,1.5f,eye,point),b=clusterAt(r,1920,1080,2.f,1.5f,eye,point);
  ck(a.width>0&&close(a.width,b.width)&&close(a.pose.position.x,b.pose.position.x)&&close(a.pose.position.y,b.pose.position.y)&&close(a.pose.position.z,b.pose.position.z));
  const auto c=partAt(r,950,420,1920,1080,2.f,1.5f,eye,point);const float s=c.width/300.f;
  const Vec3 anchor=c.pose.position+rotate(c.pose.orientation,Vec3{(950-950.f)*s,(375-420.f)*s,0});   // the anchor pixel on the quad (the rect's centre is 950, 375)
  const Vec3 toAnchor=anchor-eye,toPoint=point-eye;const Vec3 normal=cross(toAnchor,toPoint);
  ck(length(normal)<1e-3f*length(toAnchor)*length(toPoint)&&dot(toAnchor,toPoint)>0);}
 {// plateWithout: the pieces and the parts cover the image once, the pieces stay out of the parts; overlapping parts are refused.
  const auto area=[](const PixelRect& r){return int64_t(r.x1-r.x0)*int64_t(r.y1-r.y0);};
  const auto check=[&](const PixelRect* cuts,size_t count){
   std::array<QuadPart,10> out{};size_t pieces=0;ck(plateWithout(cuts,count,1920,1080,2.f,out,pieces)&&pieces<=10);
   int64_t sum=0;for(size_t i=0;i<count;++i)sum+=area(cuts[i]);
   for(size_t i=0;i<pieces;++i){const auto& r=out[i].rect;sum+=area(r);
    for(size_t j=0;j<count;++j){const auto& c=cuts[j];ck(!(r.x0<c.x1&&c.x0<r.x1&&r.y0<c.y1&&c.y0<r.y1));}
    ck(close(out[i].width,float(r.x1-r.x0)*2.f/1920)&&close(out[i].pose.position.x,(float(r.x0+r.x1)*.5f-960)*2.f/1920));}
   ck(sum==int64_t(1920)*1080);return pieces;};
  const PixelRect cluster=clusterRect(1920,1080);const PixelRect bar{800,200,1100,350},beside{1600,400,1800,700},under{700,720,1000,900};
  ck(check(&cluster,1)==4);                                       // as plateAround
  {const PixelRect two[2]{cluster,bar};check(two,2);}
  {const PixelRect two[2]{cluster,beside};check(two,2);}
  {const PixelRect two[2]{under,cluster};check(two,2);}
  {const PixelRect two[2]{cluster,{900,500,1000,560}};std::array<QuadPart,10> out{};size_t pieces=0;ck(!plateWithout(two,2,1920,1080,2.f,out,pieces));}}
 // How far along the line the mark sits: a body in the way, else the level ground of the feet, else 128 m.
 {const float nan=std::nanf("");int by=-1;
  const Line level{true,{0,1.3f,0},{0,0,-1}};
  ck(markReach(level,0,nullptr,0,&by)==markDistance&&by==0);                                    // level: no ground ahead
  const Line down{true,{0,1.3f,0},Vec3{0,-.45f,-.893f}*(1/std::sqrt(.45f*.45f+.893f*.893f))};
  float t=markReach(down,0,nullptr,0,&by);ck(by==1&&close(t,1.3f/.45f,2e-2f)&&close(markPoint(down,t).y,0,1e-3f));   // the ground 2.9 m along
  ck(markReach(down,nan,nullptr,0,&by)==markDistance&&by==0);                                   // the feet's height not known
  const Line up{true,{0,1.3f,0},{0,.6f,-.8f}};ck(markReach(up,0,nullptr,0,&by)==markDistance&&by==0);
  // A creature 6 m ahead: the line ends on its near side, also when the ground would be hit further on - or nearer.
  const MarkBody ahead{{0,0,-6}};
  t=markReach(level,0,&ahead,1,&by);ck(by==2&&close(t,5.4f,1e-3f));
  const Line shallow{true,{0,1.3f,0},Vec3{0,-.1f,-1}*(1/std::sqrt(1.01f))};
  t=markReach(shallow,0,&ahead,1,&by);ck(by==2&&close(markPoint(shallow,t).z,-5.4f,1e-3f));    // the ground would be 13 m out
  const MarkBody below{{0,-8,-6}};                                                              // from a ledge: the creature stands 8 m lower
  const Line ledge{true,{0,1.3f,0},Vec3{0,-8.2f,-6}*(1/std::sqrt(8.2f*8.2f+36))};
  t=markReach(ledge,0,&below,1,&by);ck(by==2&&t>1.6f*5&&markPoint(ledge,t).y<-5);               // not the feet's level 1.6 m along
  // Past it, over it, behind it: no body.
  const MarkBody aside{{2,0,-6}};ck(markReach(level,0,&aside,1,&by)==markDistance&&by==0);
  const Line high{true,{0,3,0},{0,0,-1}};ck(markReach(high,0,&ahead,1,&by)==markDistance&&by==0);
  const MarkBody behind{{0,0,6}};ck(markReach(level,0,&behind,1,&by)==markDistance&&by==0);
  // Straight down onto a creature's head: the top.
  const Line drop{true,{0,5,-6},{0,-1,0}};t=markReach(drop,0,&ahead,1,&by);ck(by==2&&close(t,5-2.2f,1e-3f));
  // The nearest of several; never nearer than a metre; nonsense is left out.
  const MarkBody two[]{{{0,0,-9}},{{0,0,-4}},{{NAN,0,0}}};t=markReach(level,0,two,3,&by);ck(by==2&&close(t,3.4f,1e-3f));
  const MarkBody close1{{0,0,-.8f}};ck(markReach(level,0,&close1,1,&by)==markNearest&&by==2);
  ck(markReach(Line{},0,&ahead,1,&by)==markDistance&&by==0);
  ck(close(length(markPoint(level)-Vec3{0,1.3f,-128}),0,1e-3f));}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<"aim_mark FAIL "<<e.what()<<'\n';return 1;}}
