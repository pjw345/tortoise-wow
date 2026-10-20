#define MANGOSBOT_ZERO
#include <algorithm>
#include <type_traits>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>
using uint8=uint8_t;using uint16=uint16_t;using uint32=uint32_t;using int32=int32_t;
constexpr uint8 NULL_SLOT=255,INVENTORY_SLOT_BAG_0=0,EQUIPMENT_SLOT_FINGER1=10,EQUIPMENT_SLOT_FINGER2=11,
 EQUIPMENT_SLOT_TRINKET1=12,EQUIPMENT_SLOT_TRINKET2=13,EQUIPMENT_SLOT_MAINHAND=15,EQUIPMENT_SLOT_OFFHAND=16,
 EQUIPMENT_SLOT_BODY=3,EQUIPMENT_SLOT_TABARD=18;
constexpr int ITEM_CLASS_WEAPON=2,ITEM_CLASS_ARMOR=4,ITEM_CLASS_QUIVER=11,ITEM_CLASS_CONTAINER=1,
 INVTYPE_NON_EQUIP=0,INVTYPE_HEAD=1,INVTYPE_NECK=2,INVTYPE_CHEST=5,INVTYPE_FINGER=11,INVTYPE_TRINKET=12,
 INVTYPE_WEAPON=13,INVTYPE_SHIELD=14,INVTYPE_CLOAK=16,INVTYPE_2HWEAPON=17,INVTYPE_TABARD=19,
 INVTYPE_WEAPONMAINHAND=21,INVTYPE_HOLDABLE=23,INVTYPE_AMMO=24;
constexpr int CLASS_WARRIOR=1,CLASS_PALADIN=2,CLASS_HUNTER=3,CLASS_ROGUE=4,CLASS_PRIEST=5,
 CLASS_SHAMAN=7,CLASS_MAGE=8,CLASS_WARLOCK=9,CLASS_DRUID=11;
constexpr int ITEM_SUBCLASS_ARMOR_CLOTH=1,ITEM_SUBCLASS_ARMOR_LEATHER=2,ITEM_SUBCLASS_ARMOR_MAIL=3,
 ITEM_SUBCLASS_ARMOR_PLATE=4,ITEM_SUBCLASS_ARMOR_LIBRAM=7,ITEM_SUBCLASS_ARMOR_TOTEM=9,ITEM_SUBCLASS_ARMOR_IDOL=8,
 ITEM_SUBCLASS_AMMO_POUCH=3,ITEM_SUBCLASS_CONTAINER=0;
