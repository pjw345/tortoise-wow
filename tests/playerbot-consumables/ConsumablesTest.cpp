#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>
using uint32=uint32_t; using ObjectGuid=int; using SpellEffectIndex=int;
constexpr int MAX_ITEM_PROTO_SPELLS=5, MAX_EFFECT_INDEX=3;
enum { POWER_MANA=0, EQUIP_ERR_OK=0, CLASS_HUNTER=3, ITEM_CLASS_CONSUMABLE=0, ITEM_CLASS_TRADE_GOODS=7,
 ITEM_SUBCLASS_CONSUMABLE=0, ITEM_SUBCLASS_POTION=1, ITEM_SUBCLASS_ELIXIR=2, ITEM_SUBCLASS_FLASK=3, ITEM_SUBCLASS_EXPLOSIVES=2,
 ITEM_SPELLTRIGGER_ON_USE=0,ITEM_SPELLTRIGGER_ON_NO_DELAY_USE=5,
 SPELL_EFFECT_APPLY_AURA=6, SPELL_EFFECT_ENERGIZE=30,SPELL_EFFECT_SCHOOL_DAMAGE=2,
 STAT_STRENGTH=0,STAT_AGILITY=1,STAT_STAMINA=2,STAT_INTELLECT=3,STAT_SPIRIT=4,
 SPELL_SCHOOL_MASK_NORMAL=1,
 SPELL_AURA_MOD_REGEN=84,SPELL_AURA_MOD_REGEN_DURING_COMBAT=116,SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT=161,
 SPELL_AURA_MOD_HEALTH_REGEN_PERCENT=88,SPELL_AURA_MOD_INCREASE_HEALTH=34,
 SPELL_AURA_MOD_POWER_REGEN=85,SPELL_AURA_MOD_POWER_REGEN_PERCENT=110,SPELL_AURA_MOD_INCREASE_ENERGY=35,
 SPELL_AURA_MOD_STAT=29,SPELL_AURA_MOD_RESISTANCE=22 };
enum class IterateItemsMask { ITERATE_ITEMS_IN_BAGS };
struct SpellEntry {
 uint32 Id=0,rangeIndex=1; uint32 Effect[3]={},EffectApplyAuraName[3]={},EffectRadiusIndex[3]={};
 int EffectMiscValue[3]={}; int amount[3]={2,2,2}; int duration=1200000;
 bool positive=true,aura=true;
 bool IsPositiveSpell() const {return positive;} bool IsSpellAppliesAura() const {return aura;}
 int GetDuration() const{return duration;} int CalculateSimpleValue(int i) const{return amount[i];}
 bool HasEffect(int effect) const{return std::find(Effect,Effect+3,effect)!=Effect+3;}
};
struct SpellRangeEntry {float min=0,max=30;}; struct SpellRadiusEntry {float radius=5;};
template<class T> struct Store { std::map<int,T> data; const T* LookupEntry(int i) const {auto it=data.find(i);return it==data.end()?nullptr:&it->second;} };
Store<SpellRangeEntry> sSpellRangeStore; Store<SpellRadiusEntry> sSpellRadiusStore;
float GetSpellMinRange(const SpellRangeEntry* p){return p?p->min:0;} float GetSpellMaxRange(const SpellRangeEntry*p){return p?p->max:0;}
float GetSpellRadius(const SpellRadiusEntry*p){return p?p->radius:0;}
namespace Spells {using ::GetSpellMinRange;using ::GetSpellMaxRange;using ::GetSpellRadius;}
struct ItemPrototype {
 uint32 ItemId=0,ItemLevel=1,RequiredLevel=0,RequiredSkillRank=0,Class=0,SubClass=0;
 bool usable=true;
 struct SpellData {uint32 SpellId=0,SpellTrigger=0;} Spells[5];
};
struct Unit {
 bool alive=true,combat=true,cc=false,stopped=true,casting=false;float x=0,y=0;int guid=1;
 bool IsAlive()const{return alive;} ObjectGuid GetObjectGuid()const{return guid;}
 float GetDistance(Unit* u)const{return std::hypot(x-u->x,y-u->y);}float GetCombatReach()const{return 0;}
 bool IsWithinDistInMap(Unit* u,float radius)const{return GetDistance(u)<=radius;}
 bool IsNonMeleeSpellCasted(bool)const{return casting;}bool IsStopped()const{return stopped;}
};
struct Player:Unit {
 uint32 mana=0,maxMana=500,level=24,skill=100;bool hasMana=true,heal=false,mounted=false,sit=false,moving=false,los=true,stronger=false;
 float health=100;int classId=5;std::set<uint32> auras;
 bool HasMana()const{return hasMana;}uint32 GetPower(int)const{return mana;}uint32 GetMaxPower(int)const{return maxMana;}
 float GetHealthPercent()const{return health;}float GetPowerPercent()const{return maxMana?100.0f*mana/maxMana:0;}
 bool IsMounted()const{return mounted;}bool IsSitState()const{return sit;}
 uint32 GetLevel()const{return level;}uint32 GetSkillValue(int)const{return skill;}int getClass()const{return classId;}
 int CanUseItem(const ItemPrototype*p)const{return p->usable&&level>=p->RequiredLevel&&skill>=p->RequiredSkillRank?0:1;}
 bool HasAura(uint32 id)const{return auras.count(id)!=0;}
 bool HasMorePowerfulSpellActive(const SpellEntry*)const{return stronger;}
 bool IsWithinLOSInMap(Unit*)const{return los;}
};
struct Item {ItemPrototype proto;uint32 count=1;bool trade=false;
 const ItemPrototype*GetProto()const{return &proto;}uint32 GetCount()const{return count;}bool IsInTrade()const{return trade;}};
