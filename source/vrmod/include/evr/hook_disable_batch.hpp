#pragma once
#include <MinHook.h>
#include <array>
#include <cstddef>
#include <vector>
namespace evr::diagnostic {
// Single-owner, single-use batch. Callers must already stop/drain callbacks and
// supply only their installed ENABLED hooks. This retains all trampolines.
// A successful bulk operation issues exact one-use receipts, never a blanket
// exemption for MH_ERROR_DISABLED from somebody else's teardown.
class HookDisableBatch {
 std::array<void*,32> pending_{};size_t count_=0;DWORD thread_=0;bool used_=false;
public:
 MH_STATUS prepare(const std::vector<void*>& targets) noexcept {
  if(used_||targets.empty()||targets.size()>pending_.size())return MH_ERROR_UNSUPPORTED_FUNCTION;
  for(size_t i=0;i<targets.size();++i){
   if(!targets[i])return MH_ERROR_UNSUPPORTED_FUNCTION;
   for(size_t j=0;j<i;++j)if(targets[i]==targets[j])return MH_ERROR_UNSUPPORTED_FUNCTION;
  }
  size_t queued=0;
  for(auto* target:targets){
   const auto status=MH_QueueDisableHook(target);
   if(status!=MH_OK){
    // Restore queued intentions to the required original ENABLED state. This
    // does not execute, enable or disable any code. Preserve a rollback error.
    auto result=status;for(size_t i=0;i<queued;++i){const auto undo=MH_QueueEnableHook(targets[i]);if(undo!=MH_OK)result=undo;}return result;
   }++queued;
  }
  used_=true;const auto status=MH_ApplyQueued();
  if(status!=MH_OK)return status; // No receipts for a partial/failed application.
  thread_=GetCurrentThreadId();count_=targets.size();
  for(size_t i=0;i<count_;++i)pending_[i]=targets[i];return MH_OK;
 }
 MH_STATUS disable(void* target) noexcept {
  for(auto& p:pending_)if(p&&p==target){
   if(GetCurrentThreadId()!=thread_)return MH_ERROR_UNSUPPORTED_FUNCTION;
   const auto status=MH_DisableHook(target);
   if(status!=MH_ERROR_DISABLED)return status==MH_OK?MH_ERROR_ENABLED:status;
   p=nullptr;--count_;return MH_OK;
  }
  return MH_DisableHook(target);
 }
 size_t pending()const noexcept{return count_;}
 bool complete()const noexcept{return used_&&thread_&&count_==0;}
};
}
