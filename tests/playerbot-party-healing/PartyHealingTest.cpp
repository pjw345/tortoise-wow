#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using uint8=uint8_t;using uint32=uint32_t;using uint64=uint64_t;using int32=int32_t;
constexpr int MAX_EFFECT_INDEX=3,SPELL_STATE_PREPARING=1,SPELL_STATE_CASTING=2,SPELL_STATE_FINISHED=3;
constexpr int UNIT_FIELD_MINDAMAGE=1,UNIT_FIELD_MAXDAMAGE=2,UNIT_FIELD_FLAGS=3,UNIT_FLAG_SILENCED=4,UNIT_STAT_CAN_NOT_REACT=1;
constexpr int SPELL_EFFECT_HEAL=10,POWER_MANA=0,HEAL=0,SPELLFAMILY_PALADIN=10,SPELLCATEGORY_RANGED_WEAPON=351,RANGED_ATTACK=0;
enum SpellEffectIndex { EFFECT_INDEX_0,EFFECT_INDEX_1,EFFECT_INDEX_2 };
enum CurrentSpellTypes { CURRENT_MELEE_SPELL,CURRENT_GENERIC_SPELL,CURRENT_AUTOREPEAT_SPELL,CURRENT_CHANNELED_SPELL,CURRENT_MAX_SPELL };
enum class BotState { BOT_STATE_COMBAT,BOT_STATE_NON_COMBAT };
struct ObjectGuid { int id=0;bool operator!=(ObjectGuid b)const{return id!=b.id;}bool operator==(ObjectGuid b)const{return id==b.id;} };
struct SpellEntry
{
    uint32 Id=1,SpellFamilyName=0,SpellIconID=0,Category=0,rangeIndex=1;int powerType=0;uint32 cost=100;
    int Effect[3]={SPELL_EFFECT_HEAL,0,0};bool channel=false;
    bool IsChanneledSpell()const{return channel;}
    bool HasEffect(int value)const{for(int e:Effect)if(e==value)return true;return false;}
};
struct SpellRangeEntry {float maxRange=30;};
struct Spell;struct Player;struct Group;
struct Unit
{
    void* map=reinterpret_cast<void*>(1);bool world=true,alive=true,los=true,reach=true,player=false;
    uint32 hp=700,maxHp=1000,mana=700,maxMana=1000;float hit=100;int id=0;Unit* victim=nullptr;
    std::set<Unit*> attackers;
    void* FindMap(){return map;}bool IsInWorld(){return world;}bool IsAlive(){return alive;}
    uint32 GetHealth(){return hp;}uint32 GetMaxHealth(){return maxHp;}float GetHealthPercent(){return 100.0f*hp/maxHp;}
    uint32 GetPower(int){return mana;}bool IsWithinLOSInMap(Unit* u){return u->los;}
    Unit* GetVictim(){return victim;}bool CanReachWithMeleeAttack(Unit*){return reach;}
    float GetFloatValue(int){return hit;}std::set<Unit*>& getAttackers(){return attackers;}
    ObjectGuid GetObjectGuid(){return {id};}bool IsPlayer(){return player;}Player* ToPlayer(){return reinterpret_cast<Player*>(this);}
    bool IsWithinDistInMap(Unit*,float){return reach;}
    uint32 SpellHealingBonusTaken(Player*,SpellEntry const*,SpellEffectIndex,uint32 n,int,uint32=1,Spell* s=nullptr){if(s)throw std::runtime_error("prediction got live Spell");return n;}
};
struct Spell
{
    SpellEntry* m_spellInfo=nullptr;int state=SPELL_STATE_PREPARING;uint32 timer=2500;int32 m_currentBasePoints[3]={499,0,0};
    struct Targets { ObjectGuid guid;Unit* patient=nullptr;ObjectGuid getUnitTargetGuid(){return guid;}Unit* getUnitTarget(){return patient;} } m_targets;
    int getState(){return state;}uint32 GetCastedTime(){return timer;}uint32 GetPowerCost(){return m_spellInfo->cost;}
    uint32 GetDamage(){return 0;}bool CanBeInterrupted(){return state==SPELL_STATE_PREPARING;}
    static uint32 CalculatePowerCost(SpellEntry const* s,Player*){return s->cost;}
};
struct Player:Unit
{
    Player(){player=true;}bool healer=false,tank=false,stunned=false,silenced=false,ready=true;
    Group* group=nullptr;std::map<uint32,int> known;std::map<int,Spell*> casts;int cancels=0;
    Group* GetGroup(){return group;}Spell* GetCurrentSpell(CurrentSpellTypes slot){return casts[slot];}
    std::map<uint32,int>const& GetSpellMap(){return known;}bool HasSpell(uint32 id){return known.count(id);}
    bool IsSpellReady(uint32){return ready;}bool HasUnitState(int){return stunned;}bool HasFlag(int,int){return silenced;}
    bool IsInCombat(){return true;}uint32 GetAttackTimer(int){return 0;}
    void InterruptSpell(CurrentSpellTypes slot){if(casts[slot])casts[slot]->m_spellInfo=nullptr;casts[slot]=nullptr;++cancels;}
    uint32 SpellHealingBonusDone(Unit*,SpellEntry const*,SpellEffectIndex,int32 n,int,uint32=1,Spell* s=nullptr){if(s)throw std::runtime_error("prediction got live Spell");return n;}
};
struct GroupReference {Player* member=nullptr;GroupReference* nextRef=nullptr;Player* getSource(){return member;}GroupReference* next(){return nextRef;}};
struct Group {GroupReference* first=nullptr;GroupReference* GetFirstMember(){return first;}};
struct PlayerbotAI
{
    Player* bot=nullptr;bool owned=true,real=false,pvp=false,offheal=false;uint32 interrupted=0;
    bool HasActivePlayerMaster(){return owned;}bool IsRealPlayer(){return real;}bool IsInPvp(){return pvp;}
    bool HasStrategy(std::string name,BotState){return offheal&&name=="offheal";}
    static bool IsHeal(Player* p){return p->healer;}bool IsTank(Player* p){return p->tank;}
    static bool IsHealSpell(SpellEntry const* s){return s->HasEffect(SPELL_EFFECT_HEAL)||(s->SpellFamilyName==SPELLFAMILY_PALADIN&&(s->SpellIconID==70||s->SpellIconID==242));}
    void SpellInterrupted(uint32 id){interrupted=id;}void TellPlayerNoFacing(Player*,std::string){}
    bool UsesCoordinatedPartyHealing();uint32 GetPartyHealingPressure(Unit*);uint32 GetPendingPartyHealing(Unit*);
    bool IsPartyHealCovered(Unit*);bool IsBackupPartyHealAllowed(Unit*);
};
struct Facade {std::map<uint32,SpellEntry*> spells;SpellRangeEntry range;SpellEntry const* LookupSpellInfo(uint32 id){return spells[id];}SpellRangeEntry const* LookupSpellRangeEntry(uint32){return &range;}}sServerFacade;
struct {uint32 criticalHealth=20,globalCoolDown=1500,reactDelay=100;}sPlayerbotAIConfig;
struct Event {};
struct CastAuraSpellAction {bool baseUseful=true;int executed=0;bool isUseful(){return baseUseful;}bool Execute(Event&){++executed;return true;}};
struct CastHealingSpellAction:CastAuraSpellAction {PlayerbotAI* ai;Player* bot;Unit* patient;uint32 GetSpellID(){return 1;}Unit* GetTarget(){return patient;}bool isUseful();bool Execute(Event&);};
struct HealTargetFullHealthTrigger {PlayerbotAI* ai;Player* bot;HealTargetFullHealthTrigger(PlayerbotAI* a):ai(a),bot(a->bot){}Player* GetMaster(){return nullptr;}bool IsActive();};
struct InterruptCurrentSpellAction {PlayerbotAI* ai;Player* bot;std::string qualifier="heal";std::string GetQualifier(){return qualifier;}std::string getQualifier(){return qualifier;}void SetDuration(uint32){}bool isUseful();bool Execute(Event&);};
#include "NativeHealing.inc"
void check(bool ok,char const* reason){if(!ok)throw std::runtime_error(reason);}
int main(){try{
    Player a,b,patient; a.id=1;b.id=2;patient.id=3;a.healer=b.healer=true;
    Group g;GroupReference ra{&a},rb{&b},rp{&patient};ra.nextRef=&rb;rb.nextRef=&rp;g.first=&ra;a.group=b.group=patient.group=&g;
    PlayerbotAI ai;ai.bot=&a;
    SpellEntry heal;Spell pending;pending.m_spellInfo=&heal;pending.m_targets.patient=&patient;pending.m_targets.guid=patient.GetObjectGuid();b.casts[CURRENT_GENERIC_SPELL]=&pending;
    check(ai.UsesCoordinatedPartyHealing(),"owned party scope");
    check(ai.GetPendingPartyHealing(&patient)==500&&ai.IsPartyHealCovered(&patient),"other heal covers deficit");
    Unit attacker;attacker.victim=&patient;patient.attackers.insert(&attacker);attacker.hit=300;
    check(!ai.IsPartyHealCovered(&patient),"incoming pressure permits second heal");
    attacker.hit=100;check(ai.IsPartyHealCovered(&patient),"coverage includes pressure allowance");
    patient.hp=100;check(!ai.IsPartyHealCovered(&patient),"critical rescue never reserved away");patient.hp=700;
    pending.m_currentBasePoints[0]=99;check(!ai.IsPartyHealCovered(&patient),"partial coverage permits help");pending.m_currentBasePoints[0]=499;
    b.map=reinterpret_cast<void*>(2);check(ai.GetPendingPartyHealing(&patient)==0,"foreign map never read");b.map=a.map;
    b.alive=false;check(ai.GetPendingPartyHealing(&patient)==0,"dead caster not coverage");b.alive=true;
    b.mana=0;check(ai.GetPendingPartyHealing(&patient)==0,"unaffordable pending heal");b.mana=700;
    pending.m_spellInfo->channel=true;check(ai.GetPendingPartyHealing(&patient)==0,"channel not single pending heal");pending.m_spellInfo->channel=false;
    heal.Effect[0]=0;check(ai.GetPendingPartyHealing(&patient)==0,"dummy and shields not invented coverage");heal.Effect[0]=SPELL_EFFECT_HEAL;
    ai.owned=false;check(!ai.IsPartyHealCovered(&patient),"free bot policy unchanged");ai.owned=true;
    ai.pvp=true;check(!ai.IsPartyHealCovered(&patient),"PvP policy unchanged");ai.pvp=false;
    a.healer=false;ai.offheal=true;b.casts.clear();b.known[1]=1;sServerFacade.spells[1]=&heal;
    check(!ai.IsBackupPartyHealAllowed(&patient),"ready priest owns routine heal");
    b.mana=499;check(!ai.IsBackupPartyHealAllowed(&patient),"primary below 50 percent may still heal");
    b.mana=1;check(ai.IsBackupPartyHealAllowed(&patient),"unaffordable primary permits backup");b.mana=700;
    b.silenced=true;check(ai.IsBackupPartyHealAllowed(&patient),"silenced primary permits backup");b.silenced=false;
    b.reach=false;check(ai.IsBackupPartyHealAllowed(&patient),"out of range primary permits backup");b.reach=true;
    b.casts[CURRENT_GENERIC_SPELL]=&pending;check(ai.IsBackupPartyHealAllowed(&patient),"busy primary permits needed supplement");b.casts.clear();
    patient.hp=100;check(ai.IsBackupPartyHealAllowed(&patient),"emergency overrides backup preference");patient.hp=700;
    // Actual selector prioritises pressure at equal health and keeps emergency overrides.
    Player tank,mage;tank.hp=mage.hp=500;tank.id=4;mage.id=5;tank.tank=true;
    Unit x,y,z;x.victim=y.victim=z.victim=&tank;tank.attackers={&x,&y,&z};
    check(Select(&ai,&a,{&mage,&tank})==&tank,"tank under three attackers outranks idle mage");
    mage.hp=100;check(Select(&ai,&a,{&tank,&mage})==&mage,"critical caster overrides healthy tank");
    mage.hp=500;tank.hp=1000;check(Select(&ai,&a,{&tank,&mage})==&mage,"full health tank not stale candidate");
    // Prepared Heal, cancellation, and a stale queued reaction after fresh damage.
    a.healer=true;patient.attackers.clear();patient.hp=1000;Spell active;active.m_spellInfo=&heal;active.m_targets.patient=&patient;a.casts[CURRENT_GENERIC_SPELL]=&active;
    HealTargetFullHealthTrigger trigger(&ai);check(trigger.IsActive(),"ordinary PREPARING cast cancels at full health");
    InterruptCurrentSpellAction cancel{&ai,&a};Event event;patient.hp=700;
    check(!cancel.Execute(event)&&a.cancels==0,"stale reaction must not cancel needed heal");
    patient.hp=1000;check(cancel.Execute(event)&&ai.interrupted==1&&a.cancels==1,"native cancellation copies ID before invalidation");
    heal.Id=1;active.m_spellInfo=&heal;active.timer=0;a.casts[CURRENT_GENERIC_SPELL]=&active;
    check(!trigger.IsActive(),"completed/instant cast not cancelled");active.timer=2500;
    CastHealingSpellAction action;action.ai=&ai;action.bot=&a;action.patient=&patient;
    check(!action.isUseful(),"full health direct heal not queued");patient.hp=700;b.casts[CURRENT_GENERIC_SPELL]=&pending;
    check(!action.Execute(event)&&action.executed==0,"actual execution rechecks incoming coverage");
    pending.m_currentBasePoints[0]=49;check(action.Execute(event),"partial heal can be supplemented");
    heal.Effect[0]=0;heal.SpellFamilyName=SPELLFAMILY_PALADIN;heal.SpellIconID=70;a.healer=false;
    b.casts.clear();SpellEntry priestHeal;priestHeal.Id=2;b.known[2]=1;sServerFacade.spells[2]=&priestHeal;
    check(!action.isUseful(),"Turtle dummy-effect Paladin heal obeys backup gate");
    ai.owned=false;check(action.isUseful(),"legacy free-bot action path preserved");
    std::cout<<"Native healing coverage, urgency, backup availability and cancellation cases passed\n";
}catch(std::exception const& error){std::cerr<<error.what()<<'\n';return 1;}}
