#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>
namespace evr::motion {
// XInput-style value packet. Once its owner epoch stops, late worker writes
// cannot restore a nonzero attack; releasing a held attack advances the packet.
class EpochAttackPacket {
 std::mutex mutex_;std::atomic<uint64_t> value_{1ull<<8};bool stopped_=false;
 uint64_t set(uint8_t level){auto value=value_.load();if(uint8_t(value)!=level){value=(((value>>8)+1)<<8)|level;value_.store(value);}return value;}
public:
 uint64_t load()const{return value_.load();}
 uint64_t publish(uint8_t level){std::lock_guard<std::mutex> lock(mutex_);return set(stopped_?0:level);}
 uint64_t stop(){std::lock_guard<std::mutex> lock(mutex_);stopped_=true;return set(0);}
};
}
