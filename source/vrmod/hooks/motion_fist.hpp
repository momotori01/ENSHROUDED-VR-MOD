// Bare hands (VHVR FistCollision, docs/analysis/vhvr_spec_20261010/equipment_tools.md section 8): with nothing in the
// selected slot each tracked hand is a fist. A fist has no model to show; this only describes it for the stroke
// (live_melee.hpp: the hand's own speed, 0.45 of the swing requirement, the grip held) and for the target gate.
// The attack itself, its damage and its hit volume are the game's (_Weapon_Unarmed_Fists_Hand_lvl00).
namespace motionFist {
inline bool requested=false,ready=false;inline uint64_t frame=0,readyMs=0,updates=0;
inline motion::LiveMeleeWeapon right,left;
constexpr float knuckles=.08f;   // metres from the grip to the knuckles, along the fist's direction
inline void prepare(const game::StableAnchor& anchor){
    ready=false;right={};left={};
    if(!requested||!controllerUiFixture::handItem||!controllerUiFixture::liveMeleeRequested)return;
    const auto now=GetTickCount64();const auto held=controllerUiFixture::handItem(now);
    if(!held.valid||held.item)return;                       // something is selected: not bare hands
    if(!motionHandVisual::ready||!shownHands.focused||anchor.worldUnitsPerMeter!=1||!rootCamera::firstPerson(rootCamera::snapshot()))return;
    const auto facing=normalized(anchor.orientation);
    const auto origin=cameraPosition(anchor.root,rotate(facing,anchor.cameraOffsetGameUnits),1);
    for(unsigned h=0;h<2;++h){
        const auto& hand=shownHands.hands[h];if(!hand.tracked()||!finite(hand.grip.position))continue;
        const auto tracking=game::trackingPose(compose(liveTrackingToAnchor,hand.grip));
        const auto position=cameraPosition(origin,rotate(facing,tracking.position),1);
        const auto rotation=normalized(multiply(facing,tracking.orientation));
        const auto tip=cameraPosition(position,rotate(rotation,Vec3{0,knuckles,0}),1);
        auto& w=h?right:left;
        w.binding=(uint64_t(motion::unarmedFistsItem)<<32)|(h+1);w.hand=h;w.fist=true;w.speedFactor=motion::fistSpeedFactor;
        w.forward=rotate(hand.grip.orientation,{0,1,0});   // the knuckles: the grip's forward (weapon_mount.hpp)
        w.bladeFrom={position.x,position.y,position.z};w.bladeTo={tip.x,tip.y,tip.z};w.blade=true;w.needsTarget=true;
    }
    if(!right.binding&&!left.binding)return;
    if(!updates++||frame+1!=liveFrame.frame)logFile->event("motion_fist_state","\"frame\":"+std::to_string(liveFrame.frame)+",\"bare_hands\":true,\"right\":"+(right.binding?"true":"false")+",\"left\":"+(left.binding?"true":"false")+
        ",\"item\":"+std::to_string(motion::unarmedFistsItem)+",\"speed_factor\":"+std::to_string(motion::fistSpeedFactor));
    ready=true;frame=liveFrame.frame;readyMs=now;
}
// The attack of this item belongs to the bare hands shown now (motion_event_selection_observer.hpp).
inline bool shown(uint32_t item,uint64_t now){return requested&&item==motion::unarmedFistsItem&&readyMs&&now>=readyMs&&now-readyMs<=200;}
}
