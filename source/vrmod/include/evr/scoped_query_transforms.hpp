#pragma once
#include <array>
#include <cstdint>
#include <stdexcept>
namespace evr::game {
// Borrow two stack query entries for ONE native entity iteration. Does not own
// or modify ECS data. Caller must close before the iterator advances or exits.
class ScopedQueryTransforms {
public:
 using Query=std::array<uintptr_t,10>;using Current=std::array<uint8_t,56>;using Previous=std::array<uint8_t,72>;
 ScopedQueryTransforms()=default;ScopedQueryTransforms(const ScopedQueryTransforms&)=delete;ScopedQueryTransforms& operator=(const ScopedQueryTransforms&)=delete;
 bool active()const{return query_!=nullptr;}
 void open(Query& query,const Current& current,const Previous& previous){
  if(active()||!query[1])throw std::logic_error("query transform borrow invalid");
  query_=&query;original_=query;current_=current;previous_=previous;
  query[1]=reinterpret_cast<uintptr_t>(current_.data());if(query[3])query[3]=reinterpret_cast<uintptr_t>(previous_.data());
 }
 bool close(Query& query){
  if(query_!=&query)throw std::logic_error("query transform return identity mismatch");
  auto expected=original_;expected[1]=reinterpret_cast<uintptr_t>(current_.data());if(expected[3])expected[3]=reinterpret_cast<uintptr_t>(previous_.data());
  const bool intact=query==expected;query[1]=original_[1];query[3]=original_[3];query_=nullptr;return intact;
 }
 const Query& original()const{return original_;}
 const Current& current()const{return current_;}
 const Previous& previous()const{return previous_;}
private:
 Query* query_=nullptr;Query original_{};Current current_{};Previous previous_{};
};
}
