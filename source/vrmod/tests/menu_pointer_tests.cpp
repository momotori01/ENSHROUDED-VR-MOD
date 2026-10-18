#include <evr/menu_pointer.hpp>
#include <iostream>
#include <stdexcept>
#include <string>
using namespace evr;using namespace evr::menu;
int main(){try{unsigned n=0;auto ck=[&](bool b,int line=__builtin_LINE()){++n;if(!b)throw std::runtime_error(std::to_string(n)+" at line "+std::to_string(line));};
 auto close=[](float a,float b){return std::abs(a-b)<1e-4f;};
 // The plate as the frame submits it: 1.5 m in front of the head, 2 m wide, 1920x1080.
 Plate plate;plate.pose.position={0,0,-1.5f};
 auto s=spot(plate,{0,0,0},{0,0,-1});
 ck(s.on&&close(s.x,0)&&close(s.y,0)&&s.pixelX==960&&s.pixelY==540);            // straight ahead: the middle
 s=spot(plate,{.2005f,-.3004f,0},{0,0,-1});
 ck(s.on&&close(s.x,.2005f)&&close(s.y,-.3004f)&&s.pixelX==1152&&s.pixelY==828);      // right and down: rows run downwards
 s=spot(plate,{0,0,0},{-1,.5625f,-1.5f});
 ck(s.on&&s.pixelX==0&&s.pixelY==0);                                           // the top left corner
 s=spot(plate,{0,0,0},{1,-.5625f,-1.5f});
 ck(s.on&&s.pixelX==1919&&s.pixelY==1079);                                     // the bottom right corner stays inside the image
 s=spot(plate,{0,0,0},{2,0,-1.5f});
 ck(!s.on&&close(s.x,1)&&s.pixelX==1919&&s.pixelY==540);                       // past the edge: off, and kept at the edge
 ck(!spot(plate,{0,0,0},{0,0,1}).on);                                          // pointing away
 ck(!spot(plate,{0,0,0},{1,0,0}).on);                                          // along the plate
 ck(!spot(plate,{0,0,-2},{0,0,1}).on);                                         // from behind
 ck(!spot(plate,{NAN,0,0},{0,0,-1}).on);
 // A hand below and to the right, its ray going up and left to the plate's middle.
 s=spot(plate,{.3f,-.4f,-.2f},{-.3f,.4f,-1.3f});
 ck(s.on&&close(s.x,0)&&close(s.y,0));
 // A plate turned a quarter to the left of the viewer (its front faces +x): x on the plate runs along -z of the room.
 Plate side;side.pose={euler(pi/2,0,0),{-1.5f,0,0}};
 s=spot(side,{0,0,0},{-1,0,0});
 ck(s.on&&close(s.x,0)&&close(s.y,0));
 s=spot(side,{0,.1f,-.25f},{-1,0,0});
 ck(s.on&&close(s.x,.25f)&&close(s.y,.1f));
 // A small plate: the same metres are more pixels.
 Plate small;small.pose.position={0,0,-.5f};small.width=.4f;small.height=.225f;
 s=spot(small,{.1002f,0,0},{0,0,-1});
 ck(s.on&&s.pixelX==1440);
 Plate broken;broken.width=0;ck(!spot(broken,{0,0,1},{0,0,-1}).on);

 // The plate of an open menu: upright, 1.5 m ahead of where the head was, in the level direction it looked.
 {const auto ahead=[](const Pose& o){return rotate(o.orientation,{0,0,-1});};
  Pose head{euler(0,0,0),{.1f,1.6f,-.2f}};auto o=plateOrigin(head);
  ck(close(o.position.x,.1f)&&close(o.position.y,1.6f)&&close(o.position.z,-.2f)&&close(ahead(o).z,-1)&&close(ahead(o).x,0));
  head.orientation=euler(pi/2,-.26f,.1f);o=plateOrigin(head);                       // turned a quarter left, looking down a little, tilted
  ck(close(ahead(o).x,-1)&&close(ahead(o).y,0)&&close(ahead(o).z,0)&&close(rotate(o.orientation,{0,1,0}).y,1));
  head.orientation=euler(-pi/2,.5f,0);o=plateOrigin(head);                          // a quarter right, looking up
  ck(close(ahead(o).x,1)&&close(ahead(o).y,0));
  head.orientation=euler(pi/2,-pi/2,0);o=plateOrigin(head);                         // face straight down: the top of the head points ahead
  ck(close(ahead(o).x,-1)&&close(ahead(o).y,0)&&std::abs(ahead(o).z)<1e-3f);
  head.orientation=euler(pi/2,pi/2,0);o=plateOrigin(head);                          // face straight up
  ck(close(ahead(o).x,-1)&&close(ahead(o).y,0)&&std::abs(ahead(o).z)<1e-3f);
  // The pointer on that plate: the head has turned away since, the plate has not.
  head={euler(pi/2,-.26f,0),{0,1.65f,0}};o=plateOrigin(head);
  const auto fixed=fixedPlate(o,1.5f,2.f,1.125f,1920,1080);
  ck(close(fixed.pose.position.x,-1.5f)&&close(fixed.pose.position.y,1.65f)&&close(fixed.pose.position.z,0));
  auto at=spot(fixed,{0,1.65f,0},{-1,0,0});ck(at.on&&at.pixelX==960&&at.pixelY==540);
  at=spot(fixed,{-.3f,1.25f,-.25f},Vec3{-1.5f,1.65f+.3004f,.5003f}-Vec3{-.3f,1.25f,-.25f}); // a hand low on the right, pointing up and left
  ck(at.on&&close(at.x,-.5003f)&&close(at.y,.3004f)&&at.pixelX==479&&at.pixelY==251);
  ck(!spot(fixed,{0,1.65f,0},{1,0,0}).on);}

 // The slack of a click.
 ClickSlack slack;
 auto at=[&](float x,float y){return spot(plate,{x,y,0},{0,0,-1});};
 ck(slack.filter(at(.1003f,.1f),false).pixelX==at(.1003f,.1f).pixelX);         // no button: the cursor follows
 const auto pressed=at(.1003f,.1f);
 ck(pressed.pixelX==1056&&slack.filter(pressed,true).pixelX==1056);            // the press: where the ray is
 ck(slack.filter(at(.1043f,.1f),true).pixelX==1056);                           // 4 mm: held (under both limits)
 ck(slack.filter(at(.1103f,.1f),true).pixelX==1056);                           // 10 mm is only 9 pixels here: held
 ck(slack.filter(at(.1203f,.1f),true).pixelX==1075);                           // 20 mm, 19 pixels: loose
 ck(slack.filter(at(.1013f,.1f),true).pixelX==1057);                           // and stays loose until the release
 ck(slack.filter(at(.3f,.1f),false).pixelX==at(.3f,.1f).pixelX);
 ck(slack.filter(at(.3f,.1f),true).pixelX==at(.3f,.1f).pixelX&&!slack.loose);  // the next press anchors again
 ClickSlack fine;                                                              // on the small plate the metres decide
 auto on=[&](float x){return spot(small,{x,0,0},{0,0,-1});};
 const auto first=on(0);fine.filter(first,true);
 ck(fine.filter(on(.004f),true).pixelX==first.pixelX);                         // 4 mm is 19 pixels there: still held
 ck(fine.filter(on(.006f),true).pixelX==on(.006f).pixelX);                     // 6 mm: loose

 // The buttons.
 Buttons b;
 ck(!b.update(false,true,false).any()&&!b.takesTrigger());                     // no ray: the trigger is the game's own
 ck(!b.update(true,true,false).any()&&b.wait&&b.takesTrigger());               // the ray comes on with the trigger down: not a click
 ck(!b.update(true,true,false).any());
 ck(!b.update(true,false,false).any()&&!b.wait);                               // let go: from now on it counts
 auto c=b.update(true,true,false);ck(c.leftDown&&!c.leftUp&&b.left);
 ck(!b.update(true,true,false).any());                                         // held: told once
 c=b.update(true,false,false);ck(c.leftUp&&!b.left);
 c=b.update(true,false,true);ck(c.rightDown&&!c.leftDown);
 c=b.update(true,true,true);ck(c.leftDown&&!c.rightDown);                      // both at once are two buttons
 c=b.update(false,true,true);ck(!c.any()&&b.takesTrigger()&&b.takesSecondary()); // the ray goes with both down: still held
 c=b.update(false,false,true);ck(c.leftUp&&!c.rightUp&&!b.takesTrigger()&&b.takesSecondary());
 c=b.update(false,false,false);ck(c.rightUp&&!b.takesSecondary());             // each release reaches the game
 ck(!b.update(false,true,false).any());                                        // without the ray nothing starts
 ck(!b.update(true,true,false).any()&&b.wait);                                 // and that press does not count when the ray returns
 ck(!b.update(true,false,true).any()&&b.wait);                                 // nor does the other button until both are up
 ck(!b.update(true,false,false).any()&&!b.wait);
 ck(b.update(true,false,true).rightDown);
 std::cout<<"menu_pointer "<<n<<" checks PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<"menu_pointer FAIL "<<e.what()<<"\n";return 1;}}