enum ItemSubclassWeapon {ITEM_SUBCLASS_WEAPON_GUN=3,ITEM_SUBCLASS_WEAPON_BOW=2,ITEM_SUBCLASS_WEAPON_CROSSBOW=18};
constexpr int INVENTORY_SLOT_BAG_END=23,INVENTORY_SLOT_ITEM_START=23,CMSG_AUTOEQUIP_ITEM=1,PLAYER_AMMO_ID=1;
constexpr int ITEM_FIELD_DURABILITY=0,ITEM_FIELD_MAXDURABILITY=1,MAX_ITEM_PROTO_DAMAGES=5;
enum WeaponAttackType { RANGED_ATTACK };
enum InventoryResult {EQUIP_ERR_OK,EQUIP_ERR_ITEM_NOT_FOUND,EQUIP_ERR_ITEM_CANT_BE_EQUIPPED};
enum class ItemUsage {ITEM_USAGE_NONE,ITEM_USAGE_EQUIP,ITEM_USAGE_BAD_EQUIP,ITEM_USAGE_BROKEN_EQUIP,ITEM_USAGE_KEEP,ITEM_USAGE_GUILD_TASK,ITEM_USAGE_FORCE_NEED,ITEM_USAGE_SKILL,ITEM_USAGE_USE,ITEM_USAGE_AH,ITEM_USAGE_BROKEN_AH,ITEM_USAGE_VENDOR,ITEM_USAGE_FORCE_GREED,ITEM_USAGE_DISENCHANT};
enum class ForceItemUsage {FORCE_USAGE_NONE,FORCE_USAGE_EQUIP};
enum class IterateItemsMask:uint8 {ITERATE_ITEMS_IN_BAGS=1,ITERATE_ITEMS_IN_EQUIP=2};
struct ItemPrototype {
 uint32 ItemId=0,Class=ITEM_CLASS_ARMOR,SubClass=ITEM_SUBCLASS_ARMOR_CLOTH,InventoryType=INVTYPE_CHEST,
 Quality=2,ItemLevel=20,Armor=20,Block=0,ContainerSlots=0,Delay=2000,score=0;std::string Name1="Item";bool usable=true,suitable=true,quest=false;
 struct {float DamageMin=0,DamageMax=0;}Damage[MAX_ITEM_PROTO_DAMAGES];struct{uint32 SpellId=0;}Spells[5];
};
struct Item {ItemPrototype* proto;uint8 slot=23,bag=0;uint32 variant=0,bonus=0,durability=10,maxDurability=10;
 uint8 GetBagSlot(){return bag;}uint16 GetPos(){return uint16(bag<<8)|slot;}bool IsBag(){return proto->Class==ITEM_CLASS_CONTAINER;}uint32 GetCount(){return 1;}ItemPrototype const* GetProto(){return proto;}uint8 GetSlot(){return slot;}uint32 GetUInt32Value(int n){return n?maxDurability:durability;}};
struct Bag:Item {bool empty=true;bool IsEmpty(){return empty;}uint32 GetBagSize(){return proto->ContainerSlots;}};
extern std::map<uint32,ItemPrototype*> prototypes;
struct ItemQualifier {ItemPrototype const* proto;uint32 variant=0,bonus=0;ItemQualifier(Item*i):proto(i->proto),variant(i->variant),bonus(i->bonus){}
 explicit ItemQualifier(ItemPrototype*p):proto(p){} ItemQualifier(uint32 id,int32 n):proto(prototypes[id]),variant(n){}uint32 GetId(){return proto->ItemId;}ItemPrototype const* GetProto(){return proto;}
 std::string GetQualifier(){return std::to_string(GetId())+":"+std::to_string(variant)+":"+std::to_string(bonus);}
 bool operator==(ItemQualifier const&q)const{return proto->ItemId==q.proto->ItemId&&variant==q.variant&&bonus==q.bonus;}};
