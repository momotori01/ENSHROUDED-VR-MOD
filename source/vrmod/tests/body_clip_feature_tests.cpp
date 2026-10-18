#include <evr/body_clip_feature.hpp>
#include <cstring>
#include <iostream>
#include <functional>
using namespace evr;
int main(){
 unsigned passed=0;auto check=[&](bool v,const char* name){if(!v)throw std::runtime_error(name);++passed;std::cout<<"PASS "<<name<<'\n';};
 try{
 VkPhysicalDeviceFeatures2 core{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};core.features.samplerAnisotropy=VK_TRUE;
 VkPhysicalDeviceVulkan12Features tail{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};tail.timelineSemaphore=VK_TRUE;core.pNext=&tail;
 VkLayerDeviceCreateInfo b{VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO},a{VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO};a.pNext=&b;b.pNext=&core;
 a.function=VK_LAYER_LINK_INFO;b.function=VK_LOADER_DATA_CALLBACK;
 VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};info.pNext=&a;
 const auto originalCore=core;const auto originalTail=tail;const auto originalA=a,originalB=b;
 BodyClipFeaturePlan disabled(info,false,true);check(!disabled.changed()&&!disabled.enabled()&&disabled.info.pNext==info.pNext,"disabled preserves request");
 BodyClipFeaturePlan unsupported(info,true,false);check(!unsupported.changed()&&!unsupported.enabled(),"unsupported retains native device");
 BodyClipFeaturePlan plan(info,true,true);check(plan.changed()&&plan.enabled(),"supported ClipDistance enabled");
 const auto* ac=static_cast<const VkLayerDeviceCreateInfo*>(plan.info.pNext);const auto* bc=static_cast<const VkLayerDeviceCreateInfo*>(ac->pNext);
 const auto* cc=static_cast<const VkPhysicalDeviceFeatures2*>(bc->pNext);
 check(ac!=&a&&bc!=&b&&cc!=&core&&ac->function==a.function&&bc->function==b.function&&cc->pNext==&tail,"owned loader prefix with original extension tail");
 auto expected=core;expected.features.shaderClipDistance=VK_TRUE;check(!std::memcmp(cc,&expected,sizeof(expected)),"only requested core bit changes");
 check(!std::memcmp(&core,&originalCore,sizeof(core))&&!std::memcmp(&tail,&originalTail,sizeof(tail))&&!std::memcmp(&a,&originalA,sizeof(a))&&!std::memcmp(&b,&originalB,sizeof(b)),"all caller storage unchanged");
 core.features.shaderClipDistance=VK_TRUE;BodyClipFeaturePlan existing(info,true,true);check(!existing.changed()&&existing.enabled()&&existing.info.pNext==info.pNext,"already enabled preserves chain");core.features.shaderClipDistance=VK_FALSE;
 VkBaseInStructure unknown{VkStructureType(0x7ffffff0),reinterpret_cast<const VkBaseInStructure*>(&core)};info.pNext=&unknown;
 BodyClipFeaturePlan unknownPlan(info,true,true);check(!unknownPlan.changed()&&std::string(unknownPlan.status)=="unreviewed_prefix","unknown prefix safe fallback");
 info.pNext=&core;tail.pNext=&core;BodyClipFeaturePlan cycle(info,true,true);check(!cycle.changed()&&std::string(cycle.status)=="chain_cycle","cyclic chain rejected");tail.pNext=nullptr;
 VkPhysicalDeviceFeatures2 duplicate{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};tail.pNext=&duplicate;BodyClipFeaturePlan duplicates(info,true,true);check(!duplicates.changed()&&std::string(duplicates.status)=="duplicate_features2","duplicate core features rejected");tail.pNext=nullptr;
 VkPhysicalDeviceFeatures legacy{};legacy.geometryShader=VK_TRUE;info.pEnabledFeatures=&legacy;
 BodyClipFeaturePlan ambiguous(info,true,true);check(!ambiguous.changed()&&std::string(ambiguous.status)=="ambiguous_core_features","legacy and Features2 ambiguity rejected");
 info.pNext=&tail;BodyClipFeaturePlan old(info,true,true);check(old.changed()&&old.enabled()&&old.info.pNext==&tail&&old.info.pEnabledFeatures!=&legacy&&old.info.pEnabledFeatures->geometryShader&&!legacy.shaderClipDistance,"legacy feature copy preserves extensions");
 info.pEnabledFeatures=nullptr;BodyClipFeaturePlan empty(info,true,true);check(empty.changed()&&empty.enabled(),"empty core feature set supported");
 std::cout<<"RESULT "<<passed<<" PASS\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
