#pragma once
// A consumable chosen on the quick wheel is held, not used (2026-10-23, the user: "choosing a healing item or another item
// on the item slots must not use it at once - its model comes to the right hand, and bringing it to the mouth uses it;
// bandages, mushrooms, healing potions, every item: at the mouth or by the right trigger. Throwables are thrown as before").
// The game uses a consumable through the item action of the sent input (keen::ecs::ClientPlayerInputData.itemAction at
// 476, keen::ecs::ItemAction: inventory slot {entity, index} at 0, a second slot at 8, its version at 16, its type at 20 -
// 1 Consume; the type table of build 26082781). Headset run 20261022-000930: the wheel let go on the potion and the
// game's use of it (UseItem_Potion_Health) began a frame later while the action bar's selection stayed on the weapon.
// As with the holsters' actions (holster.hpp HolsterSender): the game takes an action when its version differs from the
// last one it took. A consume the wheel wrote for a consumable is kept back - the action sent stays the one sent before,
// which the game has taken already - and the slot is remembered; use() sends a consume of that slot with a version of
// the mod's own, kept in the input until the game writes a new action of its own.
#include <array>
#include <cstdint>
#include <cstring>
#include <evr/idle_head_camera.hpp>
namespace evr::game {
constexpr size_t inputItemAction=476,itemActionBytes=24,itemActionVersion=16,itemActionType=20;
constexpr uint8_t itemActionConsume=1;
using ItemActionBytes=std::array<uint8_t,itemActionBytes>;
using InventorySlotBytes=std::array<uint8_t,8>;
class ConsumableHold {
    ItemActionBytes passed_{},seen_{};bool started_=false,use_=false;uint32_t version_=0x45570000;
    InventorySlotBytes slot_{};uint32_t item_=0;bool held_=false;
public:
    struct Result {bool deferred=false,used=false,passedGameAction=false;uint32_t item=0;InventorySlotBytes slot{};};
    // Where the mod's versions begin (a later session must not begin where an earlier one ended).
    void seed(uint32_t version){version_=version;}
    bool held()const{return held_;}
    uint32_t item()const{return held_?item_:0;}
    // The held item is used with the next input sent.
    void use(){if(held_)use_=true;}
    // The held item is let go without being used (another item chosen, the world left).
    void drop(){held_=false;use_=false;}
    // deferrable(slot): the item to hold for a consume of that inventory slot, 0 to let the game's action go as it is.
    template<class Deferrable> Result apply(const ClientInputBytes& source,ClientInputBytes& output,Deferrable&& deferrable){
        Result r;ItemActionBytes action{};std::memcpy(action.data(),source.data()+inputItemAction,itemActionBytes);
        if(!started_){started_=true;seen_=action;passed_=action;}      // the first input: what the game had goes as it is
        else if(action!=seen_){
            seen_=action;InventorySlotBytes slot{};std::memcpy(slot.data(),action.data(),8);
            const uint32_t item=action[itemActionType]==itemActionConsume?deferrable(slot):0;
            if(item){slot_=slot;item_=item;held_=true;use_=false;r.deferred=true;r.item=item;r.slot=slot;}
            else{passed_=action;r.passedGameAction=true;}
        }
        if(use_&&held_){
            ItemActionBytes own{};std::memcpy(own.data(),slot_.data(),8);const uint32_t version=++version_;
            std::memcpy(own.data()+itemActionVersion,&version,4);own[itemActionType]=itemActionConsume;
            passed_=own;held_=false;use_=false;r.used=true;r.item=item_;r.slot=slot_;
        }
        std::memcpy(output.data()+inputItemAction,passed_.data(),itemActionBytes);
        return r;
    }
};
} // namespace evr::game