struct FindItemByIdVisitor {uint32 id;std::list<Item*> result;explicit FindItemByIdVisitor(uint32 n):id(n){}std::list<Item*>GetResult(){return result;}};
struct PlayerbotAI;struct AiObjectContext;
struct Session {template<class T>void HandleAutoEquipItemOpcode(T&) {}};
struct Player {uint32 ammo=0;Session session;Session*GetSession(){return &session;}void SetAmmo(uint32 id){if(!failSwap)ammo=id;}uint32 GetUInt32Value(int){return ammo;}int cls=CLASS_PRIEST,spec=14;bool dual=false,failSwap=false;std::map<uint8,Item*> gear;std::vector<Item*> bags;std::set<uint8> blocked;
 void SendEquipError(InventoryResult,Item*,Item*){}
 void SwapItem(uint16 a,uint16 b){if(failSwap)return;Item*source=nullptr;for(auto*i:bags)if(i->GetPos()==a)source=i;if(!source)return;
 Item*old=GetItemByPos(b);bags.erase(std::remove(bags.begin(),bags.end(),source),bags.end());if(old){old->slot=a&255;bags.push_back(old);}gear[b&255]=source;source->slot=b&255;}int getClass(){return cls;}uint8 GetGUIDLow(){return 1;}char const*GetName(){return "Bot";}bool CanDualWield(){return dual;}
 Item*GetItemByPos(uint16 p){auto it=gear.find(p&255);return it==gear.end()?nullptr:it->second;}Item*GetItemByPos(uint8,uint8 p){return GetItemByPos(p);}
 InventoryResult CanUseItem(ItemPrototype const*p){return p&&p->usable?EQUIP_ERR_OK:EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;}
 Item*GetWeaponForAttack(WeaponAttackType,bool,bool){return nullptr;}
 uint8 defaultSlot(ItemPrototype const*p){switch(p->InventoryType){case INVTYPE_FINGER:return !GetItemByPos(10)?10:(!GetItemByPos(11)?11:10);
 case INVTYPE_TRINKET:return !GetItemByPos(12)?12:(!GetItemByPos(13)?13:12);case INVTYPE_2HWEAPON:case INVTYPE_WEAPONMAINHAND:return 15;
 case INVTYPE_WEAPON:return dual&&!GetItemByPos(16)?16:15;case INVTYPE_HOLDABLE:case INVTYPE_SHIELD:return 16;default:return 4;}}
 InventoryResult check(uint8 slot,uint16&dest,ItemPrototype const*p){if(CanUseItem(p)!=EQUIP_ERR_OK)return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;
 dest=slot==NULL_SLOT?defaultSlot(p):slot;if(blocked.count(dest))return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;return EQUIP_ERR_OK;}
 InventoryResult CanEquipItem(uint8 s,uint16&d,Item*i,bool swap,bool=true){if(i->IsBag()&&!swap)return EQUIP_ERR_ITEM_CANT_BE_EQUIPPED;return check(s,d,i->proto);}
};
std::map<uint32,ItemPrototype*> prototypes;
struct RandomPlayerbotMgr {static InventoryResult CanEquipUnseenItem(Player*b,uint8 s,uint16&d,uint32 id){return b->check(s,d,prototypes.at(id));}};
struct RandomItemMgr {struct Scale {struct {uint32 id=1;std::string name="holy";}info;};std::map<uint32,Scale>m_weightScales;
 uint32 GetPlayerSpecId(Player*p){return p->spec;}uint32 ItemStatWeight(Player*,ItemQualifier&q){return q.proto->score+q.bonus;}
 uint32 ItemStatWeight(Player*b,Item*i){ItemQualifier q(i);return ItemStatWeight(b,q);}
 bool ShouldEquipWeaponForSpec(int,int,ItemPrototype const*p){return p->suitable;}
 bool ShouldEquipArmorForSpec(uint8,uint8,ItemPrototype const*);
}sRandomItemMgr;
struct ChatHelper {template<class T>static std::string formatItem(T){return "item";}std::string formatSlot(int){return "slot";}};
struct AiObjectContext {PlayerbotAI*ai=nullptr;std::map<std::string,ItemUsage>cache;std::map<uint32,ForceItemUsage>forced;
 void ClearValues(std::string){cache.clear();}void Reset(std::string q){cache.erase(q);}ItemUsage Usage(ItemQualifier q);};
struct ItemUsageValue {static InventoryResult GetEquipDestination(ItemQualifier&,Player*,uint16&);static ItemUsage QueryItemUsageForEquip(ItemQualifier&,Player*);
 static bool MustEquipForQuest(ItemPrototype const*p,Player*){return p->quest;}static uint32 CurrentStacks(PlayerbotAI*,ItemPrototype const*){return 0;}
 static uint32 GetSmallestBagSize(Player*){return 0;}static std::string ReasonForNeed(ItemUsage,Item*,int,Player*){return "reason";}static std::string ReasonForNeed(ItemUsage,ItemQualifier,int,Player*){return "upgrade";}};
struct FindItemUsageVisitor {Player*bot;ItemUsage usage;std::list<Item*>result;FindItemUsageVisitor(Player*b,ItemUsage u):bot(b),usage(u){}
 void SetUsage(ItemUsage u){usage=u;}std::list<Item*>GetResult(){return result;}};
