// Other players' VR bodies (user, 2026-10-13: "the goal is what VHVR has - every player's IK is synchronised, the
// movements are seen"; 2026-10-14: the datagrams go from mod to mod over Steam). Each frame, at the place where the
// own body is solved: what came from the other players is taken in, this player's body goes out to those of them who
// run the mod, and the characters of the players that sent a body are driven with it (include/evr/player_sync.hpp:
// the datagram, the receiver's stepping, who is sent what; steam_link.hpp: the datagrams' way; remote_body.hpp: the
// IK on another character). Whose body a datagram is, is found by where its sender says its character stands.
// The held items of other players (2026-10-15, 2.1 of spec_20261015.md): the datagram carries the main hand's and the
// off hand's item relative to the hand sockets; here they are put about the driven character's hands, and the
// attachment pass writes them (attachment_observer.hpp remoteItemFollow). Without a sent item the game's own
// attachment stays (the animated hand).
// EVR_ONLINE_SYNC=1 in the game's environment switches it on (with the body IK; no launch flag yet).
// EVR_SYNC_SELF=1: a test for one player - the only player sent to is this one (Steam delivers a message to oneself
// within the process), and the own character is driven by what comes back instead of by the own solve directly: the
// whole way of another player's body, seen on the own one. EVR_SYNC_SELF=2: the same without Steam (steam_link.hpp).
namespace onlineSync {
inline bool requested=false,selfTest=false;
inline sync::Peers peers;inline uint16_t sequence=0;
inline double listedAt=-1e18,lastBodyAt=-1e18,statsAt=0;inline sync::Place lastPlace{};inline bool placed=false;
inline uint64_t refused=0,notCounted=0,drivenFrames=0,ownDrivenFrames=0,bodiesSent=0,hellosSent=0;inline unsigned logs=0;inline unsigned unboundLogs=0;inline size_t listedCount=SIZE_MAX;
// The mod without VR (view_resource_probe.cpp onlineFlat): there is no body to send, but where the own character
// stands goes out once a second, so that VR players near know a player with the mod is there and send theirs.
inline bool flat=false,flatPlaced=false;inline sync::Place flatPlace{};
// Players learned from other players' rosters (id -> when), listed for thirty seconds after the last roster that named
// them; and when this player last sent its roster to each peer.
inline std::map<uint64_t,double> learned;inline std::map<uint64_t,double> rosterSentAt;inline uint64_t rostersSent=0,rostersHeard=0,learnedTotal=0;
inline constexpr double learnedFor=30,rosterEvery=1;
inline constexpr double sightMetres=250;      // players farther apart than this exchange a datagram a second, not their bodies
inline constexpr size_t mostEveryFrame=12;    // the nearest this many within sight get the body every frame (2.5); the rest a datagram a second
inline double seconds(){return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();}
inline void configure(bool always=false){
    char value[8]{};auto n=GetEnvironmentVariableA("EVR_ONLINE_SYNC",value,sizeof value);requested=always||(n&&n<sizeof value&&value[0]=='1');
    char self[8]{};n=GetEnvironmentVariableA("EVR_SYNC_SELF",self,sizeof self);selfTest=requested&&n&&n<sizeof self&&(self[0]=='1'||self[0]=='2');
    if(!requested)return;
    equipmentObserver::trackOthers=true;motionBodyIk::remoteDrivesOwn=selfTest;
    {char v[8]{};const auto m=GetEnvironmentVariableA("EVR_SYNC_SELF_ITEMS",v,sizeof v);attachmentObserver::selfItems=m&&m<sizeof v?v[0]=='1':selfTest;}
    logFile->event("online_sync_enabled","\"self_test\":"+std::string(selfTest?"true":"false")+",\"datagram_bytes\":["+std::to_string(sync::helloBytes)+","+std::to_string(sync::pausedBytes)+","+std::to_string(sync::fullBytes)+
        "],\"version\":"+std::to_string(sync::version)+",\"held_items\":true,\"self_items\":"+(attachmentObserver::selfItems?"true":"false"));
}
struct Character {uint32_t entity=0;std::array<uint32_t,24> handles{};std::array<double,3> at{};bool own=false;};
// The sent items about the driven character: hand socket (world-oriented, about the character) and the item in it
// -> the item's world transform (position Q32 from the character's root, orientation), one per attachment slot.
inline unsigned itemsPublished=0;
inline void publishItems(uint32_t entity,const sync::Body& body,const sync::Body* newest=nullptr){
    const auto found=remoteBody::bodies.find(entity);if(found==remoteBody::bodies.end())return;const auto& root=found->second.root;
    const auto now=GetTickCount64();
    const auto put=[&](unsigned hand,const Pose& inHand,std::initializer_list<uint8_t> slots){
        const auto world=sync::fromHand(body.hand[hand],inHand);if(!finite(world.position)||length(world.position)>4.f)return;
        attachmentObserver::RemoteItem r;r.parent=entity;r.at=now;r.frame=liveFrame.frame;
        WorldPositionQ32 q{root.x+int64_t(double(world.position.x)*4294967296.),root.y+int64_t(double(world.position.y)*4294967296.),root.z+int64_t(double(world.position.z)*4294967296.)};
        const auto o=normalized(world.orientation);std::memcpy(r.transform.data(),&q,24);std::memcpy(r.transform.data()+24,&o,16);
        for(const auto slot:slots){r.slot=slot;attachmentObserver::publishRemoteItem(r);}++itemsPublished;};
    if(body.item){put(1,body.itemInHand,{8});
        // The same weapon for the server's sweep of this player's strokes (sequence_collider_observer.hpp remoteMelee).
        // As it came (2026-10-20): the stepping toward the newest (a fifth each 20 ms) leaves a fast stroke some 80 ms behind
        // the real blade; what is drawn keeps the stepping, the sweep takes the newest datagram's hand and weapon.
        const auto& hit=newest&&newest->ik&&newest->item?*newest:body;
        const auto world=sync::fromHand(hit.hand[1],hit.itemInHand);
        if(finite(world.position)&&length(world.position)<=4.f){
            const WorldPositionQ32 q{root.x+int64_t(double(world.position.x)*4294967296.),root.y+int64_t(double(world.position.y)*4294967296.),root.z+int64_t(double(world.position.z)*4294967296.)};
            const auto o=normalized(world.orientation);std::array<uint8_t,40> t{};std::memcpy(t.data(),&q,24);std::memcpy(t.data()+24,&o,16);
            remoteMelee::publishWeapon(entity,root,t,now);}}
    if(body.offhandItem)put(0,body.offhandInHand,{9,12});
}
inline bool placeOf(const std::array<uint32_t,24>& handles,std::array<double,3>& at){
    for(unsigned part=0;part<20;part++)if(const auto handle=handles[part]){const auto object=motionVisual::currentObject(handle);if(!object)continue;
        const auto q=read<std::array<int64_t,3>>(object+0x38);at={double(q[0])/4294967296.,double(q[1])/4294967296.,double(q[2])/4294967296.};return true;}
    return false;
}
// The other players' characters of this Scene as the equipment observer saw them within the last moments (and the own
// one for the self test).
inline std::vector<Character> characters(uint32_t local){
    std::vector<Character> out;const auto now=GetTickCount64();
    {std::lock_guard<std::mutex> lock(equipmentObserver::otherMutex);
     for(auto it=equipmentObserver::others.begin();it!=equipmentObserver::others.end();){
        if(now-it->second.milliseconds>10000){it=equipmentObserver::others.erase(it);continue;}
        if(it->first!=local&&it->second.scene==motionVisual::scene&&now-it->second.milliseconds<=1500&&out.size()<32){Character c;c.entity=it->first;c.handles=it->second.handles;out.push_back(c);}
        ++it;}}
    if(selfTest){std::lock_guard<std::mutex> lock(equipmentObserver::snapshotMutex);const auto& s=equipmentObserver::latest;
        if(s.number&&s.entity==local&&s.scene==motionVisual::scene&&now-s.milliseconds<=1500){Character c;c.entity=local;c.handles=s.handles;c.own=true;out.push_back(c);}}
    out.erase(std::remove_if(out.begin(),out.end(),[](Character& c){return !placeOf(c.handles,c.at);}),out.end());
    return out;
}
// Actions of the other players' mods (include/evr/player_sync.hpp sync::Action): each id once per sender.
inline std::map<uint64_t,std::array<uint16_t,8>> actionsSeen;inline uint64_t actionsHeard=0,actionsRefused=0,actionsSent=0;
inline void action(uint64_t from,const uint8_t* data,size_t size){
    const auto a=sync::decodeAction(data,size);if(!a){++actionsRefused;return;}
    auto& seen=actionsSeen[from];const uint16_t key=uint16_t(a->id^(uint16_t(a->kind)<<12));
    if(std::find(seen.begin(),seen.end(),key)!=seen.end())return;
    std::rotate(seen.rbegin(),seen.rbegin()+1,seen.rend());seen[0]=key;++actionsHeard;
    if(a->kind==sync::castAction){fishingCharge::peerCast(from,a->strength);
        if(logs++<200)logFile->event("online_sync_cast_heard","\"player\":\""+steamLink::tail(from)+"\",\"id\":"+std::to_string(a->id)+",\"strength\":"+std::to_string(a->strength));}
}
// This player's casts out to every listed player: the strength of the swing at the trigger's release (0 without a
// swing), three frames in a row (a datagram may be lost; the receiver takes the id once).
inline uint64_t castSeen=0;inline uint16_t castId=0;inline unsigned castRepeats=0;inline float castStrength=0;
inline void sendCasts(){
    const auto tap=controllerUiFixture::castTapAt.load();
    if(tap&&tap!=castSeen){castSeen=tap;++castId;castRepeats=3;
        controllerUiFixture::FishingSwing swing;{std::lock_guard<std::mutex> lock(controllerUiFixture::fishingAimMutex);swing=controllerUiFixture::fishingSwing;}
        const auto now=GetTickCount64();castStrength=swing.serial&&swing.until>now?swing.strength:0.f;}
    if(!castRepeats)return;--castRepeats;
    const auto bytes=sync::encodeCast(castId,castStrength);
    for(auto& p:peers.all())if(p.listed&&steamLink::send(p.id,bytes))++actionsSent;
}
inline void frame(uint64_t renderSerial) noexcept {
    if(!requested)return;
    try{
        if(!steamLink::start())return;
        const double now=seconds();
        if(now-listedAt>=5){listedAt=now;
            auto ids=selfTest?std::vector<uint64_t>{steamLink::self}:steamLink::playersHere();
            for(auto it=learned.begin();it!=learned.end();){if(now-it->second>learnedFor)it=learned.erase(it);else{if(std::find(ids.begin(),ids.end(),it->first)==ids.end()&&it->first!=steamLink::self&&ids.size()<64)ids.push_back(it->first);++it;}}
            peers.list(ids,now);
            if(ids.size()!=listedCount){listedCount=ids.size();std::string tails;for(const auto id:ids)tails+=std::string(tails.empty()?"":",")+"\""+steamLink::tail(id)+"\"";
                if(logs++<200)logFile->event("online_sync_players","\"listed\":"+std::to_string(ids.size())+",\"players\":["+tails+"],\"friends_in_this_game_and_lobby_members_only\":true");}}
        steamLink::receive([&](uint64_t from,const uint8_t* data,size_t size){
            if(size&&data&&data[0]==sync::actionMagic){action(from,data,size);return;}
            const auto r=sync::decode(data,size);if(!r){++refused;return;}
            if(!peers.heard(from,*r,now))++notCounted;
            else if(!r->roster.empty()){++rostersHeard;for(const auto id:r->roster)if(id!=steamLink::self){if(!learned.count(id)){++learnedTotal;if(logs++<200)logFile->event("online_sync_learned","\"player\":\""+steamLink::tail(id)+"\",\"from\":\""+steamLink::tail(from)+"\"");}learned[id]=now;}}});
        sendCasts();
        // Out: the body of this frame; for a second after it stopped (the view left first person, the hands are gone)
        // "the IK is paused", so that the others let go of the body at once; a hello otherwise.
        const auto& mine=motionBodyIk::published;const bool have=mine.have&&mine.serial==renderSerial;
        if(have){lastBodyAt=now;lastPlace=mine.place;placed=true;}
        else if(flat&&flatPlaced){lastPlace=flatPlace;placed=true;}
        const bool stopping=!have&&placed&&now-lastBodyAt<=1;
        // The body every frame to the nearest players within sight (twelve at the most, 2.5); the others a datagram a second.
        std::vector<std::array<double,3>> places;std::vector<char> known;
        for(const auto& p:peers.all()){places.push_back(sync::metres(p.place));known.push_back(p.placed?1:0);}
        std::vector<char> everyFrame(peers.all().size(),0);
        if(placed)for(const auto i:sync::bodyRecipients(sync::metres(lastPlace),places,known,sightMetres,mostEveryFrame))everyFrame[i]=1;
        size_t index=0;
        for(auto& p:peers.all()){
            const bool inSight=everyFrame[index++]!=0;
            const auto due=peers.due(p,now,(have||stopping)&&inSight);
            if(due==sync::Peers::Due::Nothing)continue;
            std::vector<uint8_t> bytes;
            if(have)bytes=sync::encode(mine.body,++sequence,mine.place);
            else if(stopping||(placed&&due==sync::Peers::Due::Hello&&(flat||now-lastBodyAt<=30))){sync::Body paused;paused.ik=false;bytes=sync::encode(paused,++sequence,lastPlace);}
            else bytes=sync::encodeHello(++sequence);
            if(steamLink::send(p.id,bytes)){if(bytes.size()>=sync::fullBytes)++bodiesSent;else ++hellosSent;}
        }
        // The roster: once a second to every listed player, the other players heard from within five seconds.
        for(auto& p:peers.all()){
            if(!p.listed||now-rosterSentAt[p.id]<rosterEvery)continue;
            std::vector<uint64_t> known;for(const auto& q:peers.all())if(q.id!=p.id&&now-q.heard<=5&&known.size()<sync::rosterMost)known.push_back(q.id);
            if(known.empty())continue;rosterSentAt[p.id]=now;
            if(steamLink::send(p.id,sync::encodeRoster(++sequence,known)))++rostersSent;
        }
        // In: every player's newest body steps on, finds its character and drives it. Only inside this frame's
        // proven native mutation boundary (as the own body), and only characters nobody else's body took this frame.
        const bool boundary=motionVisual::contextReady&&motionVisual::contextSerial==renderSerial&&motionVisual::contextFrame==liveFrame.frame&&motionVisual::scene;
        if(boundary){
            const auto local=rootCamera::snapshot().entity;const auto list=characters(local);
            std::vector<std::array<double,3>> at;for(const auto& c:list)at.push_back(c.at);
            std::vector<char> taken(list.size(),0);
            // Why a player's character was let go (2026-10-20: in the first session with a VR guest both players' bodies were
            // let go and bound again once a second for twenty seconds, 1.1 to 2.0 m from the sent place; the logs did not say why).
            const auto unbound=[&](const sync::Peers::Peer& p,const char* why,double metres){
                if(p.character<0||unboundLogs>=300)return;++unboundLogs;
                logFile->event("online_sync_unbound","\"player\":\""+steamLink::tail(p.id)+"\",\"entity\":"+std::to_string(p.character)+",\"reason\":\""+why+"\",\"metres_from_the_sent_place\":"+std::to_string(metres)+
                    ",\"characters_seen\":"+std::to_string(list.size())+",\"since_heard_s\":"+std::to_string(now-p.heard));};
            for(auto& p:peers.all()){
                const auto body=p.receiver.update(now);
                if(!body||!body->ik||!p.placed){unbound(p,!body?"no_body":!body->ik?"ik_paused":"no_place",-1);p.character=-1;continue;}
                int previous=-1;for(size_t i=0;i<list.size();i++)if(int64_t(list[i].entity)==p.character)previous=int(i);
                // A bound character stays bound farther from the sent place the older that place is (2026-10-20: in the first
                // session with a VR guest both players sent each other a datagram a second for twenty seconds - out of each
                // other's sight - and each body was let go about 0.5 s after every datagram and bound again with the next,
                // 1.6..2.0 m from the fresh place: the character ran on more than 6 m from a place a second old, guess).
                // 6 m, and 15 m for each second since that player was last heard from.
                const double keep=6+15*std::clamp(now-p.heard,0.,4.);
                const int index=sync::boundCharacter(previous,sync::metres(p.place),at,3,1,keep);
                if(index<0||taken[size_t(index)]){unbound(p,index<0?(previous<0?"character_not_seen":"farther_than_the_kept_distance"):"taken",previous>=0?sync::distance(sync::metres(p.place),at[size_t(previous)]):-1);p.character=-1;continue;}
                const auto& c=list[size_t(index)];taken[size_t(index)]=1;
                if(p.character!=int64_t(c.entity)&&logs++<200)logFile->event("online_sync_character","\"player\":\""+steamLink::tail(p.id)+"\",\"entity\":"+std::to_string(c.entity)+",\"own_character\":"+(c.own?"true":"false")+
                    ",\"metres_from_the_sent_place\":"+std::to_string(sync::distance(sync::metres(p.place),c.at))+",\"characters_seen\":"+std::to_string(list.size()));
                p.character=c.entity;
                const bool drove=remoteBody::drive(c.entity,c.handles,c.own,*body,renderSerial);
                if(!drove&&unboundLogs<300){++unboundLogs;logFile->event("online_sync_unbound","\"player\":\""+steamLink::tail(p.id)+"\",\"entity\":"+std::to_string(c.entity)+",\"reason\":\"drive_failed\",\"metres_from_the_sent_place\":"+std::to_string(sync::distance(sync::metres(p.place),c.at)));}
                if(drove){++drivenFrames;
                    if(c.own){++ownDrivenFrames;motionBodyIk::ready=true;motionBodyIk::serial=renderSerial;}
                    publishItems(c.entity,*body,&p.receiver.newest());}
            }
            remoteBody::releaseOthers(renderSerial,"no body for the character");
        }
        if(now-statsAt>=10){statsAt=now;unsigned heard=0,bound=0;for(const auto& p:peers.all()){if(now-p.heard<=5)++heard;if(p.character>=0)++bound;}
            if(logs++<2000)logFile->event("online_sync_stats","\"frame\":"+std::to_string(liveFrame.frame)+",\"listed\":"+std::to_string(peers.all().size())+",\"heard_from\":"+std::to_string(heard)+",\"with_a_character\":"+std::to_string(bound)+
                ",\"bodies_sent\":"+std::to_string(bodiesSent)+",\"hellos_sent\":"+std::to_string(hellosSent)+",\"sent_bytes\":"+std::to_string(steamLink::sentBytes.load())+",\"send_failures\":"+std::to_string(steamLink::sendFailures.load())+
                ",\"last_send_result\":"+std::to_string(steamLink::lastSendResult)+",\"received\":"+std::to_string(steamLink::received.load())+",\"received_bytes\":"+std::to_string(steamLink::receivedBytes.load())+
                ",\"refused\":"+std::to_string(refused)+",\"actions_heard\":"+std::to_string(actionsHeard)+",\"actions_refused\":"+std::to_string(actionsRefused)+",\"actions_sent\":"+std::to_string(actionsSent)+",\"not_counted\":"+std::to_string(notCounted)+",\"driven_frames\":"+std::to_string(drivenFrames)+",\"own_driven_frames\":"+std::to_string(ownDrivenFrames)+
                ",\"rosters_sent\":"+std::to_string(rostersSent)+",\"rosters_heard\":"+std::to_string(rostersHeard)+",\"learned\":"+std::to_string(learned.size())+",\"game_peers\":"+std::to_string(steamLink::gamePeersWithin(10000).size())+",\"game_messages_seen\":"+std::to_string(steamLink::peerMessages.load())+
                ",\"items_published\":"+std::to_string(itemsPublished)+",\"item_writes\":"+std::to_string(attachmentObserver::remoteItemWrites.load())+",\"item_rejected\":"+std::to_string(attachmentObserver::remoteItemRejected.load())+",\"item_stale\":"+std::to_string(attachmentObserver::remoteItemStale.load()));}
    }catch(const std::exception& e){if(logs++<2000)try{logFile->event("online_sync_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
    catch(...){}
}
inline void dispose(uintptr_t scene) noexcept {remoteBody::dispose(scene);}
}
