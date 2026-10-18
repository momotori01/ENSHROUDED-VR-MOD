// Read-only attachment ownership from completed native ECS iterations.
// No component pointers escape callbacks. This stage does not change visibility.
namespace attachmentObserver {
inline bool requested=false,allowCandidateScene=false;
// Live held items (world-live-held-items.txt): tools are shown in the tracked hand as weapons are. They are the
// items of the equipment slots Tool (2: felling axes, fishing rods) and BuildTool (3: pickaxes, rakes, the building
// and summoning tools, the watering can) that are not weapons; docs/analysis/weapon_mount_20261010/held-models.
// Headset run game180455: the felling axe (slot 2) did not appear while the pickaxe (slot 3) did.
inline bool liveTools=false;
inline constexpr bool toolItemSlot(uint8_t itemSlot){return itemSlot==2||itemSlot==3;}
// Swung like a weapon (VHVR: Axe and Pickaxe take the weapon route): the long tools the game attaches to the left
// hand slot. The tools of the right hand slot (building, summoning, watering, fishing) are used with the button.
inline constexpr bool swungTool(uint8_t itemSlot,uint8_t attachmentSlot){return toolItemSlot(itemSlot)&&attachmentSlot==9;}
inline std::atomic<unsigned> failures{0},observations{0};
// The item registry of the running game, as the equipment query hands it over (for item metadata elsewhere).
inline std::atomic<uintptr_t> itemRegistry{0};
// Opt-in experiment (world-live-held-items.txt, word "follow"): the game's own entity of the item shown in the tracked
// hand follows that hand (hand slots 8 and 9; the bow is left out), so that what the game hangs on the entity (its light, its effects, the flash of a
// cast) appears at the tracked item. VHVR has this by parenting the real item to the hand. The attachment system
// writes the child's world transform from the animated hand slot and the SlotAttachment offset (2b2730: slot matrix
// of the parent's AnimationGraphSlotComponent, d7bb10, then 87c570); right after it, in the same pass over the same
// entity, position and orientation are replaced by the tracked item's (published by the display, one frame old).
// The SlotAttachment itself, the hand slots and the impact side stay as the game has them.
inline bool followRequested=false;
struct Follow {uint32_t entity=0,parent=0;uintptr_t world=0;std::array<uint8_t,40> transform{};uint64_t at=0,frame=0;
    WorldPositionQ32 anchorRoot{};double anchorTurn=0,anchorTurnStep=0;uint64_t anchorSample=0;};   // the camera's anchor the pose was made from (0: none)
inline Follow follow;inline std::mutex followMutex;
// The tracked pose that is written here was made when the last frame was drawn; the game draws the next frame with the
// player a step further. The item's own entity - what its light and its flame hang on - therefore trailed the shown
// item by one frame of walking: 0.10 m at 4.3 m/s (Mock run game154431; headset run game145015: "the light of the
// torch and of the wand is in the right place but follows a good deal late when I move").
// The display places what it draws from the camera's anchor of that frame (motion_visual_bridge.hpp prepare): the
// player's root interpolated between two simulation ticks (root_interpolation.hpp stableRoot) and the heading of the
// stick's turns. The pose is published with that anchor (stampAnchor), and here it is moved with the anchor to the
// camera's newest sample - the one the next frame is drawn from: by what the interpolated root has moved, and turned
// about it by what the stick's turn has grown. The hand's own movement of one frame is not predicted.
// Until 2026-10-16 evening the root's CurrentTransform (the newest tick) and the turn seen at the last write were used;
// the ticks do not move with the drawn frames, so the light and the flame jumped ahead and fell behind by turns while
// walking (Mock game-20261016-204336, held_follow_state each frame: native - tracked +4, -2, -4, +39, 0, -4 mm ...; the
// user after the noon headset run: "the staff's light followed shaking, as if left behind by the movement; if it is
// moved by a correction, attach it directly - simpler has fewer bugs").
inline std::atomic<unsigned> followRebased{0},followTurned{0},followUnanchored{0};inline std::atomic<int64_t> followRebasedMicrometres{0};
constexpr double followTurnSense=-1;   // the tracked space turns against the sum of the stick's turns (headset run game-20261013-005905)
// The id of a component type in a World's own registry (256 byte records, name at +16, size at +56: the layout
// live_melee_targets.hpp reads), found by its name once for each World. -1: not there.
inline int componentIdByName(uintptr_t world,const char* wanted,uint16_t bytes){
    static std::mutex guard;static std::map<std::pair<uintptr_t,std::string>,int> ids;std::lock_guard<std::mutex> lock(guard);
    const auto key=std::make_pair(world,std::string(wanted));const auto known=ids.find(key);if(known!=ids.end())return known->second;
    int found=-1;
    try{for(unsigned id=0;id<nativeImpactComponents::componentCapacity&&found<0;++id){
            const auto t=nativeImpactComponents::componentType(world,id);if(t.bytes==bytes&&t.qualifiedName==wanted)found=int(id);}}
    catch(const std::exception&){}
    ids[key]=found;
    try{logFile->event("held_follow_component_id","\"world\":"+hex(world)+",\"name\":"+jsonString(wanted)+",\"id\":"+std::to_string(found));}catch(...){}
    return found;
}
// What the camera will take as its anchor for the next frame, worked out at the attachment: the same interpolation
// (stableRoot) of the player's RenderTransform history (the camera's query slot 2, 176 bytes) at the history's newest
// tick and the fraction of this task (state +0x88, the camera's layout). Mock game-20261016-211648 (trace each frame):
// the fraction is the camera's on every frame, and the camera's tick is the history's newest on every frame - the
// attachment task's own tick table holds an old value (827 throughout), which put the light a frame of walking off
// whenever two ticks came in one frame (7 frames of 57). The trace sets the result against the anchor the next frame
// is drawn with (held_follow_state predicted_minus_anchor).
inline std::mutex predictedMutex;inline WorldPositionQ32 lastPredictedRoot{};inline double lastPredictedTurn=0;inline bool lastPredictedValid=false;
inline uint32_t lastPredictedTick=0,lastPredictedSourceTick=0,lastPredictedNewestTick=0;inline float lastPredictedFraction=0;
// The stick's turn of the frame drawn next is not known yet at the attachment (Mock game-20261016-212721: the sum of the
// turns there is still the last frame's on every write; the drawn heading changes by -1 x that sum's growth, the
// interpolated root does not turn). It is taken to go on as in the last frame (anchorTurnStep) - a whole turn at once
// (a snap, or a hitch: over 0.25 rad) is not carried on.
inline void stampAnchor(Follow& f,const WorldPositionQ32& root){
    f.anchorRoot=root;
    static std::mutex m;static uint64_t previousSample=0;static double previousTurn=0,step=0;
    try{rootCamera::Sample s;if(rootCamera::trySnapshot(s)&&s.stable.position.x==root.x&&s.stable.position.y==root.y&&s.stable.position.z==root.z){
        std::lock_guard<std::mutex> lock(m);
        if(s.number!=previousSample){step=s.number==previousSample+1?s.controls.gameplayTurn-previousTurn:0;previousSample=s.number;previousTurn=s.controls.gameplayTurn;}
        f.anchorTurn=s.controls.gameplayTurn;f.anchorTurnStep=std::abs(step)<.25?step:0;f.anchorSample=s.number;}}catch(...){}
}
inline std::atomic<uint64_t> followWrites{0},followStale{0},followRejected{0};
// Diagnostic (2026-10-16 evening, user after the noon headset run: "the light of the staff followed as if left behind
// by the movement, shaking; if it is moved by a correction, attach it directly"): what the follow wrote last, to set
// against where the game's own model is drawn (motion_visual_bridge.hpp held_follow_state). EVR_HELD_FOLLOW_TRACE=1 in
// the game's environment: that line every frame instead of every second, and the item entity's component types once.
inline std::mutex writtenMutex;inline WorldPositionQ32 lastWritten{};inline uint64_t lastWrittenAt=0,lastWrittenCount=0;
inline const bool followTrace=[]{char v[8]{};const auto n=GetEnvironmentVariableA("EVR_HELD_FOLLOW_TRACE",v,sizeof v);return n==1&&v[0]=='1';}();
inline void traceEntityTypes(uintptr_t world,uint32_t entity){
    static std::mutex m;static std::vector<uint32_t> done;{std::lock_guard<std::mutex> lock(m);if(done.size()>4||std::find(done.begin(),done.end(),entity)!=done.end())return;done.push_back(entity);}
    try{const auto e=nativeImpactComponents::entityAtWorld(world,entity);const auto bits=read<std::array<uint64_t,10>>(e.layout);std::string out;unsigned count=0;
        for(unsigned i=0;i<nativeImpactComponents::componentCapacity;++i){if(!(bits[i/64]&(1ull<<(i%64))))continue;
            std::string name="?";try{name=nativeImpactComponents::componentType(world,i).qualifiedName;}catch(...){}
            out+=std::string(count++?",":"")+"["+std::to_string(i)+","+jsonString(name)+","+std::to_string(read<uint16_t>(e.layout+0xa84+i*2))+"]";}
        logFile->event("held_follow_item_types","\"entity\":"+std::to_string(entity)+",\"types\":["+out+"]");
    }catch(const std::exception& ex){try{logFile->event("held_follow_item_types","\"entity\":"+std::to_string(entity)+",\"error\":"+jsonString(ex.what()));}catch(...){}}
}
inline void publishFollow(const Follow& f){std::lock_guard<std::mutex> lock(followMutex);follow=f;}
inline Follow followLeft;inline void publishFollowLeft(const Follow& f){std::lock_guard<std::mutex> lock(followMutex);followLeft=f;}
// Other players' held items (2026-10-15, online 2.1): the attachment of a hand-slot item (8 the main hand, 9 the off
// hand, 12 the shield) of a character driven by another player's body (remote_body.hpp) is replaced, as the own one
// above, by where that player's datagram puts the item: its hand socket and the item's pose in that hand (online_
// sync.hpp publishes one of these per slot and frame). The game's own attachment runs first and stays as it is
// otherwise; nothing else of the character is written. selfItems (EVR_SYNC_SELF_ITEMS, on in the self test): the
// own character's items go this way too, after the own follow, and the two are compared on record.
// The own hand items as shown this frame: the world transform (position Q32, orientation: 40 bytes) of the main
// hand's copy (1: the weapon, tool or bow; motion_visual_bridge.hpp) and of the off hand's (0: the pair's left part,
// motion_offhand_visual.hpp; the shield, motion_shield_visual.hpp), with the render serial and frame they were made
// for. The body's datagram carries them relative to the hand sockets (motion_body_ik.hpp).
struct ShownItem {uint64_t serial=0,frame=0;std::array<uint8_t,40> transform{};};inline std::array<ShownItem,2> shownItem{};
struct RemoteItem {uint32_t parent=0;uint8_t slot=0;uint64_t at=0,frame=0;std::array<uint8_t,40> transform{};};
inline std::array<RemoteItem,24> remoteItems{};inline std::mutex remoteItemMutex;inline bool selfItems=false;
inline std::atomic<uint64_t> remoteItemWrites{0},remoteItemRejected{0},remoteItemStale{0};
inline void publishRemoteItem(const RemoteItem& r){
    std::lock_guard<std::mutex> lock(remoteItemMutex);size_t slot=0;
    for(size_t i=0;i<remoteItems.size();++i){if(remoteItems[i].parent==r.parent&&remoteItems[i].slot==r.slot){slot=i;break;}if(remoteItems[i].at<remoteItems[slot].at)slot=i;}
    remoteItems[slot]=r;
}
inline bool remoteItem(uint32_t parent,uint8_t slot,uint64_t now,RemoteItem& out){
    std::lock_guard<std::mutex> lock(remoteItemMutex);
    for(const auto& r:remoteItems)if(r.parent==parent&&r.slot==slot&&r.at){if(now<r.at||now-r.at>250){remoteItemStale.fetch_add(1);return false;}out=r;return true;}
    return false;
}
inline void remoteItemFollow(uintptr_t sp,const std::array<uint8_t,48>& a,uint32_t parent,uint64_t now,const std::array<uint8_t,40>* own){
    if(a[40]!=0||!(a[41]==8||a[41]==9||a[41]==12))return;
    RemoteItem r;if(!remoteItem(parent,a[41],now,r))return;
    try{
        const auto transform=read<uintptr_t>(sp+0x50+8*8);   /* 1.0: RenderTransform slot 8 (June 4) */const auto native=read<std::array<uint8_t,56>>(transform);
        WorldPositionQ32 from,to;Quat q;Vec3 scale;std::memcpy(&from,native.data(),24);std::memcpy(&q,r.transform.data()+24,16);std::memcpy(&scale,native.data()+40,12);std::memcpy(&to,r.transform.data(),24);
        const double dx=relativeQ32(to.x,from.x),dy=relativeQ32(to.y,from.y),dz=relativeQ32(to.z,from.z),metres=std::sqrt(dx*dx+dy*dy+dz*dz);
        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
        // Within arm's reach of the animated hand; anything else is not this item or not this character.
        if(!std::isfinite(norm)||std::abs(norm-1)>.02f||!finite(scale)||scale.x<=0||scale.x>4||!(metres<3.)){
            if(remoteItemRejected.fetch_add(1)<4)logFile->event("remote_item_follow_rejected","\"parent\":"+std::to_string(parent)+",\"slot\":"+std::to_string(a[41])+",\"metres\":"+std::to_string(metres));
            return;}
        // The self test: how far this stands from the own display's item of the same moment (the way back through the
        // datagram and the receiver's stepping, then about the character's root again), the worst so far on record.
        double fromOwn=-1,fromShown=-1,shownDegrees=-1;static double worstShown=0;
        if(own){WorldPositionQ32 o;std::memcpy(&o,own->data(),24);const double ox=relativeQ32(to.x,o.x),oy=relativeQ32(to.y,o.y),oz=relativeQ32(to.z,o.z);fromOwn=std::sqrt(ox*ox+oy*oy+oz*oz);}
        if(selfItems){const auto& s=shownItem[a[41]==8?1:0];
            if(s.frame){WorldPositionQ32 o;Quat so;std::memcpy(&o,s.transform.data(),24);std::memcpy(&so,s.transform.data()+24,16);
                const double ox=relativeQ32(to.x,o.x),oy=relativeQ32(to.y,o.y),oz=relativeQ32(to.z,o.z);fromShown=std::sqrt(ox*ox+oy*oy+oz*oz);worstShown=std::max(worstShown,fromShown);
                const auto dq=multiply(inverse(normalized(so)),normalized(q));shownDegrees=2*std::atan2(std::sqrt(double(dq.x*dq.x+dq.y*dq.y+dq.z*dq.z)),std::abs(double(dq.w)))*57.29578;}}
        std::memcpy(reinterpret_cast<void*>(transform),r.transform.data(),40);
        const auto n=remoteItemWrites.fetch_add(1);
        if(n<6||n%300==0)logFile->event("remote_item_follow_write","\"parent\":"+std::to_string(parent)+",\"slot\":"+std::to_string(a[41])+",\"writes\":"+std::to_string(n+1)+",\"metres_from_animated\":"+std::to_string(metres)+
            ",\"metres_from_own_follow\":"+std::to_string(fromOwn)+",\"metres_from_shown\":"+std::to_string(fromShown)+",\"degrees_from_shown\":"+std::to_string(shownDegrees)+",\"worst_from_shown_m\":"+std::to_string(worstShown)+
            ",\"pose_age_ms\":"+std::to_string(now-r.at)+",\"pose_frame\":"+std::to_string(r.frame)+",\"self_test\":"+(selfItems?"true":"false")+",\"slot_attachment_unchanged\":true");
    }catch(...){}
}
inline std::mutex mutex;
struct Child {std::array<uint32_t,4> modelGuid{};uintptr_t world=0,modelScene=0,modelView=0;std::array<uint8_t,48> slotBytes{};uint32_t entity=0,parent=0,handle=0,item=0;uint16_t model=0;uint8_t itemSlot=0,category=0,target=255,slot=255;uint64_t seen=0,itemSeen=0,slotSeen=0,modelSeen=0,itemChangedAt=0;bool hideObserved=false,slotObserved=false;uint8_t weaponType=255;uint32_t weaponHash=0;bool weaponCategoryKnown=false;};
inline std::array<Child,64> children{};
// Two fresh children in one place. The game makes an item's visual anew at times without the item changing - on
// entering and on leaving the Shroud, for one (headset run game-20261013-141010: weapon and shield got new entities
// at 1:10.8, the moment the Shroud's timer went) - and the entity it dropped stays "fresh" here for half a second.
// For that half second there were two candidates, nothing was shown from the hand and the game's own weapon and
// shield stood in for it at the game's hand: "something showed double and flickered at the gloves", 33 times in that
// run. The game asks about a living child every tick; one that has not been asked about for three ticks while the
// other has is the one that went. Both asked about alike: no answer, as before.
constexpr uint64_t replacedAfterMs=90;
inline std::atomic<unsigned> replacedResolved{0};
inline const Child* survivor(const Child& a,const Child& b){
    const auto la=std::max(a.slotSeen,a.modelSeen),lb=std::max(b.slotSeen,b.modelSeen);
    const Child* kept=la>=lb+replacedAfterMs?&a:lb>=la+replacedAfterMs?&b:nullptr;
    if(kept){const auto n=++replacedResolved;const auto& gone=kept==&a?b:a;
        // (one line per replacement would need its end; the first of each run of frames is enough to see them)
        static uint32_t lastGone=0;
        if(gone.entity!=lastGone&&n<4000){lastGone=gone.entity;try{logFile->event("attachment_replaced","\"kept_entity\":"+std::to_string(kept->entity)+",\"gone_entity\":"+std::to_string(gone.entity)+
            ",\"item\":"+std::to_string(kept->item)+",\"gone_item\":"+std::to_string(gone.item)+",\"slot\":"+std::to_string(kept->slot)+",\"gone_unseen_for_ms\":"+std::to_string(std::max(kept->slotSeen,kept->modelSeen)-std::max(gone.slotSeen,gone.modelSeen))+",\"read_only\":true");}catch(...){}}}
    return kept;
}
// Same-frame consumers revalidate the returned value against Scene resources.
inline Child shieldAppearance(uint32_t local,uintptr_t scene,uintptr_t view,uintptr_t world,uint64_t now){
    std::lock_guard<std::mutex> lock(mutex);Child out;
    for(const auto& c:children){auto fresh=[&](uint64_t t){return pauseClock::fresh(t,now,500);};
        if(c.parent!=local||c.itemSlot!=18||!c.item||!c.handle||!c.slotObserved||c.target!=0||
           !fresh(c.itemSeen)||!fresh(c.slotSeen)||!fresh(c.modelSeen)||c.slotSeen<c.itemChangedAt||c.modelSeen<c.itemChangedAt||
           c.modelScene!=scene||c.modelView!=view||(world&&c.world!=world))continue;
        if(out.entity){const auto* kept=survivor(out,c);if(!kept)return {};if(kept==&c)out=c;continue;}
        out=c;
    }
    return out;
}
// A weapon held as a pair (dual_wield_catalog.hpp, every dagger pair of the game data): the equipped item has no model
// of its own; its two parts are children of the player on the hand slots 8 and 9 that carry no item (game052057:
// item entity 7428, parts 7430 on slot 8 and 7431 on slot 9, model 1229). Both parts, fresh, with the item's model.
// The returned parts carry the pair's item, slot, category and weapon type.
struct DualWield {bool valid=false;Child right,left;};
inline DualWield dualWieldLocked(uint32_t local,uint64_t now){
    auto fresh=[&](uint64_t t){return pauseClock::fresh(t,now,500);};
    const Child* item=nullptr;
    for(const auto& c:children)if(c.parent==local&&c.item&&c.category==2&&c.itemSlot==1&&fresh(c.itemSeen)&&evr::motion::catalogDualWieldItem(c.item)){if(item)return {};item=&c;}
    if(!item)return {};
    const auto* asset=evr::motion::catalogDualWieldItem(item->item);const Child* right=nullptr;const Child* left=nullptr;
    for(const auto& c:children){
        if(c.parent!=local||c.item||!c.handle||!c.slotObserved||c.target!=0||!fresh(c.slotSeen)||!fresh(c.modelSeen)||c.modelGuid!=asset->guid)continue;
        if(c.slot==8){if(right)return {};right=&c;}else if(c.slot==9){if(left)return {};left=&c;}
    }
    if(!right||!left)return {};
    DualWield out;out.valid=true;out.right=*right;out.left=*left;
    for(auto* part:{&out.right,&out.left}){part->item=item->item;part->itemSlot=item->itemSlot;part->category=item->category;part->weaponType=item->weaponType;
        part->weaponHash=item->weaponHash;part->weaponCategoryKnown=item->weaponCategoryKnown;part->itemSeen=item->itemSeen;part->itemChangedAt=item->itemChangedAt;}
    return out;
}
inline DualWield dualWield(uint32_t local,uint64_t now){std::lock_guard<std::mutex> lock(mutex);return dualWieldLocked(local,now);}
inline bool modelSlotsKnown=false;
constexpr std::array<uintptr_t,4> callers{0x382750,0x382e76,0x304484,0x3136dd};   // 1.0 (June 0x30b58e, 0x30be82, 0x2a8ee4, 0x2ac095): the second advance of 381ea0 / 382a40 / 303b60, and the advance after 31bb30 (2b2730) in 313231
inline uint32_t entityFor(void* context){
    const auto task=reinterpret_cast<uintptr_t>(context),state=read<uintptr_t>(task);
    const auto index=read<uint32_t>(task+8);
    if(!state||!index||index>0x1000000)throw std::runtime_error("attachment query identity bounds");
    const auto table=read<uintptr_t>(state+0x828);
    if(!table)throw std::runtime_error("attachment entity table absent");
    return read<uint32_t>(table+uintptr_t(index-1)*16);
}
inline Child* child(uint32_t entity,uint64_t now,bool create){
    for(auto& c:children)if(c.entity==entity)return &c;
    if(create)for(auto& c:children)if(!c.entity||(now>=c.seen&&now-c.seen>1000)){c={};c.entity=entity;return &c;}
    return nullptr;
}
struct MeleeAppearance {uint32_t entity=0,item=0,child=0;bool valid=false;uintptr_t world=0;std::array<uint8_t,48> slotBytes{};uint64_t milliseconds=0;uint8_t weaponType=255;uint32_t weaponHash=0;bool weaponCategoryKnown=false;std::array<uint32_t,4> modelGuid{};uint8_t itemSlot=0;bool tool=false;};
// eitherHand (live play): the one weapon in either hand slot, as the tracked weapon display selects it; the finite
// contact trials keep the main-weapon item slot on the left-hand attachment (the measured axe).
inline MeleeAppearance meleeAppearance(uint32_t local,uint64_t now,bool eitherHand=false){
    std::lock_guard<std::mutex> lock(mutex);MeleeAppearance out;
    // A pair counts as the weapon of its right-hand part; the left-hand part strikes through the same attack.
    if(eitherHand){const auto pair=dualWieldLocked(local,now);
        if(pair.valid){const auto& c=pair.right;out={local,c.item,c.entity,true};out.world=c.world;out.slotBytes=c.slotBytes;out.milliseconds=c.slotSeen;out.weaponType=c.weaponType;out.weaponHash=c.weaponHash;
            out.weaponCategoryKnown=c.weaponCategoryKnown;out.modelGuid=c.modelGuid;out.itemSlot=c.itemSlot;out.tool=false;return out;}}
    const Child* chosen=nullptr;
    for(const auto& c:children){
        auto fresh=[&](uint64_t t){return pauseClock::fresh(t,now,500);};
        const bool tool=eitherHand&&liveTools&&c.category!=2&&swungTool(c.itemSlot,c.slot);
        if(c.parent!=local||(c.category!=2&&!tool)||(!eitherHand&&c.itemSlot!=1)||!c.item||!c.handle||!c.slotObserved||c.target!=0||(c.slot!=9&&!(eitherHand&&c.slot==8))||
           !fresh(c.itemSeen)||!fresh(c.slotSeen)||!fresh(c.modelSeen)||c.slotSeen<c.itemChangedAt||c.modelSeen<c.itemChangedAt)continue;
        if(chosen){const auto* kept=survivor(*chosen,c);if(!kept)return {};if(kept==chosen)continue;} // Ambiguous appearance never authorizes an attack.
        chosen=&c;
        out={local,c.item,c.entity,true};out.world=c.world;out.slotBytes=c.slotBytes;out.milliseconds=c.slotSeen;out.weaponType=c.weaponType;out.weaponHash=c.weaponHash;out.weaponCategoryKnown=c.weaponCategoryKnown;out.modelGuid=c.modelGuid;out.itemSlot=c.itemSlot;out.tool=tool;
    }
    return out;
}
// Model handles of everything currently held in the local player's hands (attachment slot 8 HandR / 9 HandL, target 0)
// in this Scene, freshly observed. Real-device feedback 2026-10-09: weapons other than the calibrated axe stayed on the
// hidden native body and floated in front of the eyes; live play hides them (only the tracked clone is drawn).
// anySlot (third run feedback: the glider cape covered half the view while gliding) takes every slot of the local
// player, not only the hands: the VR eyes never show the native body or what hangs on it.
inline std::vector<uint32_t> heldHandles(uint32_t local,uintptr_t scene,uint64_t now,bool anySlot=false){
    std::vector<uint32_t> out;std::lock_guard<std::mutex> lock(mutex);
    auto fresh=[&](uint64_t t){return pauseClock::fresh(t,now,500);};
    for(const auto& c:children)
        if(c.parent==local&&c.handle&&c.modelScene==scene&&c.slotObserved&&c.target==0&&(anySlot||c.slot==8||c.slot==9)&&fresh(c.modelSeen)&&fresh(c.slotSeen))
            out.push_back(c.handle);
    return out;
}
inline void capture(uintptr_t caller,uintptr_t sp,void* context,void* query,uint32_t bytes){
    if(!requested||std::find(callers.begin(),callers.end(),caller)==callers.end())return;
    try{
        rootCamera::Sample local;
        if(!rootCamera::trySnapshot(local)||!local.entity)return; // Startup/stale observations have no attribution yet.
        const auto now=GetTickCount64();
        // 1.0 (query census, Mock game-20261016-081000): 0xc0 / 0x50 / 0xb8 / 0xb8 bytes at rsp+0x30 / +0x30 / +0x30 / +0x50
        // (June 0x80 / 0x30 / 0x78 / 0x58 at +0x20 / +0x20 / +0x20 / +0x50).
        const unsigned expected=caller==callers[0]?0xc0:caller==callers[1]?0x50:0xb8;
        const auto queryOffset=caller==callers[3]?0x50:0x30;
        if(bytes!=expected||reinterpret_cast<uintptr_t>(query)!=sp+queryOffset||read<uintptr_t>(sp+queryOffset)!=reinterpret_cast<uintptr_t>(context))
            throw std::runtime_error("attachment final query ABI differs");
        const auto entity=entityFor(context);
        if(caller==callers[0]){
            if(entity!=local.entity)return;
            // Equipment slot 6, the item registry slot 20 (the map of 4207 items; June slot 6 and 12).
            const auto equipment=read<uintptr_t>(sp+0x30+6*8),registry=read<uintptr_t>(sp+0x30+20*8);
            if(!equipment||!registry)throw std::runtime_error("attachment visual equipment/registry absent");
            itemCatalogObserver::capture(registry,entity);itemRegistry.store(registry);
            const auto first=read<std::array<uint8_t,24*24>>(equipment+0x320);
            if(first!=read<std::array<uint8_t,24*24>>(equipment+0x320))throw std::runtime_error("attachment visual equipment changed");
            for(unsigned n=0;n<24;n++){
                uint32_t id=0,visual=0;std::memcpy(&id,first.data()+n*24+8,4);std::memcpy(&visual,first.data()+n*24+20,4);
                // Which item each equipment visual slot holds, also for items that bring no model of their own (bombs,
                // bait): what is in the hand on which action bar slot, in the order the player selects it.
                {static std::array<uint32_t,24> lastIds{};static std::array<uint32_t,24> lastVisuals{};static unsigned slotEvents=0;
                 if((lastIds[n]!=id||lastVisuals[n]!=visual)&&slotEvents<400){lastIds[n]=id;lastVisuals[n]=visual;++slotEvents;
                  logFile->event("equipment_visual_slot","\"visual_slot\":"+std::to_string(n)+",\"item_id\":"+std::to_string(id)+",\"visual_entity\":"+std::to_string(visual)+",\"frame\":"+std::to_string(liveFrame.frame)+",\"read_only\":true");}}
                if(!id||!visual)continue;
                auto reader=inputRanged::readItemBytes;const auto found=game::lookupItem(reader,registry,id);game::ItemMetadata metadata;
                if(!game::readItemMetadata(reader,found,id,metadata)){
                    {static std::atomic<bool> told{false};if(found.pointer&&!told.exchange(true))try{   // 1.0 layout search: the item as read
                        std::array<uint8_t,0x140> raw{};game::itemRead(reader,found.pointer,raw);
                        logFile->event("item_metadata_layout_dump","\"item\":"+std::to_string(id)+",\"status\":"+jsonString(game::itemLookupName(found.status))+",\"bytes\":"+jsonString(nativeImpactComponents::asHex(raw)));}catch(...){}}
                    throw std::runtime_error("attachment item metadata not stable");}
                const auto wc=metadata.category==2?game::itemWeaponCategory(reader,registry,found,id):game::WeaponCategoryLookup{};
                const bool categoryKnown=wc.status==game::ItemLookupStatus::Found;
                std::lock_guard<std::mutex> lock(mutex);auto c=child(visual,now,true);if(!c)throw std::runtime_error("attachment observation capacity");
                const bool changed=c->parent!=entity||c->item!=id||c->itemSlot!=metadata.slot||c->weaponCategoryKnown!=categoryKnown||c->weaponType!=wc.type||c->weaponHash!=wc.hash;
                c->parent=entity;c->item=id;c->itemSlot=metadata.slot;c->category=metadata.category;c->seen=c->itemSeen=now;c->weaponType=wc.type;c->weaponHash=wc.hash;c->weaponCategoryKnown=categoryKnown;if(changed)c->itemChangedAt=now;
                if(changed&&observations.fetch_add(1)<256)logFile->event("attachment_visual_item","\"entity\":"+std::to_string(visual)+",\"parent\":"+std::to_string(entity)+
                    ",\"visual_slot\":"+std::to_string(n)+",\"item_id\":"+std::to_string(id)+",\"item_slot\":"+std::to_string(metadata.slot)+",\"read_only\":true");
                if(changed&&metadata.category==2){
                    std::array<uint8_t,0x108> raw{};if(!game::itemRead(reader,found.pointer,raw))throw std::runtime_error("equipped category source unreadable");
                    std::ostringstream row;row<<"\"parent\":"<<entity<<",\"entity\":"<<visual<<",\"item\":"<<id<<",\"visual_slot\":"<<n<<",\"item_slot\":"<<unsigned(metadata.slot)
                     <<",\"status\":"<<jsonString(game::itemLookupName(wc.status))<<",\"type\":"<<unsigned(wc.type)<<",\"hash\":"<<wc.hash<<",\"guid\":"<<jsonString(inputRanged::categoryBytes(wc.guid))
                     <<",\"raw_item\":"<<jsonString(inputRanged::categoryBytes(raw))<<",\"resource\":"<<hex(wc.pointer)<<",\"read_only\":true";
                    logFile->event("attachment_weapon_category",row.str());
                }
            }
        }else if(caller==callers[1]){
            const auto link=read<uintptr_t>(sp+0x30+8),hide=read<uintptr_t>(sp+0x30+16);   // slots 1 and 2 as before (1.0: not re-checked by name)
            if(!link||!hide)throw std::runtime_error("attachment hide query missing links");
            const auto parent=read<uint32_t>(link);if(parent!=local.entity)return;
            const auto hidden=read<std::array<uint8_t,2>>(hide);
            std::lock_guard<std::mutex> lock(mutex);auto c=child(entity,now,true);if(!c)throw std::runtime_error("attachment observation capacity");
            const bool changed=c->parent!=parent||!c->hideObserved;c->parent=parent;c->seen=now;c->hideObserved=true;
            if(changed&&observations.fetch_add(1)<256)logFile->event("attachment_child","\"entity\":"+std::to_string(entity)+",\"parent\":"+std::to_string(parent)+
                ",\"hide\":["+std::to_string(hidden[0])+","+std::to_string(hidden[1])+"],\"visual_equipment_query\":true,\"read_only\":true");
        }else if(caller==callers[3]){
            // 2ac073 passes query RSP+80 as arg5; 2b2754/2b2761
            // read SlotAttachment+2c, with target+28 and slot+29.
            const auto component=read<uintptr_t>(sp+0x50+14*8);if(!component)throw std::runtime_error("slot attachment component absent");   // 1.0: SlotAttachment slot 14 (June 6)
            const auto a=read<std::array<uint8_t,48>>(component);uint32_t parent=0;std::memcpy(&parent,a.data()+44,4);
            if(parent!=local.entity){remoteItemFollow(sp,a,parent,now,nullptr);return;}   // another player's character: its items by its datagram
            const auto state=read<uintptr_t>(reinterpret_cast<uintptr_t>(context)),world=read<uintptr_t>(state+0x30);
            nativeImpactComponents::learnSlot(state,entity,component);
            if(nativeImpactComponents::requested&&(!local.world||world!=local.world))return;
            if(a[40]>1||a[41]>=36||a!=read<std::array<uint8_t,48>>(component))throw std::runtime_error("slot attachment fields changed");
            if(followRequested&&a[40]==0&&(a[41]==8||a[41]==9)){
                Follow f;{std::lock_guard<std::mutex> lock(followMutex);f=follow.entity==entity?follow:followLeft;}
                if(f.entity==entity&&f.parent==parent&&f.world==world){
                    if(now<f.at||now-f.at>250)followStale.fetch_add(1);
                    else{
                        // 2ac044: the third argument of 2b2730, the transform the attachment system has just written.
                        const auto transform=read<uintptr_t>(sp+0x50+8*8);   /* 1.0: RenderTransform slot 8 (June 4) */const auto native=read<std::array<uint8_t,56>>(transform);
                        WorldPositionQ32 from,to;Quat q;Vec3 scale;std::memcpy(&from,native.data(),24);std::memcpy(&q,native.data()+24,16);std::memcpy(&scale,native.data()+40,12);std::memcpy(&to,f.transform.data(),24);
                        const double dx=relativeQ32(to.x,from.x),dy=relativeQ32(to.y,from.y),dz=relativeQ32(to.z,from.z),metres=std::sqrt(dx*dx+dy*dy+dz*dz);
                        const float norm=q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w;
                        // The tracked hand is within arm's reach of the animated one; anything else is not this transform.
                        if(!std::isfinite(norm)||std::abs(norm-1)>.02f||!finite(scale)||scale.x<=0||scale.x>4||!(metres<3.)){
                            if(followRejected.fetch_add(1)<4)logFile->event("held_follow_rejected","\"entity\":"+std::to_string(entity)+",\"metres\":"+std::to_string(metres));
                        }else{
                            double rebased=0,turned=0;
                            try{
                                if(f.anchorSample){
                                    const auto state=read<uintptr_t>(reinterpret_cast<uintptr_t>(context));if(!state)throw std::runtime_error("attachment task state absent");
                                    const auto fraction=read<float>(state+0x88);
                                    const int historyId=componentIdByName(world,"keen::ecs::RenderTransform",176);if(historyId<0)throw std::runtime_error("no RenderTransform in this World");
                                    const auto at=nativeImpactComponents::component(nativeImpactComponents::entityAtWorld(world,parent),unsigned(historyId),176);
                                    if(!at)throw std::runtime_error("the parent has no RenderTransform");
                                    const auto history=read<game::RootHistory>(at);const auto tick=history.newestTick;
                                    const auto here=game::stableRoot(history,tick,fraction).position;
                                    double turnNow=0;{std::lock_guard<std::mutex> turnLock(cameraControl::mutex);turnNow=cameraControl::gameplayTurn;}
                                    {std::lock_guard<std::mutex> lock(predictedMutex);lastPredictedRoot=here;lastPredictedTurn=turnNow;lastPredictedValid=true;lastPredictedTick=tick;lastPredictedFraction=fraction;lastPredictedSourceTick=history.sourceTick;lastPredictedNewestTick=history.newestTick;}
                                    const double mx=relativeQ32(here.x,f.anchorRoot.x),my=relativeQ32(here.y,f.anchorRoot.y),mz=relativeQ32(here.z,f.anchorRoot.z);rebased=std::sqrt(mx*mx+my*my+mz*mz);
                                    // A step of a frame, not a teleport: beyond a metre the pose is written as it is.
                                    if(rebased<1.){to.x+=here.x-f.anchorRoot.x;to.y+=here.y-f.anchorRoot.y;to.z+=here.z-f.anchorRoot.z;std::memcpy(f.transform.data(),&to,24);
                                        if(rebased>1e-4){followRebased.fetch_add(1);followRebasedMicrometres.fetch_add(int64_t(rebased*1e6));}}
                                    else rebased=0;
                                    // The turn: about the root's vertical, position and facing alike (the anchor's height offset lies on it).
                                    turned=turnNow-f.anchorTurn;if(std::abs(turned)<1e-9)turned=f.anchorTurnStep;
                                    if(std::abs(turned)>1e-5&&std::abs(turned)<1.){
                                        const float angle=float(turned*followTurnSense);const Quat turn{0,std::sin(angle*.5f),0,std::cos(angle*.5f)};
                                        WorldPositionQ32 at;Quat facing;std::memcpy(&at,f.transform.data(),24);std::memcpy(&facing,f.transform.data()+24,16);
                                        const Vec3 arm{float(relativeQ32(at.x,here.x)),float(relativeQ32(at.y,here.y)),float(relativeQ32(at.z,here.z))};
                                        if(finite(arm)&&length(arm)<3.f){
                                            const Vec3 swung=rotate(turn,arm);
                                            at.x=here.x+int64_t(double(swung.x)*4294967296.);at.y=here.y+int64_t(double(swung.y)*4294967296.);at.z=here.z+int64_t(double(swung.z)*4294967296.);
                                            facing=normalized(multiply(turn,facing));
                                            std::memcpy(f.transform.data(),&at,24);std::memcpy(f.transform.data()+24,&facing,16);
                                            const auto count=followTurned.fetch_add(1);
                                            if(count<4||count%600==0)logFile->event("held_follow_turn","\"entity\":"+std::to_string(entity)+",\"turns\":"+std::to_string(count+1)+",\"radians\":"+std::to_string(angle)+",\"arm_metres\":"+std::to_string(length(arm)));
                                        }
                                    }else turned=0;
                                }else followUnanchored.fetch_add(1);
                            }catch(const std::exception&){followUnanchored.fetch_add(1);}
                            std::memcpy(reinterpret_cast<void*>(transform),f.transform.data(),40);
                            {std::lock_guard<std::mutex> lock(writtenMutex);std::memcpy(&lastWritten,f.transform.data(),24);lastWrittenAt=now;++lastWrittenCount;}
                            if(followTrace){traceEntityTypes(world,entity);
                                logFile->event("held_follow_anchor","\"entity\":"+std::to_string(entity)+",\"pose_frame\":"+std::to_string(f.frame)+",\"pose_sample\":"+std::to_string(f.anchorSample)+
                                    ",\"rebased_m\":"+std::to_string(rebased)+",\"turned\":"+std::to_string(turned));
                                static std::atomic<unsigned> renderDumps{0};if(renderDumps.fetch_add(1)%120==0)
                                    logFile->event("held_follow_render_transform","\"entity\":"+std::to_string(entity)+",\"after_write\":"+jsonString(nativeImpactComponents::asHex(read<std::array<uint8_t,176>>(transform))));}
                            const auto n=followWrites.fetch_add(1);
                            if(n<4||n%600==0)logFile->event("held_follow_write","\"entity\":"+std::to_string(entity)+",\"writes\":"+std::to_string(n+1)+",\"moved_metres\":"+std::to_string(metres)+",\"player_moved_since_pose_metres\":"+std::to_string(rebased)+",\"rebased_writes\":"+std::to_string(followRebased.load())+",\"unanchored\":"+std::to_string(followUnanchored.load())+
                                ",\"pose_age_ms\":"+std::to_string(now-f.at)+",\"pose_frame\":"+std::to_string(f.frame)+",\"slot\":"+std::to_string(a[41])+",\"native_transform\":"+jsonString(nativeImpactComponents::asHex(native))+",\"slot_attachment_unchanged\":true");
                        }
                    }
                }
            }
            if(selfItems){Follow f;{std::lock_guard<std::mutex> lock(followMutex);f=follow.entity==entity?follow:followLeft;}
                remoteItemFollow(sp,a,parent,now,f.entity==entity&&now>=f.at&&now-f.at<=250?&f.transform:nullptr);}
            std::lock_guard<std::mutex> lock(mutex);auto c=child(entity,now,true);if(!c)throw std::runtime_error("attachment observation capacity");
            const bool changed=c->parent!=parent||!c->slotObserved||c->target!=a[40]||c->slot!=a[41];c->parent=parent;c->seen=c->slotSeen=now;c->slotObserved=true;c->target=a[40];c->slot=a[41];c->world=world;c->slotBytes=a;
            if(changed&&observations.fetch_add(1)<256)logFile->event("attachment_slot","\"entity\":"+std::to_string(entity)+",\"parent\":"+std::to_string(parent)+
                ",\"target\":"+std::to_string(a[40])+",\"slot\":"+std::to_string(a[41])+",\"read_only\":true");
        }else{
            std::lock_guard<std::mutex> lock(mutex);auto c=child(entity,now,false);
            if(!c||c->parent!=local.entity||now<c->seen||now-c->seen>1000)return;
            // 1.0 (query census, Mock game-20261016-081657): ModelComponent is slot 9 (sp+0x78, as June's slot 11 at +0x20 was);
            // the render owner is one of the two resources after the components (slots 21 / 22): the one whose View points back.
            const auto component=read<uintptr_t>(sp+0x30+9*8);uintptr_t owner=0;
            for(const unsigned slot:{22u,21u}){const auto o=read<uintptr_t>(sp+0x30+slot*8);
                try{const auto v=o?read<uintptr_t>(o+layout::ownerView):0;if(v&&read<uintptr_t>(v)==o){owner=o;break;}}catch(...){}}
            if(!component||!owner)throw std::runtime_error("attachment model query missing owner");
            const auto scene=read<uintptr_t>(owner+layout::ownerScene),view=read<uintptr_t>(owner+layout::ownerView);
            const bool proven=scene==provenWorldScene&&view==provenWorldView;
            if(!proven&&!allowCandidateScene)return;
            if(!scene||!view||read<uintptr_t>(view)!=owner||read<uintptr_t>(view+8)!=scene)throw std::runtime_error("attachment native Scene/View owner mismatch");
            const auto handle=read<uint32_t>(component);if(!handle)return;
            const auto index=handle&0xffffff;const auto objects=read<uintptr_t>(scene+0x1b0),generations=read<uintptr_t>(scene+0x1c8);
            const auto size=read<uint64_t>(scene+0x1b8),count=read<uint64_t>(scene+0x1d0);
            if(!objects||!generations||size%0x1b0||index>=size/0x1b0||index>=count||read<uint8_t>(generations+index)!=(handle>>24))throw std::runtime_error("attachment model generation invalid");
            const auto object=objects+uintptr_t(index)*0x1b0,resource=read<uintptr_t>(object+layout::modelResource);
            if(!resource||read<uint8_t>(resource+0x14)!=1)return;
            const auto model=read<uint16_t>(resource+layout::resourceModel);const auto mask=read<uint32_t>(object+layout::modelMask);
            // cd1f0 copies constructor GUID at +18 and owns refcount+10.
            // d2a70 releases this reference. Observe only, never increment it.
            const auto guid=read<std::array<uint32_t,4>>(resource+0x18);
            const auto refs=read<uint32_t>(resource+0x10);
            const auto pose=read<std::array<uint8_t,56>>(object+0x38);
            if(!refs||!(guid[0]|guid[1]|guid[2]|guid[3])||guid!=read<std::array<uint32_t,4>>(resource+0x18)||resource!=read<uintptr_t>(object+layout::modelResource))throw std::runtime_error("attachment resource identity unstable");
            static constexpr char digits[]="0123456789abcdef";std::string transform;
            for(auto b:pose){transform+=digits[b>>4];transform+=digits[b&15];}
            if(read<uint32_t>(component)!=handle||read<uint8_t>(generations+index)!=(handle>>24))throw std::runtime_error("attachment model changed during read");
            if((c->handle!=handle||c->model!=model)&&observations.fetch_add(1)<256)logFile->event("attachment_model","\"entity\":"+std::to_string(entity)+",\"parent\":"+std::to_string(c->parent)+
                ",\"item_id\":"+std::to_string(c->item)+",\"item_slot\":"+std::to_string(c->itemSlot)+",\"handle\":"+std::to_string(handle)+",\"model\":"+std::to_string(model)+
                ",\"resource_guid_words\":["+std::to_string(guid[0])+","+std::to_string(guid[1])+","+std::to_string(guid[2])+","+std::to_string(guid[3])+
                "],\"resource_references\":"+std::to_string(refs)+",\"current_transform_bytes\":\""+transform+"\",\"world_scene_proven\":"+(proven?"true":"false")+
                ",\"mask\":"+std::to_string(mask)+",\"scene\":"+hex(scene)+",\"parent_age_ms\":"+std::to_string(now-c->seen)+",\"read_only\":true");
            c->handle=handle;c->model=model;c->modelGuid=guid;c->modelSeen=now;c->modelScene=scene;c->modelView=view;
        }
    }catch(const std::exception& e){if(failures.fetch_add(1)<4)logFile->event("attachment_observer_failure","\"caller\":"+hex(caller)+",\"reason\":"+jsonString(e.what()));}
}
}