struct PlayerbotAI {bool canLoot=true;std::string feedback;bool HasRealPlayerMaster(){return true;}template<class...T>void TellPlayerNoFacing(Player*,std::string s,T...){feedback=s;}Player*bot;AiObjectContext storage;ChatHelper chat;PlayerbotAI(Player*b):bot(b){storage.ai=this;}
 Player*GetBot(){return bot;}template<class...T>void TellPlayer(T...){}Player*GetMaster(){return nullptr;}AiObjectContext*GetAiObjectContext(){return &storage;}ChatHelper*GetChatHelper(){return &chat;}
 template<class...T>void TellDebug(T...){}void InventoryIterateItems(FindItemByIdVisitor*v,IterateItemsMask){for(auto*i:bot->bags)if(i->proto->ItemId==v->id)v->result.push_back(i);
 for(auto&e:bot->gear)if(e.second&&e.second->proto->ItemId==v->id)v->result.push_back(e.second);}
 void InventoryIterateItems(FindItemUsageVisitor*v,IterateItemsMask){for(auto*i:bot->bags)if(storage.Usage(ItemQualifier(i))==v->usage)v->result.push_back(i);}
 std::vector<Bag*>GetEquippedQuivers(){return {};}};
PlayerbotAI* activeAI=nullptr;PlayerbotAI*GetBotAI(Player*){return activeAI;}
ItemUsage AiObjectContext::Usage(ItemQualifier q){auto k=q.GetQualifier();if(!cache.count(k))cache[k]=ItemUsageValue::QueryItemUsageForEquip(q,ai->bot);return cache[k];}
#define AI_VALUE2_EXISTS(T,n,id,def) (context->forced.count(id)?context->forced.at(id):def)
#define AI_VALUE2(T,n,q) context->Usage(qualifier)
#define RESET_AI_VALUE2(T,n,q) context->Reset(q)
struct Config {int events=0;template<class...T>void logEvent(T...){++events;}bool rollBadItemsWithPlayer=false;bool autoEquipUpgradeLoot=true;}sPlayerbotAIConfig;
struct RandomBots {bool IsRandomBot(Player*){return false;}}sRandomPlayerbotMgr;
struct Logger {template<class...T>void outDetail(T...){}}sLog;
constexpr int TRADE_STATUS_TRADE_ACCEPT=4;
struct ObjectGuid {int n=0;bool operator!=(ObjectGuid b){return n!=b.n;}};
enum class PlayerbotSecurityLevel {PLAYERBOT_SECURITY_ALLOW_ALL};
#define BOT_TEXT2(n,args) (std::string("equipped"))
struct WorldPacket {WorldPacket(){}WorldPacket(int,int){}template<class T>WorldPacket&operator<<(T){return *this;}WorldPacket(WorldPacket const&)=default;bool empty(){return true;}void rpos(int){}template<class T>WorldPacket&operator>>(T&){return *this;}};
struct Event {std::string source;WorldPacket packet;std::string getSource(){return source;}WorldPacket&getPacket(){return packet;}};
namespace ItemClass {constexpr int ITEM_CLASS_WEAPON=2,ITEM_CLASS_ARMOR=4,ITEM_CLASS_CONTAINER=1;}
struct EquipUpgradesAction {PlayerbotAI*ai;Player*bot;AiObjectContext*context;ChatHelper*chat;int swaps=0;
 explicit EquipUpgradesAction(PlayerbotAI*a):ai(a),bot(a->bot),context(a->GetAiObjectContext()),chat(a->GetChatHelper()){}
 Player*GetMaster(){return nullptr;}bool Execute(Event&);
 bool EquipItem(PlayerbotAI*,Player*,Item*i,bool,uint8 slot){if(bot->failSwap)return false;++swaps;
 if(Item*old=bot->GetItemByPos(slot)){old->slot=23;bot->bags.push_back(old);}bot->bags.erase(std::remove(bot->bags.begin(),bot->bags.end(),i),bot->bags.end());
 bot->gear[slot]=i;i->slot=slot;context->ClearValues("item usage");return true;}};
