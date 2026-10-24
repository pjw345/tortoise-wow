#include "EnchantingRodRequirements.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using uint32=std::uint32_t; using ObjectGuid=std::uint64_t;
constexpr uint32 BUYBACK_SLOT_START=69,BUYBACK_SLOT_END=81,PLAYER_FIELD_BUYBACK_PRICE_1=100;
constexpr int CMSG_BUYBACK_ITEM=1,UNIT_NPC_FLAG_VENDOR=2,EQUIP_ERR_OK=0,GAMEOBJECT_TYPE_CHEST=3;
enum class IterateItemsMask { ITERATE_ITEMS_IN_BUYBACK };
enum class PlayerbotSecurityLevel { PLAYERBOT_SECURITY_ALLOW_ALL };
struct GOInfo {int type=GAMEOBJECT_TYPE_CHEST;struct {uint32 minSuccessOpens=2;}chest;
#include "Gathering.inc"
};
struct Proto {uint32 ItemLevel=10;};
struct Item {uint32 id=1179,count=18;Proto proto;uint32 GetEntry(){return id;}uint32 GetCount(){return count;}Proto* GetProto(){return &proto;}};
struct Creature {} vendor;
struct WorldPacket {explicit WorldPacket(int){}std::vector<std::uint64_t> fields;WorldPacket& operator<<(std::uint64_t v){fields.push_back(v);return *this;}};
struct Player;
struct Session {Player* player;void HandleBuybackItem(WorldPacket&);};
struct Player {std::map<uint32,Item*> slots;std::map<uint32,uint32> carried;uint32 money=1000;bool vendorPresent=true,storeFails=false;Session session{this};
 Item* GetItemFromBuyBackSlot(uint32 s){auto i=slots.find(s);return i==slots.end()?nullptr:i->second;}
 uint32 GetItemCount(uint32 id){return carried[id];}uint32 GetMoney(){return money;}uint32 GetUInt32Value(uint32){return 100;}
 Creature* GetNPCIfCanInteractWith(ObjectGuid,int){return vendorPresent?&vendor:nullptr;}Session* GetSession(){return &session;}};
void Session::HandleBuybackItem(WorldPacket& p){auto slot=uint32(p.fields.at(1));auto* i=player->GetItemFromBuyBackSlot(slot);if(!i||player->storeFails)return;player->carried[i->id]+=i->count;player->slots.erase(slot);player->money-=100;}
struct Event {Player* owner=nullptr;Player* getOwner(){return owner;}std::string getParam(){return "item selector";}};
std::list<ObjectGuid> vendors{1};
#define AI_VALUE(type,name) vendors
struct ContextValue {std::list<ObjectGuid> Get(){return vendors;}};
struct Context {ContextValue value;template<class T> ContextValue* GetValue(std::string){return &value;}};
struct AI {Player* bot;std::vector<std::string> messages,errors;Context context;Context* GetAiObjectContext(){return &context;}
 std::list<Item*> InventoryParseItems(std::string,IterateItemsMask){std::list<Item*> r;for(auto p:bot->slots)r.push_back(p.second);return r;}
 void TellError(Player*,char const* s){errors.push_back(s);}void TellPlayerNoFacing(Player*,std::string const& s,PlayerbotSecurityLevel,bool){messages.push_back(s);}};
struct Chat {std::string formatItem(Item* i){return "[item:"+std::to_string(i->id)+"]";}} chatHelper;
struct Facade {void SetFacingTo(Player*,Creature*){}} sServerFacade;
struct BuyBackAction {Player* bot;AI* ai;Chat* chat=&chatHelper;Player master;Player* GetMaster(){return &master;}bool Execute(Event&);};
#include "Buyback.inc"
void check(bool x,char const* why){if(!x)throw std::runtime_error(why);}
int main(){try{
 for(std::size_t required=0;required<Enchanting::RunedRods.size();++required)
  for(std::size_t carried=0;carried<Enchanting::RunedRods.size();++carried){
   auto has=[&](uint32 id){return id==Enchanting::RunedRods[carried];};
   check(Enchanting::HasRequiredTool(Enchanting::RunedRods[required],has)==(carried>=required),"rod tier direction");
  }
 auto none=[](uint32){return false;};auto highest=[](uint32 id){return id==16207;};
 check(!Enchanting::HasRequiredTool(6218,none),"missing or bank-only rods fail carried predicate");
 check(!Enchanting::HasRequiredTool(2901,highest),"runed rod cannot replace unrelated tool");
 check(Enchanting::HasRequiredTool(2901,[](uint32 id){return id==2901;}),"exact other tool remains accepted");
 GOInfo go;check(go.IsRepeatableGatheringChest(),"native mineral exemption");go.chest.minSuccessOpens=0;check(!go.IsRepeatableGatheringChest(),"ordinary chests keep LOS");go.type=10;go.chest.minSuccessOpens=2;check(!go.IsRepeatableGatheringChest(),"other union members do not enable exemption");
 Event e;Item milk;Player p;AI ai{&p};BuyBackAction a{&p,&ai};p.slots[69]=&milk;p.carried[1179]=2;
 check(a.Execute(e)&&p.carried[1179]==20&&ai.messages.size()==1&&ai.messages[0]=="Bought back 18x [item:1179].","success reports actual stack and item");
 check(!a.Execute(e)&&ai.messages.size()==1,"repeated request cannot confirm twice");
 p.slots[69]=&milk;p.money=0;check(!a.Execute(e)&&ai.messages.size()==1,"unaffordable buyback has no success");
 p.money=1000;p.storeFails=true;check(!a.Execute(e)&&ai.messages.size()==1,"native storage rejection has no success");
 p.storeFails=false;p.vendorPresent=false;check(!a.Execute(e)&&ai.messages.size()==1,"missing vendor has no success");
 p.vendorPresent=true;Item potion{2455,2};p.slots[70]=&potion;check(a.Execute(e)&&ai.messages.size()==3,"each recovered stack confirmed once");
 std::cout<<"Session followup policy and native buyback adapter cases passed\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
