#pragma once
#include <evr/body_ik.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <vector>
namespace evr::sync {
// One player's VR body for the other players, after VHVR's VRPlayerSync
// (docs/analysis/vhvr_spec_20261010/player_sync.md; what differs and why: docs/analysis/online_sync_20261013/notes.md).
// As VHVR: every player gives head, both hands and the pelvis its own solve arrived at - positions along the world's
// axes, relative to the character's position, orientations in the world (1.2) - the fingers, four flags and the pose of
// the held item in the hand; the receiver steps toward the newest one (4.2, 4.3) and solves the same IK on that
// player's model with the pelvis as it was sent (4.5).
// Not as VHVR, because no game object carries the data here (9.2): a sequence number (an older datagram that arrives
// late is dropped), a time limit (no data for a while: back to the game's animation), "the IK is paused" as a flag
// instead of a shorter packet (9.3), the step toward the newest data by time instead of by fixed tick (9.3), the
// fingers as the five curl amounts per hand the hands here are made from (hand_rig.hpp) instead of VHVR's twenty
// rotations per hand, and where the owner's character stands in the world - VHVR's data hangs on the player's own
// game object; here the receiver finds whose body a datagram is by that place (nearestCharacter). The unused
// velocities (4.4) are left out. Value-only: the datagrams travel in hooks/steam_link.hpp.
struct Body {
    bool ik=true;                              // false: the owner's IK is paused - nothing of the body counts
    Pose head=identityPose();                  // the headset (midpoint of the eyes)
    std::array<Pose,2> hand{identityPose(),identityPose()};   // the hand sockets as the body's IK takes them
    Pose pelvis=identityPose();                // the owner's solved pelvis
    std::array<hand::FingerCurl,2> fingers{};
    bool swim=false;                           // the legs hang from the pelvis as the animation has them (body_ik.hpp)
    bool bowDrawn=false,leftHanded=false,reverseGrip=false;uint8_t twoHand=0;   // VHVR's four flags (1.2, 5.1-5.7)
    bool item=false;Pose itemInHand=identityPose();            // the main hand's held item relative to that hand's socket (5.5)
    bool offhandItem=false;Pose offhandInHand=identityPose();  // the off hand's (shield, the pair's left dagger), the same way (2026-10-15)
};
// The held item's pose in its hand and back: the hand socket and the item as they stand in one space (the owner's
// world about its character, the receiver's about its copy); the relation is the same in every space.
inline Pose inHand(const Pose& hand,const Pose& itemWorld){return compose(inverse(hand),itemWorld);}
inline Pose fromHand(const Pose& hand,const Pose& inHand){return compose(hand,inHand);}
using Place=std::array<int32_t,3>;             // the character's position in the world, millimetres
inline constexpr uint8_t magic=0xe5,version=3;   // 2 (2026-10-15): the off-hand item's pose may follow the main hand's
// 3 (2026-10-20): a body datagram ends with the sender's clock (milliseconds, 16 bit) - the receiver places the poses
// on the sender's time and shows them a little late, between two of them (Receiver). Version 2 is still read.
inline constexpr uint8_t oldVersion=2;inline constexpr size_t stampBytes=2;
inline constexpr size_t headerBytes=6,placeBytes=12,poseBytes=13,bodyAt=headerBytes+placeBytes,helloBytes=headerBytes,
    pausedBytes=bodyAt+poseBytes,fullBytes=bodyAt+4*poseBytes+10,itemBytes=poseBytes;
inline constexpr float positionUnit=1/2000.f;  // half a millimetre; +-16 m about the character
namespace detail {
inline void put16(std::vector<uint8_t>& out,int v){out.push_back(uint8_t(v&255));out.push_back(uint8_t((v>>8)&255));}
inline int get16(const uint8_t* p){return int16_t(uint16_t(p[0])|uint16_t(p[1])<<8);}
// A unit quaternion as its three smaller parts (the largest one follows from them, taken positive).
inline void putPose(std::vector<uint8_t>& out,const Pose& p){
    for(float v:{p.position.x,p.position.y,p.position.z})put16(out,int(std::lround(std::clamp(v/positionUnit,-32767.f,32767.f))));
    const auto q=normalized(p.orientation);const float parts[4]{q.x,q.y,q.z,q.w};unsigned largest=0;
    for(unsigned n=1;n<4;n++)if(std::abs(parts[n])>std::abs(parts[largest]))largest=n;
    const float sign=parts[largest]<0?-1.f:1.f;out.push_back(uint8_t(largest));
    for(unsigned n=0;n<4;n++)if(n!=largest)put16(out,int(std::lround(std::clamp(parts[n]*sign*46340.f,-32767.f,32767.f))));   // |part| <= 0.7072
}
inline bool getPose(const uint8_t* p,Pose& out){
    out.position={get16(p)*positionUnit,get16(p+2)*positionUnit,get16(p+4)*positionUnit};
    const unsigned largest=p[6];if(largest>3)return false;
    float parts[4]{};float sum=0;unsigned at=7;
    for(unsigned n=0;n<4;n++)if(n!=largest){parts[n]=get16(p+at)/46340.f;sum+=parts[n]*parts[n];at+=2;}
    if(sum>1.0001f)return false;
    parts[largest]=std::sqrt(std::max(0.f,1-sum));out.orientation=normalized(Quat{parts[0],parts[1],parts[2],parts[3]});return true;
}
inline void header(std::vector<uint8_t>& out,uint16_t sequence,uint8_t flags,uint8_t twoHand){out.push_back(magic);out.push_back(version);put16(out,int(sequence));out.push_back(flags);out.push_back(twoHand);}
}
// Datagrams: magic, version, sequence (16 bit), flags, two-hand state - that alone, with the "hello" flag, says "a
// player with this mod is here" (6 bytes); otherwise the character's place, the head, then, unless paused, left hand,
// right hand, pelvis and ten finger curls, then the held items' poses where there are any (the main hand's by the
// flag 32, the off hand's by bit 2 of the two-hand byte): 31, 80, 93 or 106 bytes.
inline std::vector<uint8_t> encodeHello(uint16_t sequence){std::vector<uint8_t> out;detail::header(out,sequence,0x40,0);return out;}
inline std::vector<uint8_t> encode(const Body& b,uint16_t sequence,const Place& place,uint16_t sentMs=0){
    if(!finite(b.head.position)||(b.ik&&(!finite(b.hand[0].position)||!finite(b.hand[1].position)||!finite(b.pelvis.position)||(b.item&&!finite(b.itemInHand.position))||(b.offhandItem&&!finite(b.offhandInHand.position)))))
        throw std::invalid_argument("player sync body");
    std::vector<uint8_t> out;out.reserve(fullBytes+2*itemBytes);
    detail::header(out,sequence,uint8_t((b.ik?1:0)|(b.bowDrawn?2:0)|(b.leftHanded?4:0)|(b.reverseGrip?8:0)|(b.swim?16:0)|(b.ik&&b.item?32:0)),uint8_t((b.twoHand>2?0:b.twoHand)|(b.ik&&b.offhandItem?4:0)));
    for(const auto v:place)for(unsigned n=0;n<4;n++)out.push_back(uint8_t(uint32_t(v)>>(8*n)));
    detail::putPose(out,b.head);
    if(b.ik){
        detail::putPose(out,b.hand[0]);detail::putPose(out,b.hand[1]);detail::putPose(out,b.pelvis);
        for(const auto& hand:b.fingers)for(float v:hand.amount)out.push_back(uint8_t(std::lround(std::clamp(std::isfinite(v)?v:0.f,0.f,1.f)*255)));
        if(b.item)detail::putPose(out,b.itemInHand);
        if(b.offhandItem)detail::putPose(out,b.offhandInHand);
    }
    detail::put16(out,int(int16_t(sentMs)));
    return out;
}
// A roster (2026-10-15): a hello that carries the ids of the players with the mod this sender knows (heard from within
// a few seconds) - bit 3 of the two-hand byte, then a count and that many ids - so that two guests of one host, who
// hear only the host, learn of each other through it. Up to rosterMost ids; a receiver only sends hellos to them.
inline constexpr size_t rosterMost=16;
struct Received {Body body;uint16_t sequence=0;bool hello=false;Place place{};std::vector<uint64_t> roster;bool timed=false;uint16_t sentMs=0;};
inline std::vector<uint8_t> encodeRoster(uint16_t sequence,const std::vector<uint64_t>& ids){
    std::vector<uint8_t> out;detail::header(out,sequence,0x40,8);
    const size_t n=std::min(ids.size(),rosterMost);out.push_back(uint8_t(n));
    for(size_t i=0;i<n;i++)for(unsigned b=0;b<8;b++)out.push_back(uint8_t(ids[i]>>(8*b)));
    return out;
}
// Nothing for anything that is not exactly one datagram of this version: these bytes come from another machine.
inline std::optional<Received> decode(const uint8_t* data,size_t size){
    if(!data||size<helloBytes||data[0]!=magic||(data[1]!=version&&data[1]!=oldVersion))return std::nullopt;
    const size_t stamp=data[1]==version?stampBytes:0;
    const uint8_t flags=data[4];if(flags&0x80||(data[5]&3)==3||data[5]&~0xf)return std::nullopt;
    Received r;r.sequence=uint16_t(data[2])|uint16_t(data[3])<<8;
    if(flags&0x40){
        if(flags!=0x40)return std::nullopt;r.hello=true;r.body.ik=false;
        if(data[5]==0){if(size!=helloBytes)return std::nullopt;return r;}
        if(data[5]!=8||size<helloBytes+1)return std::nullopt;
        const size_t n=data[helloBytes];if(!n||n>rosterMost||size!=helloBytes+1+8*n)return std::nullopt;
        for(size_t i=0;i<n;i++){uint64_t id=0;for(unsigned b=0;b<8;b++)id|=uint64_t(data[helloBytes+1+8*i+b])<<(8*b);if(!id)return std::nullopt;r.roster.push_back(id);}
        return r;}
    if(data[5]&8)return std::nullopt;   // the roster bit belongs to a hello only
    auto& b=r.body;
    b.ik=flags&1;b.bowDrawn=flags&2;b.leftHanded=flags&4;b.reverseGrip=flags&8;b.swim=flags&16;b.item=flags&32;b.twoHand=data[5]&3;b.offhandItem=data[5]&4;
    if(size!=stamp+(b.ik?fullBytes+(b.item?itemBytes:0)+(b.offhandItem?itemBytes:0):pausedBytes)||(!b.ik&&(b.item||b.offhandItem)))return std::nullopt;
    if(stamp){r.timed=true;r.sentMs=uint16_t(data[size-2])|uint16_t(data[size-1])<<8;}
    for(unsigned n=0;n<3;n++){const auto* p=data+headerBytes+4*n;r.place[n]=int32_t(uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24);}
    if(!detail::getPose(data+bodyAt,b.head))return std::nullopt;
    if(!b.ik)return r;
    const auto* p=data+bodyAt+poseBytes;
    if(!detail::getPose(p,b.hand[0])||!detail::getPose(p+poseBytes,b.hand[1])||!detail::getPose(p+2*poseBytes,b.pelvis))return std::nullopt;
    p+=3*poseBytes;for(auto& hand:b.fingers)for(float& v:hand.amount)v=*p++/255.f;
    if(b.item){if(!detail::getPose(p,b.itemInHand))return std::nullopt;p+=poseBytes;}
    if(b.offhandItem&&!detail::getPose(p,b.offhandInHand))return std::nullopt;
    return r;
}
inline std::optional<Received> decode(const std::vector<uint8_t>& bytes){return decode(bytes.data(),bytes.size());}

// Actions (2026-10-20, the first online session with a VR guest): what a player's mod decided that the host's mod needs,
// because the host's server simulates the guest's character. Another magic, so that a mod of 0.9.3 refuses them as any
// datagram it does not know. magic 0xe6, version 1, kind, id (16 bit: the same action is sent a few times; the receiver
// takes it once), then the kind's bytes:
//   kind 1, a fishing cast: the strength of the swing at the trigger's release, 0..1 in 16 bits (0: no swing - the
//   shortest cast). The host gives the guest's cast that charge as its own casts get theirs (hooks/fishing_charge.hpp).
inline constexpr uint8_t actionMagic=0xe6,actionVersion=1,castAction=1;
inline constexpr size_t castActionBytes=7;
struct Action {uint8_t kind=0;uint16_t id=0;float strength=0;};
inline std::vector<uint8_t> encodeCast(uint16_t id,float strength){
    std::vector<uint8_t> out{actionMagic,actionVersion,castAction};detail::put16(out,int(id));
    const auto s=uint16_t(std::lround(std::clamp(std::isfinite(strength)?strength:0.f,0.f,1.f)*65535));out.push_back(uint8_t(s&255));out.push_back(uint8_t(s>>8));return out;
}
inline std::optional<Action> decodeAction(const uint8_t* data,size_t size){
    if(!data||size<3||data[0]!=actionMagic||data[1]!=actionVersion)return std::nullopt;
    Action a;a.kind=data[2];
    if(a.kind==castAction){if(size!=castActionBytes)return std::nullopt;a.id=uint16_t(data[3])|uint16_t(data[4])<<8;a.strength=float(uint16_t(data[5])|uint16_t(data[6])<<8)/65535.f;return a;}
    return std::nullopt;
}

// The model's space <-> what is sent. The body's IK works in the character model's space (origin at the feet); what
// is sent lies along the world's axes about the character's position, so that the receiver's copy of the character
// may face another way than the owner's and the body still stands right in the world (VHVR 1.2).
struct Model {Quat orientation{0,0,0,1};Vec3 scale{1,1,1};};
inline Pose toWorld(const Model& m,const Pose& inModel){
    return {normalized(multiply(m.orientation,inModel.orientation)),rotate(m.orientation,{inModel.position.x*m.scale.x,inModel.position.y*m.scale.y,inModel.position.z*m.scale.z})};}
inline Pose toModel(const Model& m,const Pose& inWorld){const auto p=rotate(inverse(m.orientation),inWorld.position);
    return {normalized(multiply(inverse(m.orientation),inWorld.orientation)),{p.x/m.scale.x,p.y/m.scale.y,p.z/m.scale.z}};}
// The owner's side: what its solve was given and what it gave (bodyik::Targets, bodyik::Report::hips), all in its model's space.
inline Body fromSolve(const Model& m,const bodyik::Targets& t,const Pose& solvedHips,const std::array<hand::FingerCurl,2>& fingers){
    Body b;b.head=toWorld(m,t.head);b.hand={toWorld(m,t.socket[0]),toWorld(m,t.socket[1])};b.pelvis=toWorld(m,solvedHips);b.fingers=fingers;b.swim=t.swim;return b;}
// The receiver's side: the targets for the same solve on that player's model (the pelvis as it was sent).
inline bodyik::Targets targets(const Model& m,const Body& b){
    bodyik::Targets t;t.head=toModel(m,b.head);t.socket={toModel(m,b.hand[0]),toModel(m,b.hand[1])};t.swim=b.swim;t.hipsGiven=true;t.hips=toModel(m,b.pelvis);return t;}
// World positions in metres <-> the place in a datagram.
inline Place place(const std::array<double,3>& metres){Place p{};for(unsigned n=0;n<3;n++)p[n]=int32_t(std::llround(std::clamp(metres[n]*1000,-2.1e9,2.1e9)));return p;}
inline std::array<double,3> metres(const Place& p){return {p[0]/1000.,p[1]/1000.,p[2]/1000.};}

// Whose body a datagram is: the other character that stands where the sender says its own one stands. The receiver's
// copy of that character is behind by the game's own smoothing (a running player: a metre and more), so "where" is
// wide - and none is taken while a second character is about as near (two players in one spot). -1: none.
inline double distance(const std::array<double,3>& a,const std::array<double,3>& b){return std::sqrt((a[0]-b[0])*(a[0]-b[0])+(a[1]-b[1])*(a[1]-b[1])+(a[2]-b[2])*(a[2]-b[2]));}
inline int nearestCharacter(const std::array<double,3>& at,const std::vector<std::array<double,3>>& characters,double within=3,double clearBy=1){
    int best=-1;double first=1e300,second=1e300;
    for(size_t i=0;i<characters.size();i++){const double d=distance(at,characters[i]);if(d<first){second=first;first=d;best=int(i);}else if(d<second)second=d;}
    return best>=0&&first<=within&&second-first>=clearBy?best:-1;
}
// ... and from then on that character for as long as the sender stays about it: two players who walk into each other
// keep their bodies. previous: the character taken before, or -1.
// A sender bound to a character whose place then stands farther from it than keepWithin (a teleport, or a place that
// is not its own): nothing this time - the binding is dropped and the next datagram's place is looked at anew
// (2026-10-15, spec 2.7: such data is not put on any character).
inline int boundCharacter(int previous,const std::array<double,3>& at,const std::vector<std::array<double,3>>& characters,double within=3,double clearBy=1,double keepWithin=6){
    if(previous>=0&&size_t(previous)<characters.size())return distance(at,characters[size_t(previous)])<=keepWithin?previous:-1;
    return nearestCharacter(at,characters,within,clearBy);
}

// Who gets the body every frame (2026-10-15, spec 2.5): of the players within sight, the nearest `most`; the rest, like
// those out of sight, get the once-a-second datagram. Indices into `places`, nearest first; a player without a
// known place is not among them. Fewer than `most` within sight: all of them.
inline std::vector<size_t> bodyRecipients(const std::array<double,3>& mine,const std::vector<std::array<double,3>>& places,const std::vector<char>& known,double within,size_t most=12){
    std::vector<std::pair<double,size_t>> close;
    for(size_t i=0;i<places.size();i++){if(i<known.size()&&!known[i])continue;const double d=distance(mine,places[i]);if(d<=within)close.push_back({d,i});}
    std::sort(close.begin(),close.end());if(close.size()>most)close.resize(most);
    std::vector<size_t> out;for(const auto& n:close)out.push_back(n.second);return out;
}
// One other player, on the receiving side. Since 2026-10-20 (buffered, the default) the poses are shown a little late,
// between the two received around that time (user, after the first session with a VR guest: "the other player's arms
// move little and slowly - a stroke at an enemy looks like stroking it"). VHVR's way below - a fifth of the way toward
// the newest data each 20 ms - is a low-pass of about 90 ms: a swing back and forth in 0.4 s keeps 58 % of its reach and
// comes 60 ms late (2.5 Hz through 1/sqrt(1+(2 pi f tau)^2)). Buffered: a pose is placed on the sender's clock (version 3,
// the receiver's arrival time for version 2) moved by the smallest transit seen, and shown at now - delay, where the
// delay is 1.5 times the usual spacing of the poses plus 10 ms (30..150 ms): the full reach, a steady small lag.
// buffered=false (EVR_SYNC_SMOOTHING=vhvr): VHVR's way. VHVR keeps the newest data and steps a fifth of the way toward
// it on every fixed tick (4.2, 4.3; a difference of a millimetre or of a thousandth of a degree is not stepped); here the
// same share per 20 ms of the time that passed. Fingers, flags and the held item are taken as they come (4.7; VHVR steps
// the item's position too - left for when items are shown). The first data, and the first after a pause or after the
// time limit, is taken as it is: VHVR starts its orientations from none and turns toward the first data for some ticks.
struct ReceiverSettings {
    float sharePerTick=.2f,tick=.02f;         // VHVR: 20 % per fixed tick; the tick's length is not in VHVR's source (50 Hz noted there)
    float minimumMove=.001f;                  // m
    float minimumTurn=.001f*pi/180;           // rad
    double timeLimit=3;                       // s without data: nothing is shown for that player any more (not in VHVR)
    unsigned mostPerSecond=200;               // datagrams of one player in a second; the rest of that second is ignored (2026-10-15, spec 2.7)
    bool buffered=true;                       // 2026-10-20: shown late between two received poses (false: VHVR's stepping)
    double delayIntervals=1.5,delayExtra=.01,delayMin=.03,delayMax=.15;   // s: the lag of the buffered way
    double transitRise=.0005;                 // s a pose: how fast the smallest transit may grow again (a slower route)
};
class Receiver {
    bool have_=false,shown_=false;Body newest_{},shownBody_{};uint16_t sequence_=0;double arrived_=0,stepped_=0;
    // buffered: the poses received lately, oldest first, each at its time on the sender's clock (seconds); a pose is shown
    // at that time + transit_ (the smallest arrival - sent seen) + the delay.
    struct Sample {Body body;double sent=0;};
    std::array<Sample,32> samples_{};size_t count_=0;
    double transit_=0,spacing_=1/30.;bool clockKnown_=false;int64_t sentClock_=0;uint16_t lastStamp_=0;
    // The turn from one to the other, taken from its vector part: the angle of a small turn does not come out of the
    // four-part product in single precision (a hundredth of a degree is lost in the rounding of a value next to 1).
    static Quat toward(Quat from,Quat to,float share,float minimum){
        auto turn=multiply(inverse(normalized(from)),normalized(to));if(turn.w<0)turn={-turn.x,-turn.y,-turn.z,-turn.w};
        const float v=std::sqrt(turn.x*turn.x+turn.y*turn.y+turn.z*turn.z),angle=2*std::atan2(v,turn.w);
        if(!(angle>minimum)||v<1e-12f)return to;
        const float half=share*angle/2,k=std::sin(half)/v;
        return normalized(multiply(from,Quat{turn.x*k,turn.y*k,turn.z*k,std::cos(half)}));
    }
    static Pose toward(const Pose& from,const Pose& to,float share,const ReceiverSettings& s){
        const auto way=to.position-from.position;
        return {toward(from.orientation,to.orientation,share,s.minimumTurn),length(way)>s.minimumMove?from.position+way*share:to.position};
    }
    // Between two poses a share t of the way (positions straight, turns along the shorter way).
    static Pose between(const Pose& a,const Pose& b,float t){
        auto q=normalized(b.orientation);const auto p=normalized(a.orientation);
        if(p.x*q.x+p.y*q.y+p.z*q.z+p.w*q.w<0)q={-q.x,-q.y,-q.z,-q.w};
        return {normalized(Quat{p.x+(q.x-p.x)*t,p.y+(q.y-p.y)*t,p.z+(q.z-p.z)*t,p.w+(q.w-p.w)*t}),a.position+(b.position-a.position)*t};
    }
    static Body between(const Body& a,const Body& b,float t){
        if(!a.ik||!b.ik)return b;
        Body out=b;out.head=between(a.head,b.head,t);out.pelvis=between(a.pelvis,b.pelvis,t);
        for(unsigned h=0;h<2;h++){out.hand[h]=between(a.hand[h],b.hand[h],t);
            for(size_t f=0;f<out.fingers[h].amount.size();f++)out.fingers[h].amount[f]=a.fingers[h].amount[f]+(b.fingers[h].amount[f]-a.fingers[h].amount[f])*t;}
        if(a.item&&b.item)out.itemInHand=between(a.itemInHand,b.itemInHand,t);
        if(a.offhandItem&&b.offhandItem)out.offhandInHand=between(a.offhandInHand,b.offhandInHand,t);
        return out;
    }
    void clear(){count_=0;clockKnown_=false;spacing_=1/30.;}
    void keep(const Received& r,double now,const ReceiverSettings& s){
        double sent=now;
        if(r.timed){
            if(!clockKnown_){sentClock_=r.sentMs;clockKnown_=true;transit_=0;}else sentClock_+=int16_t(uint16_t(r.sentMs-lastStamp_));
            lastStamp_=r.sentMs;const double at=double(sentClock_)/1000,transit=now-at;
            transit_=count_?std::min(transit,transit_+s.transitRise):transit;sent=at;
        }else{transit_=0;clockKnown_=false;}
        if(count_){const double gap=sent-samples_[count_-1].sent;
            if(gap<=0||gap>s.timeLimit){clear();if(r.timed){sentClock_=r.sentMs;clockKnown_=true;lastStamp_=r.sentMs;sent=double(sentClock_)/1000;transit_=now-sent;}}
            else spacing_=.9*spacing_+.1*std::clamp(gap,.005,.2);}
        if(count_==samples_.size()){std::move(samples_.begin()+1,samples_.end(),samples_.begin());--count_;}
        samples_[count_++]={r.body,sent};
    }
public:
    // A datagram of that player that decode() took (not a hello). False: older than the newest one (or the same again) - dropped.
    bool offer(const Received& r,double now,const ReceiverSettings& s={}){
        if(r.hello)return false;
        if(have_&&now-arrived_<=s.timeLimit&&int16_t(uint16_t(r.sequence-sequence_))<=0)return false;
        if(!have_||now-arrived_>s.timeLimit)clear();
        newest_=r.body;sequence_=r.sequence;arrived_=now;have_=true;
        if(s.buffered)keep(r,now,s);
        return true;
    }
    // What to show of that player now; nothing before the first data and after the time limit.
    std::optional<Body> update(double now,const ReceiverSettings& s={}){
        if(!have_||now-arrived_>s.timeLimit){have_=shown_=false;clear();return std::nullopt;}
        if(s.buffered&&count_){
            // a pause shows at once (the others let go of the body), and so does the first pose after it
            if(!newest_.ik){shownBody_=newest_;shown_=true;return newest_;}
            const double delay=std::clamp(s.delayIntervals*spacing_+s.delayExtra,s.delayMin,s.delayMax);
            const double play=now-delay-transit_;   // on the sender's clock
            Body out;
            if(count_==1||play>=samples_[count_-1].sent)out=samples_[count_-1].body;
            else if(play<=samples_[0].sent)out=samples_[0].body;
            else{size_t i=1;while(i<count_&&samples_[i].sent<play)++i;
                const auto& a=samples_[i-1];const auto& b=samples_[i];
                out=between(a.body,b.body,float(std::clamp((play-a.sent)/std::max(b.sent-a.sent,1e-6),0.,1.)));}
            shownBody_=out;shown_=true;return out;
        }
        const bool fresh=!shown_||!shownBody_.ik||!newest_.ik;
        const double dt=std::max(0.,now-stepped_);stepped_=now;
        const float share=fresh?1.f:1-std::pow(1-std::clamp(s.sharePerTick,0.f,1.f),float(dt/std::max(s.tick,1e-4f)));
        Body out=newest_;
        if(!fresh){out.head=toward(shownBody_.head,newest_.head,share,s);out.pelvis=toward(shownBody_.pelvis,newest_.pelvis,share,s);
            for(unsigned h=0;h<2;h++)out.hand[h]=toward(shownBody_.hand[h],newest_.hand[h],share,s);}
        shownBody_=out;shown_=true;return out;
    }
    bool known()const{return have_;}
    const Body& newest()const{return newest_;}   // as it came, without the stepping (the hit of a stroke: online_sync.hpp)
    uint16_t sequence()const{return sequence_;}
    double delayShown(const ReceiverSettings& s={})const{return std::clamp(s.delayIntervals*spacing_+s.delayExtra,s.delayMin,s.delayMax);}
};

// Who is sent to and how often, and what came from whom. The players listed are the ones the transport found (friends
// in this game, members of the lobby). Each gets a hello once a second; one that was heard from within the last
// seconds - it runs this mod too - gets the body at the frame rate. Datagrams count only from the listed.
struct LinkSettings {
    double helloEvery=1;          // s
    double heardWithin=5;         // s since the last datagram of a player: it gets the body
    double bodyEvery=1/75.;       // s; no more often than this
    double forgetAfter=60;        // s unlisted and unheard: dropped
    unsigned most=16;
};
class Peers {
public:
    struct Peer {uint64_t id=0;bool listed=false;double heard=-1e18,hello=-1e18,sent=-1e18,unlisted=0;uint64_t taken=0,dropped=0,ignored=0;
        Receiver receiver;Place place{};bool placed=false;int64_t character=-1;double second=-1e18;unsigned inSecond=0;};
    enum class Due:uint8_t{Nothing,Hello,Body};
    // The transport's list of now. New players are added (up to the most), the others stay until forgotten.
    void list(const std::vector<uint64_t>& ids,double now,const LinkSettings& s={}){
        for(auto& p:peers_){const bool was=p.listed;p.listed=std::find(ids.begin(),ids.end(),p.id)!=ids.end();if(was&&!p.listed)p.unlisted=now;}
        for(const auto id:ids)if(id&&!find(id)&&peers_.size()<s.most){Peer p;p.id=id;p.listed=true;peers_.push_back(p);}
        peers_.erase(std::remove_if(peers_.begin(),peers_.end(),[&](const Peer& p){return !p.listed&&now-p.unlisted>s.forgetAfter&&now-p.heard>s.forgetAfter;}),peers_.end());
    }
    Peer* find(uint64_t id){for(auto& p:peers_)if(p.id==id)return &p;return nullptr;}
    // What to send that player now (and the time is noted): the body where there is one to send and the player was
    // heard from, a hello otherwise at its slower pace.
    Due due(Peer& p,double now,bool haveBody,const LinkSettings& s={}){
        if(!p.listed)return Due::Nothing;
        if(haveBody&&now-p.heard<=s.heardWithin){if(now-p.sent<s.bodyEvery)return Due::Nothing;p.sent=p.hello=now;return Due::Body;}
        if(now-p.hello<s.helloEvery)return Due::Nothing;p.hello=now;return Due::Hello;
    }
    // A datagram that came from that player. False: the player is not listed, or the body in it is older than the newest.
    bool heard(uint64_t id,const Received& r,double now,const ReceiverSettings& rs={}){
        auto* p=find(id);if(!p||!p->listed)return false;
        // More than mostPerSecond in a second from one player: the rest of that second is ignored - not heard from either.
        if(now-p->second>=1||now<p->second){p->second=now;p->inSecond=0;}
        if(++p->inSecond>rs.mostPerSecond){++p->ignored;return false;}
        p->heard=now;
        if(r.hello){++p->taken;return true;}
        if(!p->receiver.offer(r,now,rs)){++p->dropped;return false;}
        p->place=r.place;p->placed=true;++p->taken;return true;
    }
    std::vector<Peer>& all(){return peers_;}
private:
    std::vector<Peer> peers_;
};
} // namespace evr::sync