// The GUID is irrelevant to the non-packet regression events but required by the production method.
#define GetObjectGuid() GetGUIDLow()
// Production packet comparison needs matching types only; no packet is synthesized here.
bool operator!=(ObjectGuid,int){return false;}
struct EquipAction {static uint8 GetSmallestBagSlot(Player*){return 19;}static bool EquipItem(PlayerbotAI*,Player*,Item*,bool,uint8);};
uint32 urand(uint32,uint32){return 100;}
#include "NativeGear.inc"
enum RollVote {ROLL_PASS,ROLL_GREED,ROLL_NEED,ROLL_DISENCHANT};
struct StoreLootAction {static bool IsLootAllowed(ItemQualifier&,PlayerbotAI*a){return a->canLoot;}};
struct RollAction {PlayerbotAI*ai;Player*bot;AiObjectContext*context;explicit RollAction(PlayerbotAI*a):ai(a),bot(a->bot),context(a->GetAiObjectContext()){}RollVote CalculateRollVote(ItemQualifier&,bool=false);};
template<class T>T RollValue(AiObjectContext*c,ItemQualifier&q){if constexpr(std::is_same_v<T,bool>)return true;else return c->Usage(q);}
#undef AI_VALUE2
#define AI_VALUE2(T,n,q) RollValue<T>(context,itemQualifier)
#include "NativeRoll.inc"

