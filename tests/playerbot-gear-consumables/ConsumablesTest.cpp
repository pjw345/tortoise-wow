#include <algorithm>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using uint8=uint8_t;using uint32=uint32_t;using int32=int32_t;
#define MANGOS
#define MANGOSBOT_ZERO
constexpr int MAX_ITEM_PROTO_SPELLS=5,ITEM_CLASS_CONSUMABLE=0,ITEM_SUBCLASS_CONSUMABLE=0,ITEM_SUBCLASS_POTION=1,
 ITEM_SUBCLASS_ELIXIR=2,ITEM_SUBCLASS_FLASK=3,ITEM_SUBCLASS_SCROLL=4,ITEM_SUBCLASS_FOOD=5,ITEM_SUBCLASS_CONSUMABLE_OTHER=8,
 SPELL_AURA_SCHOOL_ABSORB=69,SPELL_AURA_MOD_RESISTANCE=22,SPELL_AURA_MOD_STAT=29,SPELL_AURA_MOD_ATTACK_POWER=99,
 SPELL_AURA_MOD_DAMAGE_DONE=13,SPELL_AURA_MOD_HIT_CHANCE=54,SPELL_AURA_MOD_CRIT_PERCENT=52,
 SPELL_AURA_MOD_POWER_REGEN=85,SPELL_AURA_MOD_HEALING_DONE=135,SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT=161,
 SPELL_SCHOOL_MASK_NORMAL=1,CLASS_WARRIOR=1,CLASS_PALADIN=2,CLASS_HUNTER=3,CLASS_ROGUE=4,CLASS_PRIEST=5,
 CLASS_SHAMAN=7,CLASS_MAGE=8,CLASS_WARLOCK=9,CLASS_DRUID=11,INVENTORY_SLOT_BAG_0=0,INVENTORY_SLOT_BAG_START=19,
 INVENTORY_SLOT_BAG_END=23,INVENTORY_SLOT_ITEM_START=23,INVENTORY_SLOT_ITEM_END=39;
enum InventoryResult {EQUIP_ERR_OK,EQUIP_ERR_ITEM_NOT_FOUND};
struct SpellEntry {uint32 Id=0,Category=0,EffectApplyAuraName[3]={},EffectTriggerSpell[3]={};int32 EffectBasePoints[3]={},EffectMiscValue[3]={};std::string SpellName[1]={"Buff"};};
struct ItemPrototype {uint32 ItemId=0,Class=0,SubClass=0,ItemLevel=1,RequiredLevel=1;
 struct {uint32 SpellId=0,SpellCategory=0;}Spells[5];};
struct Item {ItemPrototype proto;uint32 count=1;bool unusable=false;ItemPrototype const*GetProto(){return &proto;}};
struct Bag:Item {uint32 GetBagSize(){return 0;}};
struct Player {uint8 cls=CLASS_MAGE;int tab=1;bool combat=false,casting=false,taxi=false,bg=false;
 std::map<int,Item*>inventory;std::map<uint32,int>auras;std::set<uint32>stronger;
 bool InBattleGround(){return bg;}bool IsTaxiFlying(){return taxi;}bool IsInCombat(){return combat;}bool IsNonMeleeSpellCasted(bool,bool,bool){return casting;}
 int getClass(){return cls;}int GetLevel(){return 25;}Item*GetItemByPos(int,int slot){auto it=inventory.find(slot);return it==inventory.end()?nullptr:it->second;}
 InventoryResult CanUseItem(Item*i){return i->unusable||!i->count||i->proto.RequiredLevel>25?EQUIP_ERR_ITEM_NOT_FOUND:EQUIP_ERR_OK;}
 bool HasMorePowerfulSpellActive(SpellEntry const*s){return stronger.count(s->Id);}auto const&GetSpellAuraHolderMap(){return auras;}
};
struct Facade {std::map<uint32,SpellEntry>spells;SpellEntry const*LookupSpellInfo(uint32 n){auto i=spells.find(n);return i==spells.end()?nullptr:&i->second;}}sServerFacade;
using SpellGroup=int;constexpr int SPELL_GROUP_STACK_RULE_DEFAULT=0;
struct SpellManager {std::set<std::pair<uint32,uint32>> exclusive;
 int CheckSpellGroupStackRules(uint32 a,uint32 b,SpellGroup&g){g=0;return exclusive.count({a,b})?1:0;}
 bool IsNoStackSpellDueToSpell(uint32,uint32){return false;}}sSpellMgr;
enum class BotCheatMask {item};
struct PlayerbotAI {Player*bot;bool cheats=false;int stopped=0;std::set<uint32>cooldowns,fail;std::vector<uint32>uses;
 bool HasCheat(BotCheatMask){return cheats;}bool IsTank(Player*){return false;}void StopMoving(){++stopped;}
 bool HasAura(uint32 id,Player*){return bot->auras.count(id);}bool HasAura(std::string,Player*){return false;}};
