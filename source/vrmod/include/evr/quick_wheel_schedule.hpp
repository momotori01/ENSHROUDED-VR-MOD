#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace evr::motion {
// Finite native quick-wheel diagnostic schedules. `frame` is the mock hand CSV
// row seen by the same-frame gesture sample. The native overlay must stay open
// for every frame in [open, close) and must be observed closed at or after
// `close` before the next cycle. Value-only: it never writes game state and
// does not describe the product grip/long-press router.
struct QuickWheelCycle {uint64_t open=0,close=0;};
struct QuickWheelSchedule {
 const char* token;
 std::array<QuickWheelCycle,5> cycles;
 size_t count;
 // [0],[1]: captures while the overlay is open; [2]: after the final close.
 std::array<uint64_t,3> captures;
 uint64_t closeGrace,end;
};
// Single open/close: up/right/down/left/up, release with stick held (2026-10-05 wheel trial).
inline constexpr QuickWheelSchedule quickWheelSingle{"EVR_QUICK_WHEEL_1",{{{500,720}}},1,{520,620,780},0,800};
// Three selections: up-right / up / up-right, each released with the stick held.
inline constexpr QuickWheelSchedule quickWheelRebind{"EVR_QUICK_WHEEL_REBIND_1",{{{480,530},{610,660},{740,790}}},3,{515,775,850},30,860};
// Five weapon/tool selections (bow, torch, staff, mace, axe) at the visual slot centres;
// consumables are never selected because the wheel uses them on selection (game170323).
inline constexpr QuickWheelSchedule quickWheelSurvey{"EVR_QUICK_WHEEL_SURVEY_1",{{{465,510},{545,590},{625,670},{705,750},{785,830}}},5,{500,740,870},15,880};
// Two selections: down (consumable) then up-right (weapon). Observation only: the
// fixture never sends X/MainhandAction, so selecting the consumable must not use it.
inline constexpr QuickWheelSchedule quickWheelConsumable{"EVR_QUICK_WHEEL_CONSUMABLE_1",{{{480,530},{610,660}}},2,{515,645,720},30,730};
// game165733 showed straight down is outside the upper half-ring and keeps the prior
// selection; the dried berry sits at ~91 degrees, so v2 selects it with the stick right.
inline constexpr QuickWheelSchedule quickWheelConsumableRight{"EVR_QUICK_WHEEL_CONSUMABLE_2",{{{480,530},{610,660}}},2,{515,645,720},30,730};
// Grip-router driven wheel: RB comes only from GripRouter (right long press outside the
// body zones). Long presses at 450 and 730 (+300 ms), a short press and a zone press between.
inline constexpr QuickWheelSchedule quickWheelRouter{"EVR_GRIP_ROUTER_WHEEL_1",{{{490,550},{770,820}}},2,{520,800,860},15,880};
// Router equip requests executed through WheelSelect: right hip (staff) at 460, left shoulder
// (bow) at 550, right shoulder (main weapon) at 650; each opens ~2 frames later for ~330 ms.
inline constexpr QuickWheelSchedule quickWheelEquip{"EVR_GRIP_ROUTER_EQUIP_1",{{{470,490},{560,580},{660,680}}},3,{480,670,740},15,760};
// Same press frames as EQUIP_1, but the shoulder presses happen with the hand resting at its
// temple (gesture already active) inside the head-relative shoulder reach (user spec 18:50).
inline constexpr QuickWheelSchedule quickWheelEquipOverlap{"EVR_GRIP_ROUTER_EQUIP_2",{{{470,490},{560,580},{660,680}}},3,{480,670,740},15,760};
inline const QuickWheelSchedule* quickWheelSchedule(const char* token){
 auto same=[](const char* a,const char* b){while(*a&&*a==*b){++a;++b;}return *a==*b;};
 for(const auto* s:{&quickWheelSingle,&quickWheelRebind,&quickWheelConsumable,&quickWheelConsumableRight,&quickWheelSurvey,&quickWheelRouter,&quickWheelEquip,&quickWheelEquipOverlap})if(same(s->token,token))return s;
 return nullptr;
}
class QuickWheelProgress {
public:
 enum class Verdict {Continue,Complete,Fail};
 struct Step {Verdict verdict=Verdict::Continue;unsigned captureTag=0;const char* error=nullptr;};
 explicit QuickWheelProgress(const QuickWheelSchedule& s=quickWheelSingle):schedule(&s){}
 // `released` means the frontend output is fully neutral (no RB, no right stick).
 Step update(uint64_t frame,bool overlay,bool released){
  Step out;
  if(finished)return fail(out,"wheel progress used after completion");
  // The frontend may poll the same XR frame several times; time never runs backward.
  if(seen&&frame<lastFrame)return fail(out,"wheel frames decreased");
  seen=true;lastFrame=frame;
  if(cycle<schedule->count){
   const auto& c=schedule->cycles[cycle];
   if(frame>=c.open&&frame<c.close&&!overlay)return fail(out,"native quick wheel did not stay open");
   openedThis|=overlay;
   if(frame>=c.close&&!overlay&&openedThis){++cycle;openedThis=false;}
   else if(schedule->closeGrace&&frame>=c.close+schedule->closeGrace&&overlay)return fail(out,"wheel did not close/release");
  }else if(overlay)return fail(out,"wheel reopened after the final cycle");
  const bool closed=cycle==schedule->count;
  if(overlay&&frame>=schedule->captures[0]&&capture==0){capture=1;out.captureTag=1;}
  else if(overlay&&frame>=schedule->captures[1]&&capture==1){capture=2;out.captureTag=3;}
  else if(closed&&frame>=schedule->captures[2]&&capture==2){capture=3;out.captureTag=2;}
  if(frame>=schedule->end){
   if(!closed||overlay||!released||capture!=3)return fail(out,"wheel did not close/release");
   finished=true;out.verdict=Verdict::Complete;
  }
  return out;
 }
 size_t closedCycles()const{return cycle;}
 unsigned captures()const{return capture;}
 const QuickWheelSchedule& plan()const{return *schedule;}
private:
 Step fail(Step out,const char* error){finished=true;out.verdict=Verdict::Fail;out.error=error;out.captureTag=0;return out;}
 const QuickWheelSchedule* schedule;
 size_t cycle=0;unsigned capture=0;uint64_t lastFrame=0;bool seen=false,openedThis=false,finished=false;
};
}