void check(bool ok,char const*message){if(!ok)throw std::runtime_error(message);}
ItemPrototype proto(uint32 id,uint32 score,int inv=INVTYPE_CHEST){ItemPrototype p;p.ItemId=id;p.score=score;p.InventoryType=inv;return p;}
int main(){try{
 Player p;PlayerbotAI ai(&p);activeAI=&ai;ItemPrototype a=proto(1,10),b=proto(2,30),c=proto(3,20);prototypes={{1,&a},{2,&b},{3,&c}};
 Item old{&a},best{&b},middle{&c};old.slot=4;p.gear[4]=&old;p.bags={&best,&middle};
 EquipUpgradesAction action(&ai);Event event;
 check(action.Execute(event)&&p.gear[4]==&best&&action.swaps==1,"same-tick candidates never downgrade best item");
 check(!action.Execute(event)&&action.swaps==1,"repeat scan is stable and does not unequip weapons");
 p.gear[4]=&old;p.bags={&best};p.failSwap=true;ai.storage.ClearValues("");
 check(!action.Execute(event)&&p.gear[4]==&old,"failed native operation is not reported as equip");p.failSwap=false;
 Item variant{&a};variant.variant=99;variant.bonus=20;p.bags={&variant};ItemQualifier q(&variant);
 check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_EQUIP,"same template with better random stats upgrades");
 variant.bonus=0;check(ItemUsageValue::QueryItemUsageForEquip(q=ItemQualifier(&variant),&p)==ItemUsage::ITEM_USAGE_NONE,"equal variant is not an upgrade");
 a.InventoryType=b.InventoryType=c.InventoryType=INVTYPE_FINGER;a.SubClass=b.SubClass=c.SubClass=0;
 old.slot=10;best.slot=11;p.gear.clear();p.gear[10]=&best;p.gear[11]=&old;p.bags={&middle};q=ItemQualifier(&middle);uint16 dest;
 check(ItemUsageValue::GetEquipDestination(q,&p,dest)==EQUIP_ERR_OK&&dest==11,"paired slot selects weaker second ring");
 check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_EQUIP,"intermediate ring upgrades weaker equipped ring");
 std::swap(p.gear[10],p.gear[11]);check(ItemUsageValue::GetEquipDestination(q,&p,dest)==EQUIP_ERR_OK&&dest==10,"paired slot handles reverse ring order");
 p.blocked.insert(10);check(ItemUsageValue::GetEquipDestination(q,&p,dest)==EQUIP_ERR_OK&&dest==11,"native destination rejection retained");p.blocked.clear();
 p.gear.clear();check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_EQUIP,"useful empty jewelry slot is Need-worthy");
 c.usable=false;check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_NONE,"native class or level rejection retained");c.usable=true;
 a=proto(1,0);b=proto(2,0);a.Quality=b.Quality=0;a.Armor=113;b.Armor=115;
 check(IsBetterEquipment(0,0,&b,&a),"statless gray armor can improve armor");
 p.gear[4]=&old;p.bags={&best};q=ItemQualifier(&best);
 check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_EQUIP,"gray armor improvement remains usable");
 ai.storage.forced[1]=ForceItemUsage::FORCE_USAGE_EQUIP;
 check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_NONE,"forced current equipment protected");ai.storage.forced.clear();
 a=proto(1,30,INVTYPE_WEAPONMAINHAND);a.Class=ITEM_CLASS_WEAPON;b=proto(2,20,INVTYPE_HOLDABLE);b.Class=ITEM_CLASS_ARMOR;
 c=proto(3,40,INVTYPE_2HWEAPON);c.Class=ITEM_CLASS_WEAPON;p.gear.clear();p.gear[15]=&old;p.gear[16]=&best;p.bags={&middle};q=ItemQualifier(&middle);
 check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_NONE,"two-hander must beat both hands together");
 c.score=60;check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_EQUIP,"better two-handed configuration accepted");
 p.gear.clear();p.gear[15]=&middle;p.bags={&best};q=ItemQualifier(&best);
 check(ItemUsageValue::QueryItemUsageForEquip(q,&p)==ItemUsage::ITEM_USAGE_NONE,"offhand alone cannot evict two-hander");
 // Actual production native-dispatch wrapper: success means equipment really moved.
 p.gear.clear();a=proto(1,10);b=proto(2,20);old.slot=4;best.slot=23;p.gear[4]=&old;p.bags={&best};p.failSwap=true;
 int logged=sPlayerbotAIConfig.events;
 check(!EquipAction::EquipItem(&ai,nullptr,&best,true,4)&&p.gear[4]==&old&&sPlayerbotAIConfig.events==logged,"failed native swap neither reports nor logs equip");
 p.failSwap=false;check(EquipAction::EquipItem(&ai,nullptr,&best,true,4)&&p.gear[4]==&best,"native explicit-slot swap verifies final equipment");
 ItemPrototype bagProto=proto(20,0);bagProto.Class=ITEM_CLASS_CONTAINER;bagProto.ContainerSlots=12;
 Bag smallBag,bigBag;smallBag.proto=bigBag.proto=&bagProto;smallBag.slot=19;bigBag.slot=24;p.gear[19]=&smallBag;p.bags={&bigBag};
 smallBag.empty=false;check(!EquipAction::EquipItem(&ai,nullptr,&bigBag,true,NULL_SLOT)&&p.gear[19]==&smallBag,"non-empty equipped bag is never displaced");
 smallBag.empty=true;check(EquipAction::EquipItem(&ai,nullptr,&bigBag,true,NULL_SLOT)&&p.gear[19]==&bigBag,"empty bag replacement survives no-free-slot preflight");
 RollAction roll(&ai);a=proto(1,10);b=proto(2,30);p.gear[4]=&old;p.bags={&best};q=ItemQualifier(&best);
 check(roll.CalculateRollVote(q)==ROLL_NEED,"fresh valid upgrade rolls Need");
 check(roll.CalculateRollVote(q,true)==ROLL_GREED&&ai.feedback.find("Rolling greed")==0,"human priority and final feedback agree");
 ai.canLoot=false;check(roll.CalculateRollVote(q)==ROLL_PASS&&ai.feedback.find("Passing")==0,"loot denial and final feedback agree");
 std::cout<<"Native gear comparison, paired slots, suffixes, cache invalidation and native-failure cases passed\n";
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