struct AiFactory {static int GetPlayerSpecTab(Player*p){return p->tab;}};
struct Event {};
struct UseItemIdAction {PlayerbotAI*ai;Player*bot;UseItemIdAction(PlayerbotAI*a,char const*):ai(a),bot(a->bot){}
 virtual bool Execute(Event&){uint32 id=GetItemId();if(ai->fail.count(id))return false;
 for(auto&e:bot->inventory){Item*i=e.second;if(i&&i->proto.ItemId==id&&i->count){--i->count;ai->uses.push_back(id);bot->auras[i->proto.Spells[0].SpellId]=1;return true;}}return false;}
 virtual bool isPossible(){return true;}virtual bool isUseful(){return true;}virtual uint32 GetItemId(){return 0;}
 virtual Player*GetTarget(){return nullptr;}virtual bool UseInventoryItemOnly()const{return false;}
 bool HasItemCooldown(uint32 id)const{return ai->cooldowns.count(id);}};
using Unit=Player;
#include "NativeConsumables.inc"
void check(bool ok,char const*s){if(!ok)throw std::runtime_error(s);}
Item make(uint32 id,uint32 spell,uint32 aura,int amount,int misc=0,int level=1){Item i;i.proto.ItemId=id;i.proto.ItemLevel=level;i.proto.Spells[0].SpellId=spell;
 SpellEntry s;s.Id=spell;s.EffectApplyAuraName[0]=aura;s.EffectBasePoints[0]=amount-1;s.EffectMiscValue[0]=misc;sServerFacade.spells[spell]=s;return i;}
int main(){try{
 using namespace ai;
 Player bot;PlayerbotAI ai{&bot};UseConsumableAction action(&ai);Event event;ConsumableType type;uint32 spell;
 Item troll=make(3382,3219,SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT,2);
 check(action.ClassifyConsumable(troll.GetProto(),type,spell)&&type==ConsumableType::Elixir&&spell==3219,"classic health regeneration recognised as buff");
 Item armor=make(1478,8094,SPELL_AURA_MOD_RESISTANCE,120,1);
 check(action.ClassifyConsumable(armor.GetProto(),type,spell)&&type!=ConsumableType::FoodBuff,"physical armor scroll is not elemental protection or food");
 Item protection=make(999,9999,SPELL_AURA_MOD_RESISTANCE,50,4);
 check(!action.ClassifyConsumable(protection.GetProto(),type,spell),"situational elemental protection remains reserved");
 Item intellect=make(955,8096,SPELL_AURA_MOD_STAT,4,3),stamina=make(1180,8099,SPELL_AURA_MOD_STAT,4,2),strength=make(954,8118,SPELL_AURA_MOD_STAT,5,0);
 for(auto*i:{&intellect,&stamina,&strength})check(action.ClassifyConsumable(i->GetProto(),type,spell)&&type!=ConsumableType::FoodBuff,"classic stat scroll classified independently of food");
 check(action.ScoreSpellStats(8118)==0,"mage does not waste strength scroll");
 bot.cls=CLASS_WARRIOR;check(action.ScoreSpellStats(8118)>0,"strength scroll remains useful to melee");bot.cls=CLASS_MAGE;
 bot.auras[19705]=1;bot.inventory={{23,&intellect}};
 check(action.Execute(event)&&ai.uses.back()==955,"well-fed does not block unrelated intellect scroll");
 check(action.GetTarget()==&bot&&action.UseInventoryItemOnly(),"buff use explicitly targets self and requires real inventory");
 bot.inventory.clear();bot.auras.clear();ai.uses.clear();
 Item strongTroll=make(3388,3220,SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT,6,0,20);
 bot.inventory={{23,&strongTroll},{24,&troll}};
 check(action.Execute(event)&&ai.uses.back()==3382,"lower usable regeneration tier consumed first");
 sSpellMgr.exclusive.insert({3220,3219});
 check(!action.Execute(event)&&strongTroll.count==1,"existing weaker rank retained without wasting stronger potion");
 bot.auras.clear();bot.inventory={{23,&troll}};troll.count=1;ai.cooldowns.insert(3382);
 check(!action.Execute(event)&&troll.count==1,"native item cooldown excludes candidate");ai.cooldowns.clear();
 troll.unusable=true;check(!action.Execute(event),"native item-use rejection retained");troll.unusable=false;
 bot.inventory={{23,&intellect}};intellect.count=1;bot.stronger.insert(8096);
 check(!action.Execute(event)&&intellect.count==1,"native stronger class buff protects intellect scroll");bot.stronger.clear();
 bot.inventory={{23,&troll},{24,&stamina}};ai.fail.insert(3382);bot.auras.clear();
 check(action.Execute(event)&&ai.uses.back()==1180,"failed native candidate does not starve another usable buff");ai.fail.clear();
 bot.combat=true;check(!action.isUseful(),"buff preparation never interrupts combat consumable policy");bot.combat=false;
 bot.bg=true;check(!action.isPossible(),"battleground exclusion retained");bot.bg=false;
 Item emergency=make(118,440,SPELL_AURA_MOD_STAT,5);emergency.proto.Spells[0].SpellCategory=4;
 check(!action.ClassifyConsumable(emergency.GetProto(),type,spell),"generic-subclass emergency potion kept separate");
 std::cout<<"Native classic buff classification, ranks, role selection, cooldown and failure cases passed\n";
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
