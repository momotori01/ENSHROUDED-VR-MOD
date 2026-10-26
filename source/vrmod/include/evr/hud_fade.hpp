#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>
namespace evr {
// HUD auto-hide for the headset (user, 2026-10-12):
//  - the HUD is hidden while health, mana and the other displayed things do not change and the quick slot is not used;
//  - once shown it hides again after five seconds without a change, by fading out;
//  - numbers that tick by themselves (the distance to the destination, the timer of "rested") are not a change;
//  - what the game pins to things in the world ("use", "pick up") stays visible always.
// The game draws its whole UI into one image. The pins are projected with the game's camera and land in the middle
// of that image (Mock runs game083144 / game073825: the same prompt at four places as the camera turned), while
// health, mana, buffs, compass, quest and action bar sit at the edges and corners. So the middle stays visible and
// everything outside it fades as one. A change is seen in a small copy of the image (cells of 16 pixels).
struct UiRect {
 int x0=0,y0=0,x1=0,y1=0;
 bool contains(int x,int y)const{return x>=x0&&x<x1&&y>=y0&&y<y1;}
};
// In pixels of a 1920 x 1080 UI (what the game draws for the headset session); scaled for another size.
struct HudLayout {
 // World-anchored prompts and markers: always visible, never a reason to show the HUD. A prompt whose object is
 // outside the game's view is held at the edge it left by (measured in the Mock runs game092706 / game083144: the
 // prompt of the thing at the feet at y 941..968 just above the action bar, at the top edge at y 87..139), so the
 // column reaches from below the compass to above the action bar, and a band reaches the left and right edges
 // between the HUD's corners. Measured HUD parts (game093213): health, mana and buffs x 17..358, y 26..209; compass
 // x 713..1207, y 8..60; quest x 1315.., y 18..160; action bar and its hints y 974..1080.
 // The band does not reach the right edge: recipes that the player has pinned are listed in the top right corner,
 // under the quest, and run down into the band's height (user after the headset run of 2026-10-13: "the lower part
 // of the pinned crafting materials and craftable counts at the top right does not disappear"). So the band ends at
 // x 1480; a prompt held at the right edge (its object is outside the view) now fades with the HUD.
 // Both end at y 880 since 2026-10-19 19:10 (user: "the fishing rod's text at the bottom does not go away"): the held
 // item's name and its hints ("Simple fishing rod", "X charge and cast", "LT aim") stand at y 895..975, above the action
 // bar - they were inside the column and stayed shown always. Below 880 is heldItem: it fades with the HUD.
 // From x 560 since 2026-10-26: the game's floating stamina ring is held off in VR (hooks/floating_stamina.hpp) and its stamina
 // is a third bar under health and mana, x 20..556, y 74..92 - from 420 the column kept its right end shown after the HUD faded.
 UiRect pins{560,84,1300,880};
 UiRect pinsWide{0,280,1480,880};
 // The held item's name and hints: fade with the HUD; what changes there (the hints of each fishing step) is no reason
 // to show it. A prompt held at the bottom edge for a thing at the feet (y 941..968) fades with it now.
 UiRect heldItem{420,880,1500,974};
 UiRect compass{660,0,1260,84};   // direction strip and the distance to the destination: never a change
 // The rest of the band's height at the right edge. It fades with the HUD (the pinned recipes), but what moves in it
 // is no reason to show the HUD: a prompt whose object is to the right of the view is held there and slides as the
 // head turns. Headset run game-20261013-141010: "looking around brings the UI up, often", "the UI does not go
 // away with time" - 136 of that run's 143 showings came from a few cells outside the health bars, most of them
 // exactly three, again and again seconds after the HUD had gone (where they were was not on record then; this
 // strip is the one place left where a held prompt counted. HudChange::other* now says where).
 UiRect edgeRight{1480,280,1920,900};
 // health and mana bars: the smallest change counts. 1.0 made the health bar longer (x 17..539; 2026-10-18 from the
 // headset's UI pictures): to 545 now, so that a change near a full bar counts as the vitals' too.
 UiRect vitals{0,8,545,86};
 // The buffs under the bars (headset run 2026-10-17, report 18: "the UI comes back at once on something"): a buff's
 // icon there changed every ten seconds (cells at x 176..208, y 80..112) and showed the HUD again each time - too seldom
 // for a ticker (6 changes in 12 s). Buffs are not among what shows the HUD (health, mana, what is shown to the player,
 // the quick slots): their changes are left out like the compass's.
 UiRect buffs{0,86,420,230};
 // The column that stays shown. In the Shroud the game puts the time that is left there, as a bar and a clock, in
 // the compass's place: while that is out the compass stays too (user after the headset run game-20261013-105615:
 // "while the Shroud's display is out, do not hide just the compass at the top"). The column is as wide as the
 // compass and more; above it, beside the compass, the game draws nothing.
 // The column is narrowed to the compass's left edge then (2026-10-18; headset report 05: "while the Shroud gauge is
 // out, a little of the health gauge shows too"): reaching up to the top from x 420 it took in the right end of 1.0's
 // longer health bar (x 420..539, y 26..58). Pins at x 420..660 above y 280 and below 900 fade with the HUD then.
 UiRect kept(bool withCompass)const{auto r=pins;if(withCompass){r.y0=std::min(r.y0,compass.y0);r.x0=std::max(r.x0,compass.x0);}return r;}
};
struct HudFadeSettings {
 uint64_t holdMs=5000;            // shown this long after the last change
 uint64_t fadeOutMs=800,fadeInMs=150;
 unsigned cellThreshold=4;        // of 255: mean difference of one channel of a cell that counts as changed
 unsigned minimumCells=3;         // outside the vitals: cells that have to change at once (one ticking digit is fewer)
 // In the vitals (2026-10-26, user: "the UI does not go away by itself" - headset run 20261026-180423: of the reasons the HUD stayed,
 // 41 were one cell of the bars, 6 two, at a second or two apart: the mana bar filling again; a hit or a spell spent moved 7 and more).
 // The bars' own regeneration is no reason to show the HUD; a loss or a gain of three cells at once is.
 unsigned minimumVitalCells=3;
 // A cell that keeps changing by itself (a timer's digits) stops counting: this many changes within the window.
 unsigned tickerChanges=6;uint64_t tickerWindowMs=12000,tickerMergeMs=300;
};
enum class HudMode:uint8_t {Off=0,Gameplay=1,Forced=2};   // Forced: menus, the quick wheel, building mode, unknown screens
// fadingCells: outside the vitals, cells whose content only went away (the game fading out its action bar, a
// highlight, a buff that ended). Something leaving the screen is no reason to show the HUD or to keep it.
// other*: the box around the cells counted in otherCells, in pixels of the 1920 x 1080 layout (all 0 without one).
struct HudChange {bool changed=false;unsigned vitalCells=0,otherCells=0,tickerCells=0,ignoredCells=0,fadingCells=0;int otherX0=0,otherY0=0,otherX1=0,otherY1=0;};
// Compares two small copies of the UI (RGBA8, w x h cells) and keeps what it knows about cells that tick.
class HudChangeDetector {
 HudLayout layout_{};HudFadeSettings settings_{};unsigned w_=0,h_=0;
 std::vector<uint8_t> zone_;                      // 0 other, 1 vitals, 2 ignored (pins, compass)
 struct Ticks {std::array<uint64_t,8> at{};unsigned next=0;};
 std::vector<Ticks> ticks_;std::vector<uint8_t> previous_;
 bool ticking(const Ticks& t,uint64_t now)const{
  unsigned recent=0;for(auto at:t.at)if(at&&now>=at&&now-at<=settings_.tickerWindowMs)++recent;
  return recent>=settings_.tickerChanges;
 }
public:
 HudChangeDetector()=default;HudChangeDetector(HudLayout layout,HudFadeSettings settings):layout_(layout),settings_(settings){}
 const HudLayout& layout()const{return layout_;}
 void reset(){previous_.clear();ticks_.clear();zone_.clear();w_=h_=0;}
 // now: milliseconds, monotonic. The first copy (or a new size) is no change.
 HudChange update(const uint8_t* cells,unsigned w,unsigned h,uint64_t now){
  HudChange out;if(!cells||!w||!h||w>512||h>512||settings_.tickerChanges<2||settings_.tickerChanges>8)throw std::invalid_argument("HUD change input");
  const size_t count=size_t(w)*h;
  if(w!=w_||h!=h_||previous_.size()!=count*4){
   w_=w;h_=h;zone_.assign(count,0);ticks_.assign(count,{});
   for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
    const int px=int((x*2+1)*1920/(w*2)),py=int((y*2+1)*1080/(h*2));
    zone_[size_t(y)*w+x]=layout_.pins.contains(px,py)||layout_.pinsWide.contains(px,py)||layout_.compass.contains(px,py)||layout_.edgeRight.contains(px,py)||layout_.buffs.contains(px,py)||layout_.heldItem.contains(px,py)?2:layout_.vitals.contains(px,py)?1:0;}
   previous_.assign(cells,cells+count*4);return out;
  }
  for(size_t i=0;i<count;++i){
   const uint8_t* a=cells+i*4;const uint8_t* b=previous_.data()+i*4;unsigned difference=0;
   for(unsigned c=0;c<4;++c)difference=std::max(difference,unsigned(std::abs(int(a[c])-int(b[c]))));
   if(difference<settings_.cellThreshold)continue;
   if(zone_[i]==2){++out.ignoredCells;continue;}
   if(zone_[i]==1){++out.vitalCells;continue;}
   // The UI image is premultiplied: content that fades out loses coverage and no channel gains.
   if(int(b[3])-int(a[3])>=int(settings_.cellThreshold)&&a[0]<=b[0]+1&&a[1]<=b[1]+1&&a[2]<=b[2]+1){++out.fadingCells;continue;}
   auto& t=ticks_[i];const auto last=t.at[(t.next+t.at.size()-1)%t.at.size()];
   if(!last||now<last||now-last>settings_.tickerMergeMs){t.at[t.next]=now;t.next=(t.next+1)%unsigned(t.at.size());}
   if(ticking(t,now))++out.tickerCells;
   else{
    const int x=int(i%w),y=int(i/w);const int x0=x*1920/int(w),y0=y*1080/int(h),x1=(x+1)*1920/int(w),y1=(y+1)*1080/int(h);
    if(!out.otherCells){out.otherX0=x0;out.otherY0=y0;out.otherX1=x1;out.otherY1=y1;}
    else{out.otherX0=std::min(out.otherX0,x0);out.otherY0=std::min(out.otherY0,y0);out.otherX1=std::max(out.otherX1,x1);out.otherY1=std::max(out.otherY1,y1);}
    ++out.otherCells;
   }
  }
  previous_.assign(cells,cells+count*4);
  out.changed=out.vitalCells>=settings_.minimumVitalCells||out.otherCells>=settings_.minimumCells;return out;
 }
};
// Opacity of the HUD outside the pins.
class HudFade {
 HudFadeSettings settings_{};uint64_t lastChange_=0,lastUpdate_=0;float opacity_=1;bool started_=false;
public:
 HudFade()=default;explicit HudFade(HudFadeSettings s):settings_(s){}
 float opacity()const{return opacity_;}bool hidden()const{return opacity_<=0;}
 float update(uint64_t now,HudMode mode,bool changed){
  if(mode==HudMode::Off){started_=false;opacity_=1;return 1;}
  if(!started_||now<lastUpdate_){started_=true;lastChange_=lastUpdate_=now;opacity_=1;}   // shown when it starts
  if(mode==HudMode::Forced||changed)lastChange_=now;
  if(mode==HudMode::Forced){opacity_=1;lastUpdate_=now;return 1;}
  const float elapsed=float(std::min<uint64_t>(now-lastUpdate_,250));lastUpdate_=now;
  if(now-lastChange_<=settings_.holdMs)opacity_=settings_.fadeInMs?std::min(1.f,opacity_+elapsed/float(settings_.fadeInMs)):1.f;
  else opacity_=settings_.fadeOutMs?std::max(0.f,opacity_-elapsed/float(settings_.fadeOutMs)):0.f;
  return opacity_;
 }
};
}
