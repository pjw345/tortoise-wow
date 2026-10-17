#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
using uint32 = uint32_t;
using ObjectGuid = int;
constexpr int MAX_EFFECT_INDEX=3, MAX_ITEM_PROTO_SPELLS=5;
constexpr int ITEM_CLASS_TRADE_GOODS=7, ITEM_SUBCLASS_EXPLOSIVES=2;
constexpr int ITEM_SPELLTRIGGER_ON_USE=0, ITEM_SPELLTRIGGER_ON_NO_DELAY_USE=5;
constexpr int SPELL_EFFECT_SCHOOL_DAMAGE=2, EQUIP_ERR_OK=0;
constexpr int TARGET_ENUM_UNITS_ENEMY_AOE_AT_DEST_LOC=16;
enum class BotCheatMask { item };
enum class IterateItemsMask { ITERATE_ITEMS_IN_BAGS };
struct SpellEntry { uint32 Id=4054, rangeIndex=1; uint32 EffectRadiusIndex[3]={1}; int Effect[3]={2},EffectImplicitTargetA[3]={16},EffectImplicitTargetB[3]={}; bool damage=true; bool HasEffect(int)const{return damage;} };
struct ItemSpell { uint32 SpellId=0; int SpellTrigger=0; };
using _Spell = ItemSpell;
struct ItemPrototype { uint32 ItemId=0,ItemLevel=0,RequiredLevel=0,RequiredSkillRank=0; int Class=7,SubClass=2; bool allowed=true; ItemSpell Spells[5]; };
struct Item { ItemPrototype* proto; uint32 count=1; bool trade=false; uint32 GetCount(){return count;} bool IsInTrade(){return trade;} ItemPrototype const* GetProto(){return proto;} };
struct Unit { ObjectGuid guid=1; bool alive=true,combat=true,safe=true,cc=false,los=true,casting=false,stopped=true; float x=10; float GetCombatReach(){return 1;} ObjectGuid GetObjectGuid(){return guid;} bool IsWithinDistInMap(Unit* u,float r){return std::abs(x-u->x)<=r;} bool IsNonMeleeSpellCasted(bool){return casting;} bool IsStopped(){return stopped;} };
struct Player:Unit { uint32 level=25,skill=1; bool healer=false,mounted=false,arena=false; std::vector<Item*> items; std::map<uint32,uint32> counts; bool InArena(){return arena;} bool IsMounted(){return mounted;} uint32 GetLevel(){return level;} uint32 GetSkillValue(int){return skill;} float GetDistance(Unit*u){return std::abs(x-u->x);} bool IsWithinLOSInMap(Unit*u){return u->los;} bool HasItemCount(uint32 id,int){return counts[id]>0;} int CanUseItem(Item* item){auto*p=item->GetProto();return p->allowed&&level>=p->RequiredLevel&&skill>=p->RequiredSkillRank&&skill>0?0:1;} };
struct StoreRange { float low=0,high=30; bool present=true; auto LookupEntry(uint32)const{return present?this:nullptr;} }sSpellRangeStore;
struct StoreRadius { float radius=5; auto LookupEntry(uint32 id)const{return id?this:nullptr;} }sSpellRadiusStore;
namespace Spells { float GetSpellMinRange(StoreRange const*r){return r->low;} float GetSpellMaxRange(StoreRange const*r){return r->high;} float GetSpellRadius(StoreRadius const*r){return r?r->radius:0;} }
std::map<uint32,SpellEntry> spells;
struct Facade { bool IsAlive(Unit*u){return u->alive;} bool IsInCombat(Unit*u){return u->combat;} SpellEntry const* LookupSpellInfo(uint32 id){auto i=spells.find(id);return i==spells.end()?nullptr:&i->second;} }sServerFacade;
struct SpellTemplate { template<class T> T const* LookupEntry(uint32 id){return sServerFacade.LookupSpellInfo(id);} }sSpellTemplate;
struct ObjectMgr { std::map<uint32,ItemPrototype*> prototypes; ItemPrototype const* GetItemPrototype(uint32 id){auto i=prototypes.find(id);return i==prototypes.end()?nullptr:i->second;} }sObjectMgr;
struct PlayerbotAI { Player* bot; Unit* target; bool owner=true,real=false,pvp=false,attackAllowed=true,spellAllowed=true,cheat=false; std::list<ObjectGuid> attackers{1},nearby{1}; std::map<int,Unit*> units; std::set<uint32> skipped,cooldowns; bool HasActivePlayerMaster(){return owner;} bool IsRealPlayer(){return real;} bool IsInPvp(){return pvp;} bool HasCheat(BotCheatMask){return cheat;} bool IsSafe(Unit*u){return u->safe;} bool IsPlayerMasterAttackAllowed(Unit*){return attackAllowed;} bool IsPlayerMasterSpellAllowed(SpellEntry const*,Unit*){return spellAllowed;} static bool IsHeal(Player*p){return p->healer;} auto InventoryParseItems(char const*,IterateItemsMask){return bot->items;} Unit* GetUnit(int guid){return units[guid];} template<class T> T value(char const*); };
template<> std::list<ObjectGuid> PlayerbotAI::value<std::list<ObjectGuid>>(char const*){return attackers;}
template<> std::set<uint32>& PlayerbotAI::value<std::set<uint32>&>(char const*){return skipped;}
#define AI_VALUE(type, name) ai->value<type>(name)
struct PossibleAttackTargetsValue { static bool HasBreakableCC(Unit*u,Player*){return u->cc;} };
struct PossibleTargetsValue { PlayerbotAI* ai; static bool unvalidated; PossibleTargetsValue(PlayerbotAI*a,char const*,float,bool):ai(a){} void Qualify(std::string q){unvalidated=q.find(":1")!=std::string::npos;} auto Calculate(){return ai->nearby;} }; bool PossibleTargetsValue::unvalidated=false;
struct Trigger { PlayerbotAI* ai; Player* bot; int interval; Trigger(PlayerbotAI*a,char const*,int i):ai(a),bot(a->bot),interval(i){} virtual bool IsActive(){return false;} };
struct UseItemIdAction { PlayerbotAI* ai; Player* bot; UseItemIdAction(PlayerbotAI*a,std::string):ai(a),bot(a->bot){} virtual ~UseItemIdAction()=default; virtual uint32 GetItemId(){return 0;} virtual bool isUseful(); virtual bool isPossible(); virtual bool UseInventoryItemOnly()const{return false;} bool HasItemCooldown(uint32 id){return ai->cooldowns.count(id)!=0;} std::string getQualifier(){return "";} static std::string getMultiQualifierStr(std::string,int,char const*){return "";} std::string getName(){return "throw grenade";} };
struct Log { template<class...T> void outError(char const*,T...){} }sLog;
struct UseTargetedItemIdAction:UseItemIdAction { using UseItemIdAction::UseItemIdAction; virtual std::string GetTargetName(){return "current target";} Unit* GetTarget(){return ai->target;} };
#include "NativeExplosiveClass.inc"
#include "NativeExplosives.inc"
#include "NativeExplosiveTrigger.inc"
struct TestAction:ThrowGrenadeAction { using ThrowGrenadeAction::ThrowGrenadeAction; using ThrowGrenadeAction::UseInventoryItemOnly; };
struct FixedItemAction:UseItemIdAction { using UseItemIdAction::UseItemIdAction; uint32 GetItemId()override{return 4358;} bool UseInventoryItemOnly()const override{return true;} };
void check(bool good,char const* label){if(!good)throw std::runtime_error(label);}
int main(){try{
 Player bot;bot.x=0;Unit target,add;add.guid=2;add.x=12;
 PlayerbotAI ai;ai.bot=&bot;ai.target=&target;ai.units={{1,&target},{2,&add}};
 SpellEntry blast;spells[4054]=blast;
 ItemPrototype rough;rough.ItemId=4358;rough.ItemLevel=10;rough.RequiredSkillRank=1;rough.Spells[0].SpellId=4054;
 ItemPrototype coarse=rough;coarse.ItemId=4365;coarse.ItemLevel=20;coarse.RequiredSkillRank=75;
 ItemPrototype bronze=rough;bronze.ItemId=4374;bronze.ItemLevel=29;bronze.RequiredSkillRank=120;bronze.RequiredLevel=29;
 Item r{&rough},c{&coarse},b{&bronze};bot.items={&b,&c,&r};sObjectMgr.prototypes={{4358,&rough},{4365,&coarse},{4374,&bronze}};
 TestAction action(&ai);OwnedExplosiveTrigger trigger(&ai);
 check(trigger.interval==5&&trigger.IsActive(),"regular single-target owned explosive scheduling");
 check(action.GetItemId()==4358&&action.isUseful(),"level 25 engineering 1 stationary target uses Rough Dynamite");
 check(action.UseInventoryItemOnly(),"owned explosives force native item binding/consumption despite cheat");
 check(PossibleTargetsValue::unvalidated,"fresh blast scan includes CC and filtered bystanders");
 bot.skill=0;check(action.GetItemId()==0&&!action.isUseful(),"real item engineering requirement enforced");bot.skill=120;
 check(action.GetItemId()==4358,"lowest tier selected independently of bag iteration order");
 r.count=0;check(action.GetItemId()==4365,"advance after lowest stack exhausted");c.count=0;check(action.GetItemId()==0,"real item level requirement enforced");bot.level=29;check(action.GetItemId()==4374,"higher tier unlocked by actual requirement");
 bot.level=25;c.count=r.count=1;r.trade=true;check(action.GetItemId()==4365,"skip traded stack");r.trade=false;
 rough.allowed=false;check(action.GetItemId()==4365,"native item use rejection respected");rough.allowed=true;
 ai.cooldowns={4358};check(action.GetItemId()==4365,"skip individual cooldown");ai.cooldowns={4358,4365};check(action.GetItemId()==0,"no off-cooldown owned usable item");ai.cooldowns.clear();
 ai.skipped={4054};check(action.GetItemId()==0,"skip-spell restrictions respected");ai.skipped.clear();
 ai.nearby.push_back(2);check(action.GetItemId()==0,"unengaged bystander prevents new pull");
 ai.attackers.push_back(2);check(action.GetItemId()==4358,"engaged additional enemy safe");add.cc=true;check(action.GetItemId()==0,"CC in blast prevents use");add.cc=false;
 ai.attackers={1};add.x=30;check(action.GetItemId()==4358,"bystander outside blast allowed");add.x=12;add.alive=false;check(action.GetItemId()==4358,"dead bystander harmless");add.alive=true;ai.nearby={1};
 target.cc=true;check(action.GetItemId()==0,"CC target protected");target.cc=false;
 target.los=false;check(action.GetItemId()==0,"target LOS enforced");target.los=true;
 target.x=31;check(action.GetItemId()==0,"no chase to extend throw range");target.x=10;
 sSpellRangeStore.low=11;check(action.GetItemId()==0,"item minimum range enforced");sSpellRangeStore.low=0;
 sSpellRangeStore.present=false;check(action.GetItemId()==0,"unknown range fails closed");sSpellRangeStore.present=true;
 spells[4054].EffectImplicitTargetA[0]=1;check(action.GetItemId()==0,"caster-centred special explosive is not mis-scanned at the target");spells[4054].EffectImplicitTargetA[0]=16;
 sSpellRadiusStore.radius=0;check(action.GetItemId()==0,"unknown blast radius fails closed");sSpellRadiusStore.radius=5;
 ai.attackAllowed=false;check(action.GetItemId()==0,"master target and fleeing restrictions retained");ai.attackAllowed=true;
 ai.spellAllowed=false;check(action.GetItemId()==0,"shared spell permission retained");ai.spellAllowed=true;
 ai.attackers.clear();check(action.GetItemId()==0,"target fighting unrelated party cannot initiate throw");ai.attackers={1};
 target.combat=false;check(action.GetItemId()==0,"never open combat with explosive");target.combat=true;
 bot.casting=true;check(!action.isUseful(),"do not interrupt healing/casts");bot.casting=false;
 bot.healer=true;check(!action.isUseful()&&!trigger.IsActive(),"healing role preserved");bot.healer=false;
 bot.mounted=true;check(!action.isUseful(),"mounted bot does not throw");bot.mounted=false;
 bot.combat=false;check(!action.isUseful()&&!trigger.IsActive(),"out of combat inactive");bot.combat=true;
 rough.Spells[0].SpellId=99999;check(action.GetItemId()==4365,"unknown spell skipped");rough.Spells[0].SpellId=4054;
 rough.Class=0;check(action.GetItemId()==4365,"non-explosive item skipped");rough.Class=7;
 rough.Spells[0].SpellTrigger=ITEM_SPELLTRIGGER_ON_NO_DELAY_USE;
#ifdef MANGOSBOT_ZERO
 check(action.GetItemId()==4358,"vanilla no-delay use supported");
#else
 check(action.GetItemId()==4365,"later expansions preserve on-use contract");
#endif
 rough.Spells[0].SpellTrigger=0;
 FixedItemAction owned(&ai,"test");ai.cheat=true;
 check(!owned.isPossible(),"item cheat cannot conjure owned-only explosives");bot.counts[4358]=1;check(owned.isPossible(),"owned-only native action possible with real stack");bot.counts[4358]=0;
 ItemPrototype legacy=coarse;legacy.ItemId=4390;sObjectMgr.prototypes[4390]=&legacy;
 ai.owner=false;bot.skill=175;bot.level=52;target.casting=true;
 check(!trigger.IsActive()&&!action.UseInventoryItemOnly()&&action.GetItemId()==4390&&action.isUseful(),"free bot legacy grenade choice retained");
 target.casting=false;check(!action.isUseful(),"free bot stationary target restriction retained");target.stopped=false;check(action.isUseful(),"free bot moving target retained");bot.level=51;check(!action.isUseful(),"free bot level threshold retained");bot.level=52;bot.skill=325;check(action.GetItemId()==23737,"free bot higher tier retained");
 ai.owner=true;ai.pvp=true;check(!trigger.IsActive()&&!action.UseInventoryItemOnly()&&action.GetItemId()==23737,"PvP legacy path retained");ai.pvp=false;
 ai.real=true;check(!trigger.IsActive()&&!action.UseInventoryItemOnly(),"real player control unchanged");
 std::cout<<"Native owned explosive selection, blast safety, scheduling and ownership cases passed\n";
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