struct Event{};struct Action{uint32 duration=1500;virtual~Action()=default;uint32 GetDuration()const{return duration;}void SetDuration(uint32 d){duration=d;}};
std::list<Unit*> nearby;
struct PlayerbotAI {
 Player* bot;bool human=true,ranged=false,attackAllowed=true,spellAllowed=true,nestedSuccess=false;
 int attempts=0;Action nested;std::list<Item*> items;std::set<uint32> cooldown;
 explicit PlayerbotAI(Player*p):bot(p){}
 bool HasActivePlayerMaster()const{return human;}static bool IsHeal(Player*p){return p->heal;}
 bool IsRanged(Player*)const{return ranged;}bool IsPlayerMasterAttackAllowed(Unit*)const{return attackAllowed;}
 bool IsPlayerMasterSpellAllowed(const SpellEntry*,Unit*)const{return spellAllowed;}
 std::list<Item*> InventoryParseItems(const char*,IterateItemsMask){return items;}
 bool DoSpecificAction(const char*,Event&,bool){++attempts;return nestedSuccess;}
 Unit*GetUnit(ObjectGuid guid){for(Unit*u:nearby)if(u->guid==guid)return u;return nullptr;}
 PlayerbotAI*GetAiObjectContext(){return this;}Action*GetAction(const char*){return &nested;}
};
struct SpellMgr{std::vector<uint32> weak;bool ListLessPowerfulSpells(uint32,std::vector<uint32>&out){out=weak;return !weak.empty();}} sSpellMgr;
std::set<uint32> skipped;std::list<ObjectGuid> attackers;
template<class T> T value(const char*){if constexpr(std::is_same_v<T,std::set<uint32>&>)return skipped;else return attackers;}
#define AI_VALUE(type,key) value<type>(key)
struct Facade {Store<SpellEntry> spells;
 const SpellEntry*LookupSpellInfo(uint32 id)const{return spells.LookupEntry(id);}
 bool IsInCombat(Unit*u)const{return u->combat;}bool isMoving(Player*p)const{return p->moving;}
} sServerFacade;
struct PossibleTargetsValue{PossibleTargetsValue(PlayerbotAI*,const char*,float,bool){}std::list<ObjectGuid> Calculate(){std::list<ObjectGuid> out;for(Unit*u:nearby)out.push_back(u->guid);return out;}};
struct PossibleAttackTargetsValue{static bool HasBreakableCC(Unit*u,Player*){return u->cc;}};
struct UseAction:Action {
 PlayerbotAI*ai;Player*bot;Unit*target=nullptr;std::string name;
 enum class InventoryConsumable{Mana,Explosive,Buff};
 explicit UseAction(PlayerbotAI*a,std::string n="use"):ai(a),bot(a->bot),name(n){}
 virtual Unit*GetTarget(){return target;}virtual std::string GetTargetName(){return "";}
 virtual bool UseInventoryItemOnly()const{return false;}
 virtual bool isUseful(){return true;}virtual uint32 GetItemId(){return 0;}
 uint32 FindLowestInventoryConsumable(InventoryConsumable);bool IsUsefulInventoryBuff(const SpellEntry*);bool IsSafeInventoryExplosive(const SpellEntry*,Unit*);
 bool HasItemCooldown(uint32 id)const{return ai->cooldown.count(id)!=0;}
};
struct UseItemIdAction:UseAction{using UseAction::UseAction;bool isUseful()override{return GetItemId()!=0;}};
struct UseTargetedItemIdAction:UseItemIdAction{using UseItemIdAction::UseItemIdAction;};
struct UsePotionAction:UseItemIdAction{UsePotionAction(PlayerbotAI*a,std::string n,int):UseItemIdAction(a,n){}uint32 GetItemId()override{return 999;}bool isUseful()override{return bot->combat&&UseItemIdAction::isUseful();}};
#include "NativeClasses.inc"
#include "NativeConsumables.inc"
struct SayAction:UseAction{using UseAction::UseAction;std::string qualifier="low mana";int warnings=0;bool Execute(Event&);};
#include "NativeWarning.inc"
void check(bool b,const char*m){if(!b){std::cerr<<m<<'\n';std::exit(1);}}
Item item(uint32 id,uint32 level,uint32 spell,uint32 cls=0,uint32 sub=0){Item i;i.proto.ItemId=id;i.proto.ItemLevel=level;i.proto.Spells[0].SpellId=spell;i.proto.Class=cls;i.proto.SubClass=sub;return i;}
SpellEntry manaSpell(){SpellEntry s;s.Id=437;s.Effect[0]=SPELL_EFFECT_ENERGIZE;s.EffectMiscValue[0]=POWER_MANA;return s;}
SpellEntry buffSpell(){SpellEntry s;s.Id=3219;s.Effect[0]=SPELL_EFFECT_APPLY_AURA;s.EffectApplyAuraName[0]=SPELL_AURA_MOD_HEALTH_REGEN_IN_COMBAT;return s;}
SpellEntry bombSpell(){SpellEntry s;s.Id=4054;s.Effect[0]=SPELL_EFFECT_SCHOOL_DAMAGE;s.EffectRadiusIndex[0]=1;s.positive=false;s.aura=false;return s;}
void reset(){sSpellMgr.weak.clear();skipped.clear();attackers.clear();nearby.clear();sServerFacade.spells.data.clear();sServerFacade.spells.data[437]=manaSpell();sServerFacade.spells.data[3219]=buffSpell();sServerFacade.spells.data[4054]=bombSpell();sSpellRangeStore.data[1]={0,30};sSpellRadiusStore.data[1]={5};}
int main(){
 reset();Player p;PlayerbotAI ai(&p);UseAction action(&ai);using K=UseAction::InventoryConsumable;
 Item low=item(2455,15,437),high=item(3827,31,437);ai.items={&high,&low};
 check(action.FindLowestInventoryConsumable(K::Mana)==2455,"generic subclass minor mana, lower tier first");
 low.count=0;check(action.FindLowestInventoryConsumable(K::Mana)==3827,"exhaust lower stock before higher");low.count=32;
 for(int test=0;test<6;++test){
  ai.items={&low,&high};low.trade=false;low.proto.usable=true;low.proto.RequiredLevel=0;low.proto.RequiredSkillRank=0;ai.cooldown.clear();skipped.clear();
  if(test==0)low.trade=true;if(test==1)low.proto.usable=false;if(test==2)low.proto.RequiredLevel=60;
  if(test==3)low.proto.RequiredSkillRank=200;if(test==4)ai.cooldown.insert(2455);if(test==5)skipped.insert(437);
  check(action.FindLowestInventoryConsumable(K::Mana)==(test==5?0u:3827u),"skip unusable low tier and retain spell suppression");
 }
 reset();low=item(2455,15,437);ai.items={&low};p.mana=p.maxMana;
 check(action.FindLowestInventoryConsumable(K::Mana)==0,"no wasted mana at full");p.mana=0;
 ai.items.clear();UseManaPotionAction mp(&ai);check(mp.GetItemId()==0&&mp.UseInventoryItemOnly(),"human bots consume real owned potions even with item cheats");ai.human=false;check(mp.GetItemId()==999&&!mp.UseInventoryItemOnly(),"free bot legacy selection preserved");ai.human=true;
 reset();Item buff=item(3382,8,3219);ai.items={&buff};p.mana=p.maxMana;p.combat=false;
 check(action.FindLowestInventoryConsumable(K::Buff)==3382,"actual Weak Troll Blood data supported");
 p.auras.insert(3219);check(action.FindLowestInventoryConsumable(K::Buff)==0,"do not refresh active buff");p.auras.clear();p.stronger=true;check(action.FindLowestInventoryConsumable(K::Buff)==0,"no weaker replacement");p.stronger=false;
 sSpellMgr.weak={111};p.auras.insert(111);check(action.FindLowestInventoryConsumable(K::Buff)==0,"do not replace an unfinished weaker dose");p.auras.clear();sSpellMgr.weak.clear();
 for(int test=0;test<7;++test){auto s=buffSpell();
  if(test==0)s.positive=false;if(test==1)s.duration=15000;if(test==2)s.Effect[1]=SPELL_EFFECT_SCHOOL_DAMAGE;
  if(test==3)s.EffectApplyAuraName[0]=999;if(test==4){s.EffectApplyAuraName[0]=SPELL_AURA_MOD_STAT;s.EffectMiscValue[0]=STAT_INTELLECT;p.hasMana=false;}
  if(test==5){s.EffectApplyAuraName[0]=SPELL_AURA_MOD_STAT;s.EffectMiscValue[0]=STAT_STRENGTH;ai.ranged=true;}
  if(test==6){s.EffectApplyAuraName[0]=SPELL_AURA_MOD_RESISTANCE;s.EffectMiscValue[0]=4;}
  check(!action.IsUsefulInventoryBuff(&s),"reject harmful, short, situational or irrelevant buffs");p.hasMana=true;ai.ranged=false;
 }
 UseBuffPotionAction bp(&ai);check(bp.isUseful(),"safe preparation can use buff");
 for(int test=0;test<9;++test){p.combat=false;p.mounted=p.sit=p.moving=p.casting=false;p.health=100;p.mana=p.maxMana;ai.human=true;
  if(test==0)p.combat=true;if(test==1)p.mounted=true;if(test==2)p.sit=true;if(test==3)p.moving=true;if(test==4)p.casting=true;if(test==5)p.health=50;if(test==6)p.mana=1;if(test==7)ai.human=false;if(test==8)p.alive=false;
  check(!bp.isUseful(),"do not buff during recovery, casting, combat, movement or legacy free bot");p.alive=true;
 }
 reset();Player engineer;engineer.skill=1;PlayerbotAI ea(&engineer);Unit enemy;enemy.x=10;enemy.guid=7;
 Item rough=item(4358,10,4054,7,2),coarse=item(4365,20,4054,7,2);rough.proto.RequiredSkillRank=1;coarse.proto.RequiredSkillRank=75;
 ea.items={&coarse,&rough};ThrowGrenadeAction grenade(&ea);grenade.target=&enemy;nearby={&enemy};attackers={7};
 check(grenade.GetItemId()==4358&&grenade.isUseful(),"low level stationary target allows owned Rough Dynamite");
 for(int test=0;test<9;++test){engineer.los=true;enemy.x=10;enemy.alive=enemy.combat=true;enemy.cc=false;ea.attackAllowed=ea.spellAllowed=true;nearby={&enemy};attackers={7};Unit extra;extra.x=12;extra.guid=8;
  if(test==0)engineer.los=false;if(test==1)enemy.x=31;if(test==2)enemy.alive=false;if(test==3)enemy.combat=false;if(test==4)enemy.cc=true;if(test==5)ea.attackAllowed=false;if(test==6)ea.spellAllowed=false;if(test==7)nearby.push_back(&extra);if(test==8){extra.cc=true;nearby.push_back(&extra);attackers.push_back(8);}
  check(grenade.GetItemId()==0,"respect LOS/range/focus/CC and avoid extra pulls");
 }
 engineer.heal=true;check(!grenade.isUseful(),"healer does not throw explosives");engineer.heal=false;
 ea.human=false;check(!grenade.isUseful(),"legacy low-level grenade restriction retained for free bots");
 reset();Player caster;PlayerbotAI ca(&caster);SayAction say(&ca);Event event;ca.nestedSuccess=true;ca.nested.duration=2200;
 check(say.Execute(event)&&ca.attempts==1&&say.warnings==0&&say.GetDuration()==2200,"native accepted potion precedes warning and keeps cast duration");
 ca.nestedSuccess=false;check(!say.Execute(event)&&say.warnings==1,"unavailable/failed potion retains warning");
 say.qualifier="critical health";int before=ca.attempts;say.Execute(event);check(ca.attempts==before,"other warnings unchanged");
 say.qualifier="low mana";caster.combat=false;say.Execute(event);check(ca.attempts==before,"no warning-side potion use outside combat");
 std::cout<<"Native inventory selection, buff, explosive safety and warning tests passed\n";
}
