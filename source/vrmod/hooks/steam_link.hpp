// Datagrams between players who run this mod, over Steam (user decision 2026-10-14: "mod to mod, Steam's direct
// messages"; docs/analysis/online_sync_20261013/notes.md 3 and 8). The game's own traffic is not touched: nothing of
// the game is hooked here. The functions are the plain-C entries of the steam_api64.dll the game has loaded
// (ISteamNetworkingMessages: unreliable messages to a Steam user on a channel number of the mod's own), taken by name.
// Who is sent to: friends who are in this game right now, and the members of a lobby this player is in - never
// anybody else; a player without the mod never answers, and then gets a few bytes a second and nothing more
// (include/evr/player_sync.hpp Peers). A session with a player opens by sending to that player, so only the players
// sent to can send here. Steam ids go on record by their last four digits only.
// EVR_SYNC_SELF=2: no Steam at all - what is sent to "this player" comes back from a queue in the process (tests).
// The game's own peers (2026-10-15, user: "take the ids from the game's own connections - we will need it anyway"):
// the players the game itself receives from - on Steam networking sockets (the sessions, P2P or a server), on Steam
// networking messages (the game's own channels) and the old P2P packets. The implementations behind the interfaces'
// tables (steamclient's code, which every caller reaches) are hooked; the messages are only read, after the game got
// them. A host hears every client, a client hears its host; the others come by the roster of player_sync.hpp.
// EVR_SYNC_GAME_PEERS=0 leaves the game's interfaces alone (friends and lobby members only, as before).
namespace steamLink {
inline constexpr int channel=0x4556;                 // "EV": the mod's own; a game's channels are small numbers
inline constexpr int sendFlags=5|32;                 // unreliable, no delay (not queued behind a connection that is still made), restart a broken session
inline constexpr uint32_t mostBytes=256;
struct FriendGame {uint64_t game=0;uint32_t address=0;uint16_t port=0,queryPort=0;uint64_t lobby=0;};   // FriendGameInfo_t
static_assert(sizeof(FriendGame)==24);
struct Api {
    void* (*messages)()=nullptr;void* (*user)()=nullptr;void* (*friends)()=nullptr;void* (*utils)()=nullptr;void* (*matchmaking)()=nullptr;
    int (*hUser)()=nullptr;
    int (*send)(void*,const void*,const void*,uint32_t,int,int)=nullptr;
    int (*receive)(void*,int,void**,int)=nullptr;
    void (*release)(void*)=nullptr;
    void (*identityClear)(void*)=nullptr;void (*identitySet)(void*,uint64_t)=nullptr;uint64_t (*identityGet)(void*)=nullptr;
    uint64_t (*steamId)(void*)=nullptr;uint32_t (*appId)(void*)=nullptr;
    int (*friendCount)(void*,int)=nullptr;uint64_t (*friendAt)(void*,int,int)=nullptr;bool (*friendGame)(void*,uint64_t,FriendGame*)=nullptr;
    int (*lobbyMembers)(void*,uint64_t)=nullptr;uint64_t (*lobbyMember)(void*,uint64_t,int)=nullptr;
    int (*coplayCount)(void*)=nullptr;uint64_t (*coplayAt)(void*,int)=nullptr;int (*coplayTime)(void*,uint64_t)=nullptr;uint32_t (*coplayGame)(void*,uint64_t)=nullptr;
};
inline Api api;inline void* messages=nullptr;inline bool tried=false,ready=false,inProcess=false;
inline uint64_t self=0;inline uint32_t app=0;
inline std::atomic<uint64_t> sent{0},sentBytes{0},sendFailures{0},received{0},receivedBytes{0},oversized{0};
inline int lastSendResult=1;
inline std::deque<std::vector<uint8_t>> loop;       // EVR_SYNC_SELF=2
inline std::string tail(uint64_t id){char text[8];std::snprintf(text,sizeof text,"%04u",unsigned(id%10000));return text;}
inline std::mutex peerMutex;inline std::map<uint64_t,uint64_t> gamePeers;   // id -> last heard, GetTickCount64
inline std::atomic<unsigned> peerNotes{0};inline std::atomic<uint64_t> peerMessages{0};
inline void notePeer(uint64_t id) noexcept {
    if(!id||id==self||((id>>52)&0xf)!=1)return;   // individuals only: not a game server, not a lobby
    ++peerMessages;const auto now=GetTickCount64();std::lock_guard<std::mutex> lock(peerMutex);
    auto& last=gamePeers[id];const bool first=!last;last=now;
    if(first&&peerNotes.fetch_add(1)<40)try{logFile->event("steam_game_peer_seen","\"player\":\""+tail(id)+"\",\"peers\":"+std::to_string(gamePeers.size()));}catch(...){}
}
inline std::vector<uint64_t> gamePeersWithin(uint64_t milliseconds){
    std::vector<uint64_t> out;const auto now=GetTickCount64();std::lock_guard<std::mutex> lock(peerMutex);
    for(const auto& [id,at]:gamePeers)if(now>=at&&now-at<=milliseconds)out.push_back(id);
    return out;
}
// The hooks on the game's receiving: the message list the game got, read for the senders.
using ReceiveList=int(__fastcall*)(void*,uint32_t,void**,int);         // ISteamNetworkingSockets: (this, poll group or connection, messages, most)
using ReceiveChannel=int(__fastcall*)(void*,int,void**,int);           // ISteamNetworkingMessages: (this, local channel, messages, most)
using ReadP2P=bool(__fastcall*)(void*,void*,uint32_t,uint32_t*,uint64_t*,int);   // ISteamNetworking: (this, destination, size, out size, out sender, channel)
inline ReceiveList originalPoll=nullptr,originalConnection=nullptr;inline ReceiveChannel originalChannel=nullptr;inline ReadP2P originalReadP2P=nullptr;
inline std::vector<void*> gameHooks;
inline void noteMessages(void** list,int count){for(int i=0;i<count&&i<4096;i++){const auto message=reinterpret_cast<uintptr_t>(list[size_t(i)]);if(!message)continue;
    try{notePeer(api.identityGet(reinterpret_cast<void*>(message+16)));}catch(...){}}}
int __fastcall receivePoll(void* self_,uint32_t group,void** list,int most){const int n=originalPoll(self_,group,list,most);if(n>0&&list)noteMessages(list,n);return n;}
int __fastcall receiveConnection(void* self_,uint32_t connection,void** list,int most){const int n=originalConnection(self_,connection,list,most);if(n>0&&list)noteMessages(list,n);return n;}
int __fastcall receiveChannel(void* self_,int localChannel,void** list,int most){const int n=originalChannel(self_,localChannel,list,most);if(n>0&&list&&localChannel!=channel)noteMessages(list,n);return n;}
bool __fastcall readP2P(void* self_,void* destination,uint32_t size,uint32_t* outSize,uint64_t* sender,int p2pChannel){const bool got=originalReadP2P(self_,destination,size,outSize,sender,p2pChannel);if(got&&sender)notePeer(*sender);return got;}
inline void hookGame(HMODULE module) noexcept {
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SYNC_GAME_PEERS",value,sizeof value);if(n==1&&value[0]=='0'){logFile->event("steam_game_peers_off","\"reason\":\"EVR_SYNC_GAME_PEERS=0\"");return;}
        const auto get=[&](const char* name){const auto p=GetProcAddress(module,name);if(!p)throw std::runtime_error(std::string("missing ")+name);return reinterpret_cast<void*>(p);};
        // The thin exported functions are "mov rax,[rcx]; jmp [rax+offset]": the offset in the interface's table.
        const auto slot=[&](const char* name){const auto p=reinterpret_cast<const uint8_t*>(get(name));
            if(p[0]==0x48&&p[1]==0x8b&&p[2]==0x01&&p[3]==0x48&&p[4]==0xff&&p[5]==0x60)return unsigned(p[6])/8u;                   // jmp qword ptr [rax+disp8]
            if(p[0]==0x48&&p[1]==0x8b&&p[2]==0x01&&p[3]==0x48&&p[4]==0xff&&p[5]==0xa0){uint32_t d=0;std::memcpy(&d,p+6,4);return d/8u;}   // disp32
            if(p[0]==0x48&&p[1]==0x8b&&p[2]==0x01&&p[3]==0x4c&&p[4]==0x8b&&p[5]==0x50)return unsigned(p[6])/8u;                   // mov r10,[rax+disp8]; jmp r10
            // 1.0's steam_api64.dll (ReadP2PPacket): mov r10,[rcx]; jmp qword ptr [r10+disp8 / disp32]
            if(p[0]==0x4c&&p[1]==0x8b&&p[2]==0x11&&p[3]==0x49&&p[4]==0xff&&p[5]==0x62)return unsigned(p[6])/8u;
            if(p[0]==0x4c&&p[1]==0x8b&&p[2]==0x11&&p[3]==0x49&&p[4]==0xff&&p[5]==0xa2){uint32_t d=0;std::memcpy(&d,p+6,4);return d/8u;}
            throw std::runtime_error(std::string(name)+": unexpected thunk");};
        using Accessor=void*(*)();
        // The interface versions of the steam_api64.dll beside the game: 1.0 (2026-10-16) brought a newer one (sockets v013,
        // friends v018, utils v011; before v012 / v017 / v010) and nothing of the online link started ("missing
        // SteamAPI_SteamFriends_v017", headset run 20261016-134128). The newer name first, the older one after it.
        struct Want {const char* accessor;const char* thunk;void* detour;void** original;const char* label;const char* older=nullptr;};
        const Want wants[]={
            {"SteamAPI_SteamNetworkingSockets_SteamAPI_v013","SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnPollGroup",reinterpret_cast<void*>(&receivePoll),reinterpret_cast<void**>(&originalPoll),"sockets poll group","SteamAPI_SteamNetworkingSockets_SteamAPI_v012"},
            {"SteamAPI_SteamNetworkingSockets_SteamAPI_v013","SteamAPI_ISteamNetworkingSockets_ReceiveMessagesOnConnection",reinterpret_cast<void*>(&receiveConnection),reinterpret_cast<void**>(&originalConnection),"sockets connection","SteamAPI_SteamNetworkingSockets_SteamAPI_v012"},
            {"SteamAPI_SteamNetworkingMessages_SteamAPI_v002","SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel",reinterpret_cast<void*>(&receiveChannel),reinterpret_cast<void**>(&originalChannel),"messages channel"},
            {"SteamAPI_SteamNetworking_v006","SteamAPI_ISteamNetworking_ReadP2PPacket",reinterpret_cast<void*>(&readP2P),reinterpret_cast<void**>(&originalReadP2P),"p2p packet"}};
        std::string hooked;
        for(const auto& w:wants){
            const auto accessor=w.older&&!GetProcAddress(module,w.accessor)?w.older:w.accessor;
            const auto object=reinterpret_cast<Accessor>(get(accessor))();if(!object)throw std::runtime_error(std::string(accessor)+" gave no interface");
            const auto table=read<uintptr_t>(reinterpret_cast<uintptr_t>(object));const auto index=slot(w.thunk);
            const auto target=read<uintptr_t>(table+uintptr_t(index)*8);if(!target)throw std::runtime_error(std::string(w.thunk)+": empty table entry");
            if(MH_CreateHook(reinterpret_cast<void*>(target),w.detour,w.original)!=MH_OK||MH_EnableHook(reinterpret_cast<void*>(target))!=MH_OK)throw std::runtime_error(std::string(w.label)+": hook failed");
            gameHooks.push_back(reinterpret_cast<void*>(target));hooked+=std::string(hooked.empty()?"":",")+"\""+w.label+"\"";
        }
        logFile->event("steam_game_peers_hooked","\"hooks\":["+hooked+"],\"read_only\":true,\"messages_untouched\":true");
    }catch(const std::exception& e){try{logFile->event("steam_game_peers_failure","\"reason\":"+jsonString(e.what()));}catch(...){}}
}
inline void disableGameHooks() noexcept {
    for(const auto target:gameHooks)MH_DisableHook(target);
    if(!gameHooks.empty())try{logFile->event("steam_game_peers_removed","\"hooks\":"+std::to_string(gameHooks.size())+",\"messages_seen\":"+std::to_string(peerMessages.load()));}catch(...){}
    gameHooks.clear();
}
inline bool start() noexcept {
    if(tried)return ready;tried=true;
    try{
        char value[8]{};const auto n=GetEnvironmentVariableA("EVR_SYNC_SELF",value,sizeof value);
        if(n&&n<sizeof value&&value[0]=='2'){inProcess=true;self=1;ready=true;logFile->event("steam_link_enabled","\"in_process\":true,\"steam\":false");return true;}
        const auto module=GetModuleHandleW(L"steam_api64.dll");if(!module)throw std::runtime_error("steam_api64.dll is not loaded");
        const auto get=[&](const char* name){const auto p=GetProcAddress(module,name);if(!p)throw std::runtime_error(std::string("missing ")+name);return reinterpret_cast<void*>(p);};
        #define EVR_STEAM(field,name) api.field=reinterpret_cast<decltype(api.field)>(get(name))
        const auto either=[&](const char* newer,const char* older){return GetProcAddress(module,newer)?newer:older;};   // 1.0: see the game peers above
        EVR_STEAM(messages,"SteamAPI_SteamNetworkingMessages_SteamAPI_v002");EVR_STEAM(user,"SteamAPI_SteamUser_v023");EVR_STEAM(friends,either("SteamAPI_SteamFriends_v018","SteamAPI_SteamFriends_v017"));
        EVR_STEAM(utils,either("SteamAPI_SteamUtils_v011","SteamAPI_SteamUtils_v010"));EVR_STEAM(matchmaking,"SteamAPI_SteamMatchmaking_v009");EVR_STEAM(hUser,"SteamAPI_GetHSteamUser");
        EVR_STEAM(send,"SteamAPI_ISteamNetworkingMessages_SendMessageToUser");EVR_STEAM(receive,"SteamAPI_ISteamNetworkingMessages_ReceiveMessagesOnChannel");
        EVR_STEAM(release,"SteamAPI_SteamNetworkingMessage_t_Release");EVR_STEAM(identityClear,"SteamAPI_SteamNetworkingIdentity_Clear");
        EVR_STEAM(identitySet,"SteamAPI_SteamNetworkingIdentity_SetSteamID64");EVR_STEAM(identityGet,"SteamAPI_SteamNetworkingIdentity_GetSteamID64");
        EVR_STEAM(steamId,"SteamAPI_ISteamUser_GetSteamID");EVR_STEAM(appId,"SteamAPI_ISteamUtils_GetAppID");
        EVR_STEAM(friendCount,"SteamAPI_ISteamFriends_GetFriendCount");EVR_STEAM(friendAt,"SteamAPI_ISteamFriends_GetFriendByIndex");EVR_STEAM(friendGame,"SteamAPI_ISteamFriends_GetFriendGamePlayed");
        EVR_STEAM(lobbyMembers,"SteamAPI_ISteamMatchmaking_GetNumLobbyMembers");EVR_STEAM(lobbyMember,"SteamAPI_ISteamMatchmaking_GetLobbyMemberByIndex");
        EVR_STEAM(coplayCount,"SteamAPI_ISteamFriends_GetCoplayFriendCount");EVR_STEAM(coplayAt,"SteamAPI_ISteamFriends_GetCoplayFriend");EVR_STEAM(coplayTime,"SteamAPI_ISteamFriends_GetFriendCoplayTime");EVR_STEAM(coplayGame,"SteamAPI_ISteamFriends_GetFriendCoplayGame");
        #undef EVR_STEAM
        if(!api.hUser())throw std::runtime_error("Steam is not initialised in the game");
        messages=api.messages();const auto user=api.user(),utils=api.utils();
        if(!messages||!user||!utils||!api.friends()||!api.matchmaking())throw std::runtime_error("a Steam interface is missing");
        self=api.steamId(user);app=api.appId(utils);
        if(!self||!app)throw std::runtime_error("no Steam user or application");
        ready=true;
        logFile->event("steam_link_enabled","\"in_process\":false,\"steam\":true,\"application\":"+std::to_string(app)+",\"player\":\""+tail(self)+"\",\"channel\":"+std::to_string(channel)+",\"send_flags\":"+std::to_string(sendFlags)+
            ",\"game_peers_by_hooks\":true");
        hookGame(module);
    }catch(const std::exception& e){ready=false;try{logFile->event("steam_link_unavailable","\"reason\":"+jsonString(e.what()));}catch(...){}}
    return ready;
}
// Friends in this game now, with them everybody in a lobby this player is in, the players the game itself heard from
// within the last ten seconds, and the players Steam lists as played with in this game within the last fifteen
// minutes (ISteamFriends coplay: kept by Steam when the game tells it of its players - whether this game does is not
// known; a stranger's dedicated server would come to the mod this way, or by another player's roster).
inline std::vector<uint64_t> playersHere(){
    std::vector<uint64_t> out;if(!ready||inProcess)return out;
    for(const auto id:gamePeersWithin(10000))if(out.size()<64)out.push_back(id);
    try{const auto friendsApi=api.friends();if(friendsApi&&api.coplayCount&&api.coplayAt&&api.coplayTime&&api.coplayGame){
        const int coplay=std::clamp(api.coplayCount(friendsApi),0,256);const auto unixNow=int64_t(std::time(nullptr));
        for(int i=0;i<coplay&&out.size()<64;i++){const auto id=api.coplayAt(friendsApi,i);if(!id||id==self||((id>>52)&0xf)!=1)continue;
            if(api.coplayGame(friendsApi,id)!=app)continue;const auto at=int64_t(api.coplayTime(friendsApi,id));
            if(at<=0||unixNow-at>900)continue;
            if(std::find(out.begin(),out.end(),id)==out.end())out.push_back(id);}}}catch(...){}
    const auto friends=api.friends(),matchmaking=api.matchmaking();if(!friends||!matchmaking)return out;
    std::vector<uint64_t> lobbies;constexpr int immediate=4;
    const int count=std::clamp(api.friendCount(friends,immediate),0,4000);
    for(int i=0;i<count&&out.size()<64;i++){const auto id=api.friendAt(friends,i,immediate);FriendGame game{};
        if(!id||id==self||!api.friendGame(friends,id,&game)||(game.game&0xffffff)!=app)continue;
        out.push_back(id);if(game.lobby&&std::find(lobbies.begin(),lobbies.end(),game.lobby)==lobbies.end()&&lobbies.size()<8)lobbies.push_back(game.lobby);}
    for(const auto lobby:lobbies){const int members=api.lobbyMembers(matchmaking,lobby);if(members<=0||members>64)continue;
        std::vector<uint64_t> ids;bool mine=false;
        for(int i=0;i<members;i++){const auto id=api.lobbyMember(matchmaking,lobby,i);if(id==self)mine=true;else if(id)ids.push_back(id);}
        if(mine)for(const auto id:ids)if(std::find(out.begin(),out.end(),id)==out.end()&&out.size()<64)out.push_back(id);}
    return out;
}
inline bool send(uint64_t to,const std::vector<uint8_t>& bytes){
    if(!ready||bytes.empty()||bytes.size()>mostBytes)return false;
    if(inProcess){if(to==self&&loop.size()<256)loop.push_back(bytes);++sent;sentBytes+=bytes.size();return true;}
    alignas(8) std::array<uint8_t,136> identity{};api.identityClear(identity.data());api.identitySet(identity.data(),to);
    lastSendResult=api.send(messages,identity.data(),bytes.data(),uint32_t(bytes.size()),sendFlags,channel);
    if(lastSendResult!=1){++sendFailures;return false;}
    ++sent;sentBytes+=bytes.size();return true;
}
// Everything that arrived on the mod's channel since the last call: handler(from, bytes, size).
template<class Handler> inline unsigned receive(Handler&& handler){
    if(!ready)return 0;unsigned taken=0;
    if(inProcess){while(!loop.empty()&&taken<256){const auto bytes=std::move(loop.front());loop.pop_front();++received;receivedBytes+=bytes.size();handler(self,bytes.data(),bytes.size());++taken;}return taken;}
    for(unsigned round=0;round<8;round++){
        std::array<void*,32> list{};const int count=api.receive(messages,channel,list.data(),int(list.size()));
        if(count<=0)break;
        for(int i=0;i<count&&i<int(list.size());i++){const auto message=reinterpret_cast<uintptr_t>(list[size_t(i)]);if(!message)continue;
            // SteamNetworkingMessage_t: +0 the data, +8 its size, +16 the sender's identity.
            const auto data=read<uintptr_t>(message);const auto size=read<int32_t>(message+8);
            const auto from=api.identityGet(reinterpret_cast<void*>(message+16));
            if(data&&size>0&&uint32_t(size)<=mostBytes&&from){++received;receivedBytes+=uint64_t(size);
                std::array<uint8_t,mostBytes> copy{};std::memcpy(copy.data(),reinterpret_cast<const void*>(data),size_t(size));
                try{handler(from,copy.data(),size_t(size));}catch(...){}
                ++taken;}
            else ++oversized;
            api.release(list[size_t(i)]);
        }
        if(count<int(list.size()))break;
    }
    return taken;
}
}
