#pragma once
#include <evr/math.hpp>
#include <cstdint>
#include <vector>
#include <algorithm>
#include <array>
#include <cmath>
namespace evr::hand {
struct alignas(16) Joint {Quat orientation{0,0,0,1};Vec3 position{};float positionPadding=0;Vec3 scale{1,1,1};float scalePadding=0;};
static_assert(sizeof(Joint)==48);
struct Branch {uint16_t root=0,wrist=0,socket=0;};
// Finger curl (2026-10-09 real-device feedback "指の可動はEDF6を参考にして"): the user's EDF6VR closes the index finger
// with the trigger (dead zone .12, closed at .2 of the remaining travel), middle/ring/little with the grip (closed at .08:
// the Index force sensor), the thumb when it rests on the stick, from a relaxed .15. Here each finger joint turns from
// the neutral (open) rotation toward the native closed-hand pose by its amount; lengths stay neutral.
// Fourth real-device run (2026-10-09 "人差し指だけEDF仕様で敏感なので他の指に合わせて"): the EDF6 trigger curve closed
// the index finger at about .3 of the pull. It now follows the pull over most of the travel: dead zone .05, closed
// at .8 of the rest (trigger .81), as the other fingers follow a real squeeze rather than a touch.
enum Finger:unsigned {Thumb,Index,Middle,Ring,Little,FingerCount};
struct FingerCurl {std::array<float,FingerCount> amount{};};
inline FingerCurl curlFromControls(float trigger,float squeeze,bool thumbOnStick,float relaxed=.15f){
    auto clamp01=[](float v){return std::isfinite(v)?std::clamp(v,0.f,1.f):0.f;};
    auto past=[&](float v,float dead){v=clamp01(v);return v<=dead?0.f:(v-dead)/(1-dead);};
    relaxed=clamp01(relaxed);
    auto from=[&](float control,float fullAt){const float steep=control>fullAt?1.f:control/fullAt;return relaxed+(1-relaxed)*steep;};
    FingerCurl c;
    c.amount[Index]=from(past(trigger,.05f),.8f);
    c.amount[Middle]=c.amount[Ring]=c.amount[Little]=from(clamp01(squeeze),.08f);
    c.amount[Thumb]=thumbOnStick?1.f:relaxed;
    return c;
}
// Native parent-before-child hierarchy, unit joint scales. Only two private
// branch roots change. All fingers retain the selected native neutral pose.
// Socket indices are explicit asset calibration, never guessed at runtime.
class Rig {
    std::vector<Joint> neutral_,closed_;std::vector<uint16_t> parents_;
    std::array<std::array<std::vector<uint16_t>,FingerCount>,2> fingers_{};
    std::vector<Pose> globals_;std::array<Branch,2> branches_;
    static Pose pose(const Joint& j){return {j.orientation,j.position};}
    bool descendant(uint16_t id,uint16_t root) const {
        for(size_t n=0;n<parents_.size()&&id!=UINT16_MAX;n++,id=parents_[id])if(id==root)return true;
        return false;
    }
public:
    Rig(std::vector<Joint> neutral,std::vector<uint16_t> parents,std::vector<uint16_t> skin,std::array<Branch,2> branches)
      :neutral_(std::move(neutral)),parents_(std::move(parents)),branches_(branches){
        if(neutral_.empty()||neutral_.size()>256||parents_.size()!=neutral_.size()||skin.empty()||skin.size()>256)
            throw std::invalid_argument("hand rig count bounds");
        globals_.reserve(neutral_.size());
        for(size_t i=0;i<neutral_.size();i++){
            auto& j=neutral_[i];const auto p=parents_[i];
            if((p!=UINT16_MAX&&p>=i)||!finite(j.position)||!finite(j.scale)||
               std::abs(j.scale.x-1)>1e-4f||std::abs(j.scale.y-1)>1e-4f||std::abs(j.scale.z-1)>1e-4f)
                throw std::invalid_argument("hand rig hierarchy/nonunit scale");
            j.orientation=normalized(j.orientation);globals_.push_back(p==UINT16_MAX?pose(j):compose(globals_[p],pose(j)));
        }
        for(auto b:branches_){
            if(b.root>=neutral_.size()||b.wrist>=neutral_.size()||b.socket>=neutral_.size()||b.root==b.wrist||b.wrist==b.socket||
               !descendant(b.wrist,b.root)||!descendant(b.socket,b.wrist))throw std::invalid_argument("hand rig branch calibration");
        }
        if(descendant(branches_[0].root,branches_[1].root)||descendant(branches_[1].root,branches_[0].root))
            throw std::invalid_argument("hand rig overlapping branches");
        std::array<size_t,2> counts{};
        for(auto id:skin){if(id>=neutral_.size())throw std::invalid_argument("hand rig skin index");
            const bool l=descendant(id,branches_[0].root),r=descendant(id,branches_[1].root);
            if(l==r)throw std::invalid_argument("hand mesh includes unowned body bones");++counts[l?0:1];}
        if(!counts[0]||!counts[1])throw std::invalid_argument("hand rig missing side");
    }
    // anatomy[h] = {index, middle, little} knuckle joints of each hand (the grip calibration's measured anatomy).
    // The finger chains are the wrist's child branches that are not on the socket path; the one holding each knuckle is
    // that finger, the remaining one with the lowest index is the thumb, the last is the ring finger.
    void enableCurl(std::vector<Joint> closed,const std::array<std::array<uint16_t,3>,2>& anatomy){
        if(closed.size()!=neutral_.size())throw std::invalid_argument("closed hand pose size");
        for(auto& j:closed){if(!finite(j.position))throw std::invalid_argument("closed hand pose nonfinite");j.orientation=normalized(j.orientation);}
        std::array<std::array<std::vector<uint16_t>,FingerCount>,2> fingers{};
        for(unsigned h=0;h<2;h++){
            const auto b=branches_[h];std::vector<uint16_t> tops;std::vector<std::vector<uint16_t>> groups;
            for(uint16_t j=0;j<neutral_.size();j++){
                if(j==b.wrist||!descendant(j,b.wrist)||descendant(b.socket,j)||descendant(j,b.socket))continue;
                uint16_t top=j;while(parents_[top]!=b.wrist)top=parents_[top];
                const auto at=std::find(tops.begin(),tops.end(),top);
                if(at==tops.end()){tops.push_back(top);groups.push_back({j});}else groups[size_t(at-tops.begin())].push_back(j);
            }
            // The native wrist also carries single helper bones (left 66 beside the socket 64, 2026-10-09 hierarchy):
            // a finger has at least two joints.
            for(size_t g=groups.size();g-->0;)if(groups[g].size()<2){groups.erase(groups.begin()+g);tops.erase(tops.begin()+g);}
            if(groups.size()!=FingerCount)throw std::invalid_argument("hand rig does not have five finger chains");
            std::array<int,FingerCount> pick{-1,-1,-1,-1,-1};
            const std::array<Finger,3> named{Index,Middle,Little};
            for(unsigned k=0;k<3;k++)for(size_t g=0;g<groups.size();g++)
                if(std::find(groups[g].begin(),groups[g].end(),anatomy[h][k])!=groups[g].end())pick[named[k]]=int(g);
            if(pick[Index]<0||pick[Middle]<0||pick[Little]<0||pick[Index]==pick[Middle]||pick[Index]==pick[Little]||pick[Middle]==pick[Little])
                throw std::invalid_argument("finger anatomy does not match the rig");
            std::vector<int> rest;for(int g=0;g<int(groups.size());g++)if(g!=pick[Index]&&g!=pick[Middle]&&g!=pick[Little])rest.push_back(g);
            if(rest.size()!=2)throw std::invalid_argument("finger anatomy leftovers");
            const bool firstThumb=tops[rest[0]]<tops[rest[1]];pick[Thumb]=firstThumb?rest[0]:rest[1];pick[Ring]=firstThumb?rest[1]:rest[0];
            for(unsigned f=0;f<FingerCount;f++){fingers[h][f]=groups[size_t(pick[f])];if(fingers[h][f].size()<2||fingers[h][f].size()>5)throw std::invalid_argument("finger chain length");}
        }
        closed_=std::move(closed);fingers_=fingers;
    }
    bool curlEnabled()const{return !closed_.empty();}
    const std::vector<uint16_t>& parents()const{return parents_;}
    const std::vector<Joint>& neutral()const{return neutral_;} // the hierarchy's rest pose, parent-relative
    const std::array<std::vector<uint16_t>,FingerCount>& fingerJoints(unsigned hand)const{return fingers_.at(hand);}
    std::vector<Joint> locate(const std::array<Pose,2>& grips,const std::array<FingerCurl,2>* curl=nullptr) const {
        auto result=neutral_;
        if(curl&&!closed_.empty())for(unsigned h=0;h<2;h++)for(unsigned f=0;f<FingerCount;f++){
            const float t=std::isfinite((*curl)[h].amount[f])?std::clamp((*curl)[h].amount[f],0.f,1.f):0.f;
            for(auto j:fingers_[h][f]){
                const auto a=neutral_[j].orientation;auto b=closed_[j].orientation;
                if(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0)b={-b.x,-b.y,-b.z,-b.w};
                result[j].orientation=normalized(Quat{a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t,a.w+(b.w-a.w)*t});
            }
        }
        for(size_t h=0;h<2;h++){
            if(!finite(grips[h].position))throw std::invalid_argument("hand target position");
            const auto b=branches_[h];
            const auto rootToSocket=compose(inverse(globals_[b.root]),globals_[b.socket]);
            const auto rootGlobal=compose(grips[h],inverse(rootToSocket));
            const auto p=parents_[b.root];const auto local=p==UINT16_MAX?rootGlobal:compose(inverse(globals_[p]),rootGlobal);
            result[b.root].orientation=local.orientation;result[b.root].position=local.position;
        }
        return result;
    }
    std::vector<Pose> globals(const std::vector<Joint>& joints) const {
        if(joints.size()!=parents_.size())throw std::invalid_argument("hand output size");std::vector<Pose> out;out.reserve(joints.size());
        for(size_t i=0;i<joints.size();i++)out.push_back(parents_[i]==UINT16_MAX?pose(joints[i]):compose(out[parents_[i]],pose(joints[i])));
        return out;
    }
    const std::array<Branch,2>& branches() const {return branches_;}
    // Neutral pose of the hand socket in its branch root: places that root from a grip on any parent pose.
    Pose rootToSocket(unsigned hand) const {const auto b=branches_.at(hand);return compose(inverse(globals_[b.root]),globals_[b.socket]);}
};
}
