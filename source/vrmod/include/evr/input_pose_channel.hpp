#pragma once
#include <evr/game_input.hpp>
#include <mutex>

namespace evr::game {
// Value snapshots only. No pointer into ECS, no lock held while calling native
// game/OpenXR functions. Clock units are caller-owned monotonic milliseconds;
// XrTime is opaque and must never be compared with this clock.
struct InputPoseToken {
    uint64_t session=0,origin=0;
    bool operator==(const InputPoseToken& o)const{return session==o.session&&origin==o.origin;}
};
enum class InputPoseStatus {Current,Empty,WrongEntity,ClockReversed,StaleHead,StaleRoot};
inline const char* poseStatusName(InputPoseStatus s){
    switch(s){
    case InputPoseStatus::Current:return "current";
    case InputPoseStatus::Empty:return "empty";
    case InputPoseStatus::WrongEntity:return "wrong_entity";
    case InputPoseStatus::ClockReversed:return "clock_reversed";
    case InputPoseStatus::StaleHead:return "stale_head";
    case InputPoseStatus::StaleRoot:return "stale_root";
    }return "empty";
}
struct InputPoseRead {
    HeadInputSnapshot head;
    InputPoseToken token;
    InputPoseStatus status=InputPoseStatus::Empty;
    uint64_t headAgeMs=0,rootAgeMs=0;
    uint32_t entity=0;
};
class InputPoseChannel {
    mutable std::mutex mutex_;
    InputPoseToken token_;
    HeadInputSnapshot value_;
    uint64_t frame_=0,headMs_=0,rootMs_=0;
    uint32_t entity_=0;
    bool available_=false;
public:
    // Reset on session/recenter, before publishing its first frame. A late
    // producer holding a previous token cannot revive an earlier generation.
    void reset(InputPoseToken token){
        if(!token.session||!token.origin)throw std::invalid_argument("zero pose generation");
        std::lock_guard<std::mutex> lock(mutex_);
        if(token==token_)throw std::invalid_argument("pose generation must change on reset");
        token_=token;value_={};frame_=headMs_=rootMs_=0;entity_=0;available_=false;
    }
    void invalidate(){std::lock_guard<std::mutex> lock(mutex_);available_=false;value_.current=false;}
    void close(){std::lock_guard<std::mutex> lock(mutex_);token_={};value_={};available_=false;}
    bool publish(InputPoseToken token,uint32_t entity,const HeadInputSnapshot& head,uint64_t headMs,uint64_t rootMs){
        std::lock_guard<std::mutex> lock(mutex_);
        if(!(token==token_)||!entity||!token.session||!token.origin||head.originEpoch!=token.origin||
           !head.xrFrame||head.xrFrame<=frame_||head.predictedDisplayTime<=0||!headMs||!rootMs||
           rootMs>headMs||headMs<headMs_)return false;
        value_=head;value_.current=false;frame_=head.xrFrame;entity_=entity;headMs_=headMs;rootMs_=rootMs;
        available_=true;return true;
    }
    InputPoseRead read(uint32_t entity,uint64_t nowMs,uint64_t maxAgeMs)const{
        std::lock_guard<std::mutex> lock(mutex_);InputPoseRead out;out.token=token_;
        if(!available_)return out;
        out.entity=entity_;out.head=value_;out.head.current=false;
        if(!entity||entity!=entity_){out.status=InputPoseStatus::WrongEntity;return out;}
        if(nowMs<headMs_||nowMs<rootMs_){out.status=InputPoseStatus::ClockReversed;return out;}
        out.headAgeMs=nowMs-headMs_;out.rootAgeMs=nowMs-rootMs_;
        if(!maxAgeMs||out.headAgeMs>maxAgeMs){out.status=InputPoseStatus::StaleHead;return out;}
        if(out.rootAgeMs>maxAgeMs){out.status=InputPoseStatus::StaleRoot;return out;}
        out.status=InputPoseStatus::Current;out.head.current=true;return out;
    }
};
} // namespace evr::game
