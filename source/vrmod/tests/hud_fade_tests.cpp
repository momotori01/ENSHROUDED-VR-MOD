#include <evr/hud_fade.hpp>
#include <iostream>
#include <string>
using namespace evr;
int main(){try{unsigned n=0;auto ck=[&](bool b){++n;if(!b)throw std::runtime_error(std::to_string(n));};
 // The small copy of a 1920 x 1080 UI: 120 x 67 cells of 16 pixels.
 constexpr unsigned w=120,h=67;std::vector<uint8_t> image(size_t(w)*h*4,0);
 auto cell=[&](unsigned px,unsigned py)->uint8_t*{return image.data()+(size_t(py*h/1080)*w+px*w/1920)*4;};
 auto paint=[&](unsigned px,unsigned py,uint8_t value){auto* c=cell(px,py);c[0]=c[1]=c[2]=c[3]=value;};
 HudChangeDetector d;uint64_t now=1000;
 auto look=[&](uint64_t step=33){now+=step;return d.update(image.data(),w,h,now);};
 // The first copy is no change; an unchanged copy is none either.
 ck(!look().changed);ck(!look().changed);
 // Health or mana: one or two cells (the bar filling again) are no change; three at once (a hit, a spell) are (2026-10-26).
 paint(200,40,90);{const auto c=look();ck(!c.changed&&c.vitalCells==1&&c.otherCells==0);}ck(!look().changed);
 paint(216,40,90);paint(232,40,90);{const auto c=look();ck(!c.changed&&c.vitalCells==2);}
 paint(248,40,90);paint(264,40,90);paint(280,40,90);{const auto c=look();ck(c.changed&&c.vitalCells==3&&c.otherCells==0);}ck(!look().changed);
 // The compass with the distance to the destination, and the middle of the screen with the pins: never a change.
 for(unsigned x=700;x<1240;x+=16)paint(x,30,200);{const auto c=look();ck(!c.changed&&c.ignoredCells>=30&&c.otherCells==0);}
 for(unsigned x=900;x<1100;x+=16)paint(x,860,180);{const auto c=look();ck(!c.changed&&c.ignoredCells>=12);}
 // Elsewhere: one or two cells alone (a digit) are not a change, a new icon or line of text is.
 paint(1650,200,60);{const auto c=look();ck(!c.changed&&c.otherCells==1);}
 // The buffs under the bars are no reason to show it (headset run 2026-10-17: a buff's icon changing every ten seconds).
 for(unsigned x=100;x<164;x+=16)paint(x,150,90);{const auto c=look();ck(!c.changed&&c.ignoredCells>=4&&c.otherCells==0);}
 for(unsigned x=1560;x<1880;x+=16)paint(x,60,150);{const auto c=look();ck(c.changed&&c.otherCells>=3);}
 // Something that only goes away (the game fading out its action bar) is no change; in the bars it is (health lost).
 for(unsigned x=1560;x<1880;x+=16)paint(x,60,20);{const auto c=look();ck(!c.changed&&c.fadingCells>=18&&c.otherCells==0);}
 for(unsigned x=40;x<200;x+=16)paint(x,1040,160);ck(look().changed);for(unsigned x=40;x<200;x+=16)paint(x,1040,0);ck(!look().changed);
 // The band between the HUD's corners (a prompt held at the left or right edge) is part of the always shown middle.
 for(unsigned y=300;y<880;y+=16)paint(30,y,170);{const auto c=look();ck(!c.changed&&c.ignoredCells>=30&&c.otherCells==0);}
 paint(200,40,10);paint(216,40,10);paint(232,40,10);{const auto c=look();ck(c.changed&&c.vitalCells==3&&c.fadingCells==0);}
 // Content that changes without leaving (same coverage, another colour) is a change.
 for(unsigned x=1560;x<1880;x+=16){auto* c=cell(x,60);c[0]=5;c[1]=18;c[2]=20;c[3]=20;}ck(look().changed);
 // A difference below the threshold (blit rounding, compression of a gradient) is nothing.
 {auto* c=cell(1700,300);c[0]=3;}ck(look().otherCells==0);
 // A timer: four cells that change every second stop counting after six changes, and count again once they rest.
 {d.reset();now=50000;look();unsigned changes=0,tickers=0;
  for(unsigned second=1;second<=10;++second){for(unsigned x=1560;x<1624;x+=16)paint(x,200,uint8_t(40+second*17));
   const auto c=look(1000);changes+=c.changed;if(c.tickerCells)++tickers;ck(c.vitalCells==0);}
  ck(changes==5&&tickers==5);                                   // the first five ticks show it, after that it is a ticker
  for(unsigned x=1560;x<1624;x+=16)paint(x,200,250);ck(!look(1000).changed);
  // While that timer ticks, something new elsewhere still counts.
  for(unsigned x=40;x<104;x+=16)paint(x,1040,222);ck(look(1000).changed);
  // After the window without ticks the cells are ordinary again.
  now+=20000;for(unsigned x=1560;x<1624;x+=16)paint(x,200,255);{const auto c=look(1000);ck(c.changed&&c.tickerCells==0);}}
 // A changed size starts over without a change.
 {std::vector<uint8_t> small(60*34*4,9);ck(!d.update(small.data(),60,34,now+5).changed);}
 bool rejected=false;try{d.update(nullptr,w,h,now);}catch(const std::invalid_argument&){rejected=true;}ck(rejected);
 // The zones of the layout: prompt positions seen in the Mock runs are inside the pins, the HUD parts are outside.
 {const HudLayout l;auto shown=[&](int x,int y){return l.pins.contains(x,y)||l.pinsWide.contains(x,y);};
  ck(shown(1000,866)&&shown(1100,766)&&shown(1140,868)&&shown(900,120)&&shown(1000,95)&&shown(20,600)&&shown(1470,600));
  ck(!shown(500,88)&&!shown(540,90));   // the stamina bar's right end fades with the HUD (2026-10-26)
  ck(!shown(1000,955)&&!shown(960,910)&&l.heldItem.contains(1000,955)&&l.heldItem.contains(960,910)&&!l.heldItem.contains(960,980));   // the held item's name and hints fade (2026-10-19)
  ck(!shown(100,40)&&!shown(1700,90)&&!shown(960,1030)&&!shown(960,980)&&!shown(250,105)&&!shown(40,188)&&!shown(1700,140)&&!shown(200,1040)&&!shown(935,18));
  ck(!shown(1900,600)&&!shown(1500,300)&&!shown(1700,880));            // the top right column of pinned recipes fades with the HUD
  // ... and the band's height of it is no reason to show the HUD (a prompt held at the right edge), above it is one.
  ck(l.edgeRight.contains(1900,600)&&l.edgeRight.contains(1500,300)&&l.edgeRight.contains(1700,880)&&!l.edgeRight.contains(1700,200)&&!l.edgeRight.contains(1700,940)&&!l.edgeRight.contains(1470,600));
  {HudChangeDetector e;const unsigned ew=120,eh=67;std::vector<uint8_t> cells(ew*eh*4,0);uint64_t at=50000;e.update(cells.data(),ew,eh,at);
   auto mark=[&](unsigned px,unsigned py,unsigned n,uint8_t v){for(unsigned k=0;k<n;++k){const size_t i=(size_t(py/16)*ew+px/16+k)*4;cells[i]=cells[i+1]=cells[i+2]=cells[i+3]=v;}};
   mark(1824,592,5,200);{const auto c=e.update(cells.data(),ew,eh,at+=40);ck(!c.changed&&c.otherCells==0&&c.ignoredCells==5&&c.otherX1==0);}     // a prompt at the right edge
   mark(1824,192,5,200);{const auto c=e.update(cells.data(),ew,eh,at+=40);ck(c.changed&&c.otherCells==5&&c.otherX0==1824&&c.otherX1==1904&&c.otherY0<=201&&c.otherY1>201&&c.otherY1-c.otherY0<=17);}}   // the pinned recipes above it
  ck(l.vitals.contains(100,40)&&l.vitals.contains(200,62)&&!l.vitals.contains(40,108)&&l.compass.contains(935,18)&&l.compass.contains(760,48));
  ck(l.buffs.contains(192,96)&&l.buffs.contains(40,188)&&!l.buffs.contains(200,62)&&!l.buffs.contains(1650,200));
  // In the Shroud the compass - with the time that is left - stays with the pins; health, quest and action bar do not.
  const auto plain=l.kept(false),shroud=l.kept(true);
  ck(plain.x0==l.pins.x0&&plain.y0==l.pins.y0&&plain.x1==l.pins.x1&&plain.y1==l.pins.y1&&!plain.contains(935,18));
  ck(shroud.contains(935,18)&&shroud.contains(760,48)&&shroud.contains(1200,60)&&shroud.contains(700,500)&&shroud.y1==l.pins.y1);
  ck(!shroud.contains(100,40)&&!shroud.contains(1400,60)&&!shroud.contains(960,1000));
  // 1.0's longer health bar (x 17..539): its right end counts as the vitals and does not stay in the Shroud (report 05, 2026-10-18).
  ck(l.vitals.contains(500,40)&&!shroud.contains(480,40)&&!shroud.contains(530,30));}

 // Fade: shown at the start, five seconds after the last change it fades out (0.8 s), a change brings it back at once.
 {HudFade f;uint64_t t=10000;auto run=[&](uint64_t ms,HudMode mode,bool changed=false){float o=0;for(uint64_t i=0;i<ms;i+=20){t+=20;o=f.update(t,mode,changed);}return o;};
  ck(f.update(t,HudMode::Gameplay,false)==1);ck(run(4900,HudMode::Gameplay)==1);
  {const float o=run(500,HudMode::Gameplay);ck(o>0.3f&&o<0.7f);}ck(run(600,HudMode::Gameplay)==0&&f.hidden());
  ck(run(60000,HudMode::Gameplay)==0);
  {const float o=run(60,HudMode::Gameplay,true);ck(o>0.3f&&o<0.5f);}ck(run(120,HudMode::Gameplay)==1);   // fades in within 0.15 s
  ck(run(4800,HudMode::Gameplay)==1);ck(run(1200,HudMode::Gameplay)==0);
  // A menu, the quick wheel or building mode shows everything; five seconds after leaving it the HUD goes again.
  ck(run(20,HudMode::Forced)==1);ck(run(30000,HudMode::Forced)==1);ck(run(4900,HudMode::Gameplay)==1);ck(run(1200,HudMode::Gameplay)==0);
  // Off: always fully shown, and it starts over afterwards.
  ck(run(20,HudMode::Off)==1);ck(run(20,HudMode::Gameplay)==1);ck(run(4000,HudMode::Gameplay)==1);
  // A long gap between two looks (a loading stall) does not jump: at most a quarter second of fading per look.
  run(1100,HudMode::Gameplay);t+=5000;{const float o=f.update(t,HudMode::Gameplay,false);ck(o>=0&&o<1);}
  // The clock going backwards starts over, shown.
  ck(f.update(5,HudMode::Gameplay,false)==1);}
 std::cout<<n<<" PASS\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
