#pragma once
#include <atomic>
#include <cstdint>
#include <stdexcept>
namespace evr::diagnostic {
// Controls evidence output ONLY. Never use this result to authorize or skip
// gameplay work. A test witness is a separate explicit diagnostic stream.
struct Observation {uint64_t ordinal=0;bool record=false,witness=false;};
class ObservationBudget {
 std::atomic<uint64_t> seen_{0};uint64_t limit_;bool witness_;
public:
 explicit ObservationBudget(uint64_t limit,bool witness=false):limit_(limit),witness_(witness){}
 // Configuration is immutable after hooks start; caller owns setup ordering.
 void configure(uint64_t limit,bool witness){if(seen_.load())throw std::logic_error("observation budget already in use");limit_=limit;witness_=witness;}
 Observation observe(){const auto before=seen_.fetch_add(1);const bool record=before<limit_;return {before+1,record,!record&&witness_};}
 uint64_t seen()const{return seen_.load();}
 uint64_t limit()const{return limit_;}
};
}
