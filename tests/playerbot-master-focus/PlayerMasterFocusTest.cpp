#include <cstdint>
#include <list>
#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <iostream>
#include <stdexcept>
using uint32=uint32_t; using uint8=uint8_t;
struct ObjectGuid { int value=0; ObjectGuid()=default; explicit ObjectGuid(int n):value(n){} bool operator==(ObjectGuid b)const{return value==b.value;} bool operator!=(ObjectGuid b)const{return !(*this==b);} };
enum ReactStates { REACT_PASSIVE, REACT_DEFENSIVE, REACT_AGGRESSIVE };
enum CurrentSpellTypes { CURRENT_MELEE_SPELL, CURRENT_GENERIC_SPELL, CURRENT_AUTOREPEAT_SPELL, CURRENT_CHANNELED_SPELL, CURRENT_MAX_SPELL };
constexpr int SPELL_EFFECT_INTERRUPT_CAST=68, UNIT_STAT_FLEEING=1, MAX_EFFECT_INDEX=3;
namespace Spells {bool IsDirectDamageEffect(int e){return e==2;}bool IsEffectAppliesAura(int e){return e==6;}bool IsDamagingAuraEffect(int a){return a==3;}}
constexpr float PET_FOLLOW_DIST=2, PET_FOLLOW_ANGLE=1.57f;
struct SpellEntry { uint32 Id=1,SpellFamilyName=0,rangeIndex=1; int Effect[3]={},EffectApplyAuraName[3]={}; bool wand=false; bool IsAutoRepeatRangedSpell()const{return wand;} bool positive=false,area=false,interrupt=false; bool IsAreaOfEffectSpell()const{return area;} };
bool IsPositiveSpell(SpellEntry const* s){return s->positive;}
bool IsSpellHaveEffect(SpellEntry const* s,int){return s->interrupt;}
struct Spell { SpellEntry* m_spellInfo=nullptr; bool canInterrupt=true; struct { ObjectGuid guid; ObjectGuid getUnitTargetGuid(){return guid;} }m_targets; bool CanBeInterrupted(){return canInterrupt;} };
struct Motion { int followed=0; template<class T> void MoveFollow(T*,float,float){++followed;} };
struct CreatureAI { ReactStates react=REACT_DEFENSIVE; ReactStates GetReactState(){return react;} void SetReactState(ReactStates r){react=r;} };
struct Pet; struct Player;
enum class BotState { BOT_STATE_COMBAT };
constexpr int CLASS_PALADIN=2,SPELLFAMILY_PALADIN=10,SPELL_AURA_MOD_STUN=12,SPELL_AURA_MOD_ROOT=26;
struct Unit { bool rooted=false,stunned=false,feared=false,combat=true;bool IsStunned(){return stunned;}bool IsInCombat(){return combat;}bool HasAuraType(int){return rooted;}bool isPlayer=false,meleeReach=true; bool IsPlayer(){return isPlayer;} Player* ToPlayer(){return reinterpret_cast<Player*>(this);} bool IsAlive(){return alive;} void* FindMap(){return map;} bool CanReachWithMeleeAttack(Unit*){return meleeReach;} ObjectGuid guid; bool alive=true,friendly=false,safe=true,world=true,near=true,los=true,cc=false,attackable=true,fleeing=false; void* map=reinterpret_cast<void*>(1); bool nearMaster=true;Unit* chase=nullptr; Unit* victim=nullptr; std::string name="Add"; int stops=0; bool HasUnitState(int){return fleeing;} bool IsWithinLOSInMap(Unit*u){return u->los;} Unit* GetVictim(){return victim;} void AttackStop(){victim=nullptr;++stops;} void* GetMap(){return map;} bool IsInWorld(){return world;} bool IsWithinDistInMap(Unit*,float distance,bool=false){return near&&(distance>10||nearMaster);} ObjectGuid GetObjectGuid(){return guid;} char const* GetName(){return name.c_str();} };
struct Player : Unit { Player(){isPlayer=true;}void* GetGroup(){return grouped?this:nullptr;} int cls=1; int getClass(){return cls;} bool tank=false; bool healer=false,hasMana=true; float manaPercent=100; bool HasMana(){return hasMana;}float GetPowerPercent(){return manaPercent;}ObjectGuid selection; ObjectGuid GetSelectionGuid(){return selection;} bool grouped=true; Pet* pet=nullptr; std::set<Unit*> attackers; std::map<int,Spell*> spells; bool IsInGroup(Player*){return grouped;} Pet* GetPet(){return pet;} std::set<Unit*>& getAttackers(){return attackers;} Spell* GetCurrentSpell(CurrentSpellTypes t){return spells[t];} void InterruptSpell(CurrentSpellTypes t){spells[t]=nullptr;} };
struct Pet:Unit { CreatureAI brain; Motion motion; CreatureAI* AI(){return &brain;} Motion* GetMotionMaster(){return &motion;} };
struct PossibleAttackTargetsValue {static bool HasBreakableCC(Unit*u,Player*){return u->cc;}static bool HasUnBreakableCC(Unit*u,Player*){return u->cc;}static bool IsPossibleTarget(Unit*u,Player*,float,bool ignoreCC){return u->near&&u->attackable&&(ignoreCC||(!u->cc&&!u->stunned&&!u->rooted&&!u->feared));}};
struct SpellRangeEntry {float maxRange=10;};
struct Facade { SpellRangeEntry range;SpellRangeEntry const* LookupSpellRangeEntry(uint32){return &range;} Unit* GetChaseTarget(Unit*u){return u->chase;}  bool IsFeared(Unit* u){return u->feared;}bool IsInRoots(Unit* u){return u->rooted;}bool IsAlive(Unit* u){return u->alive;} bool IsFriendlyTo(Unit*,Unit* u){return u->friendly;} }sServerFacade;
struct Config { bool assistPlayerMaster=true; float sightDistance=60; }sPlayerbotAIConfig;
struct WorldTimer { static uint32 now; static uint32 getMSTime(){return now;} static uint32 getMSTimeDiff(uint32 a,uint32 b){return b-a;} }; uint32 WorldTimer::now=100;
enum class PlayerbotSecurityLevel { PLAYERBOT_SECURITY_ALLOW_ALL };
template<class T> struct Value { T value{}; int resets=0; T Get(){return value;} void Set(T v){value=v;} void Reset(){++resets;} };
struct Context { std::map<std::string,Value<Unit*>> units; Value<ObjectGuid> attack; Value<std::list<ObjectGuid>> attackers; template<class T> Value<T>* GetValue(std::string n); };
template<> Value<Unit*>* Context::GetValue<Unit*>(std::string n){return &units[n];}
template<> Value<ObjectGuid>* Context::GetValue<ObjectGuid>(std::string){return &attack;}
template<> Value<std::list<ObjectGuid>>* Context::GetValue<std::list<ObjectGuid>>(std::string){return &attackers;}
struct PlayerbotAI { Player *bot=nullptr,*master=nullptr; Context storage; Context* aiObjectContext=&storage; bool activeMaster=true,pvp=false,real=false,silent=false,ranged=false,offheal=false; bool HasStrategy(std::string,BotState){return offheal;} bool IsMelee(Player*){return !ranged;} static bool IsTank(Player*p){return p->tank;} bool playerMasterAggroObserved=false; uint32 playerMasterAggroStartedMs=0; uint32 lastPlayerMasterAggroWarningMs=0; ObjectGuid playerMasterPursuitTarget; int movementStops=0; ObjectGuid playerMasterFocusPet; uint8 playerMasterFocusPetReact=0; std::map<int,Unit*> world; std::vector<std::string> warnings; int interrupted=0;
 bool HasActivePlayerMaster(){return master && activeMaster;} bool IsRealPlayer(){return real;} bool IsInPvp(){return pvp;} bool IsSafe(Player* p){return p&&p->safe;} static bool IsSafe(Player*,Unit* p){return p&&p->safe;}
 Unit* GetUnit(ObjectGuid g){return world[g.value];} void SpellInterrupted(uint32){++interrupted;}
 bool TellPlayerNoFacing(Player*,std::string text,PlayerbotSecurityLevel,bool,bool){if(silent)return false;warnings.push_back(text);return true;}
 static bool IsHeal(Player*p){return p->healer;} bool IsRanged(Player*){return ranged;} void StopMoving(){++movementStops;bot->chase=nullptr;playerMasterPursuitTarget=ObjectGuid();} bool CanPlayerMasterChase(Unit*);Unit* GetPlayerMasterFleeingControlTarget(SpellEntry const*);
 float GetRange(std::string){return 5;}bool ShouldAssistPlayerMaster(); Unit* GetPlayerMasterAttackTarget(); bool IsPlayerMasterAttackAllowed(Unit*); bool IsPlayerMasterSpellAllowed(SpellEntry const*,Unit*); void UpdatePlayerMasterAssist();
};
#include "NativeMasterFocus.inc"
struct Trigger {PlayerbotAI* ai;Player* bot;Trigger(PlayerbotAI*a,char const*,int):ai(a),bot(a->bot){}virtual bool IsActive(){return false;}};
#include "NativeBlink.inc"
void check(bool b,char const* label){if(!b)throw std::runtime_error(label);}
int main(){try{
 Player bot,master; Unit primary,add; primary.guid=ObjectGuid(1); add.guid=ObjectGuid(2); PlayerbotAI ai; ai.bot=&bot;ai.master=&master;ai.world[1]=&primary;ai.world[2]=&add;
 BlinkTrigger blink(&ai);bot.rooted=true;
 check(!blink.IsActive(),"owned PvE mage does not automatically Blink into unknown landing area");
 ai.activeMaster=false;check(blink.IsActive(),"free-bot Blink retained");ai.activeMaster=true;
 ai.pvp=true;check(blink.IsActive(),"PvP Blink retained");ai.pvp=false;
 ai.real=true;check(blink.IsActive(),"real-player Blink retained");ai.real=false;bot.rooted=false;
 SpellEntry stun;stun.SpellFamilyName=SPELLFAMILY_PALADIN;stun.Effect[0]=6;stun.EffectApplyAuraName[0]=SPELL_AURA_MOD_STUN;
 ai.storage.attackers.value={primary.guid};primary.fleeing=true;
 check(ai.GetPlayerMasterFleeingControlTarget(&stun)==&primary,"near engaged runner can be controlled");
 check(ai.IsPlayerMasterSpellAllowed(&stun,&primary),"pure stun allowed without melee pursuit");
 check(!ai.CanPlayerMasterChase(&primary)&&!ai.IsPlayerMasterAttackAllowed(&primary),"stun exception never enables chase or melee damage");
 primary.near=false;check(!ai.GetPlayerMasterFleeingControlTarget(&stun),"distant runner cannot recruit movement");primary.near=true;
 primary.cc=true;check(!ai.GetPlayerMasterFleeingControlTarget(&stun),"existing crowd control protected");primary.cc=false;
 stun.Effect[1]=2;check(!ai.GetPlayerMasterFleeingControlTarget(&stun),"damage spell cannot use pure control exception");stun.Effect[1]=0;
 ai.storage.attackers.value.clear();check(!ai.GetPlayerMasterFleeingControlTarget(&stun),"idle hostile cannot become runner target");primary.fleeing=false;
 // Attack toggle before combat contact must not recruit a 35-yard chase.
 master.selection=primary.guid;master.victim=&primary;primary.combat=false;
 check(!ai.GetPlayerMasterAttackTarget()&&!ai.CanPlayerMasterChase(&primary),"selected attack intent cannot pull an unengaged enemy");
 ai.storage.attackers.value={primary.guid};check(!ai.GetPlayerMasterAttackTarget(),"stale attacker cache cannot reopen unengaged pull");ai.storage.attackers.value.clear();
 primary.combat=true;primary.nearMaster=false;
 check(ai.GetPlayerMasterAttackTarget()==&primary&&!ai.CanPlayerMasterChase(&primary),"distant engaged target may be attacked from position but not pursued ahead of tank");
 ai.storage.units["current target"].value=&primary;bot.victim=&primary;bot.chase=&primary;
 int leashStops=ai.movementStops;ai.UpdatePlayerMasterAssist();
 check(ai.movementStops==leashStops+1&&!bot.chase&&bot.victim==&primary,"active chase stops at leash while stationary defense survives");
 primary.nearMaster=true;check(ai.CanPlayerMasterChase(&primary),"engaged enemy near tank permits ordinary melee positioning");
 // Stun is not permission to break Polymorph/fear/roots or start a pull.
 ai.ranged=true;primary.fleeing=true;primary.stunned=true;
 check(ai.GetPlayerMasterAttackTarget()==&primary&&ai.IsPlayerMasterSpellAllowed(&stun,&primary),"ranged finisher retains stunned runner as damage focus");
 check(!ai.CanPlayerMasterChase(&primary),"stunned runner still grants no pursuit");
 primary.cc=true;check(!ai.GetPlayerMasterAttackTarget(),"breakable CC remains protected even with stun");primary.cc=false;
 primary.feared=true;check(!ai.GetPlayerMasterAttackTarget(),"fear with stun remains protected");primary.feared=false;
 primary.rooted=true;check(!ai.GetPlayerMasterAttackTarget(),"root with stun retains existing protection");primary.rooted=false;
 ai.ranged=false;check(ai.IsPlayerMasterAttackAllowed(&primary),"adjacent stunned runner can be hit without movement");
 bot.meleeReach=false;check(!ai.IsPlayerMasterAttackAllowed(&primary),"melee never gains ranged reach from a stun");bot.meleeReach=true;
 primary.stunned=false;check(!ai.IsPlayerMasterAttackAllowed(&primary),"stun expiry restores melee runner restriction");
 primary.fleeing=false;master.selection=ObjectGuid();master.victim=nullptr;bot.victim=nullptr;ai.storage.units["current target"].value=nullptr;
 check(ai.ShouldAssistPlayerMaster(),"grouped human PvE policy");
 check(!ai.GetPlayerMasterAttackTarget(),"selection alone cannot pull");
 check(!ai.IsPlayerMasterAttackAllowed(&add),"no victim forbids independent add");
 master.victim=&primary;check(ai.GetPlayerMasterAttackTarget()==&primary,"actual master victim");
 check(ai.IsPlayerMasterAttackAllowed(&primary)&&!ai.IsPlayerMasterAttackAllowed(&add),"focus only");
 primary.alive=false;check(!ai.GetPlayerMasterAttackTarget(),"dead target");primary.alive=true;
 primary.friendly=true;check(!ai.GetPlayerMasterAttackTarget(),"friendly target");primary.friendly=false;
 primary.safe=false;check(!ai.GetPlayerMasterAttackTarget(),"unsafe target");primary.safe=true;
 primary.map=reinterpret_cast<void*>(2);check(!ai.GetPlayerMasterAttackTarget(),"other instance");primary.map=bot.map;
 primary.near=false;check(!ai.GetPlayerMasterAttackTarget(),"out of sight range");primary.near=true;
 master.alive=false;
 check(!ai.ShouldAssistPlayerMaster()&&!ai.GetPlayerMasterAttackTarget(),"dead or ghost master leaves focus mode");
 check(ai.IsPlayerMasterAttackAllowed(&add),"dead leader restores native attack permission");
 SpellEntry defensiveDamage;check(ai.IsPlayerMasterSpellAllowed(&defensiveDamage,&add),"dead leader restores native damage permission");
 ai.storage.units["current target"].value=&add;bot.victim=&add;
 ai.UpdatePlayerMasterAssist();check(bot.victim==&add&&ai.storage.units["current target"].value==&add,"dead leader does not repeatedly erase native combat target");
 master.alive=true;check(ai.ShouldAssistPlayerMaster()&&!ai.IsPlayerMasterAttackAllowed(&add),"revived leader restores focus restriction");
 bot.grouped=false;check(!ai.ShouldAssistPlayerMaster(),"solo unchanged");bot.grouped=true;
 ai.activeMaster=false;check(!ai.ShouldAssistPlayerMaster(),"bot-led unchanged");ai.activeMaster=true;
 ai.pvp=true;check(!ai.ShouldAssistPlayerMaster()&&ai.IsPlayerMasterAttackAllowed(&add),"PvP unchanged");ai.pvp=false;
 sPlayerbotAIConfig.assistPlayerMaster=false;check(!ai.ShouldAssistPlayerMaster(),"config opt out");sPlayerbotAIConfig.assistPlayerMaster=true;
 SpellEntry damage,heal,aoe,kick;heal.positive=true;aoe.area=true;kick.interrupt=true;
 check(ai.IsPlayerMasterSpellAllowed(&damage,&primary),"focused damage allowed");
 check(!ai.IsPlayerMasterSpellAllowed(&damage,&add),"secondary damage denied");
 check(ai.IsPlayerMasterSpellAllowed(&heal,&bot),"healing preserved");
 check(ai.IsPlayerMasterSpellAllowed(&kick,&add),"defensive interrupt preserved");
 check(ai.IsPlayerMasterSpellAllowed(&aoe,&primary),"area damage on player target allowed");
 check(ai.IsPlayerMasterSpellAllowed(&aoe,&bot)&&ai.IsPlayerMasterSpellAllowed(&aoe,nullptr),"caster-centred and ground area casts allowed");
 check(!ai.IsPlayerMasterSpellAllowed(&aoe,&add),"area spell cannot select attacking add instead");
 master.victim=nullptr;check(!ai.IsPlayerMasterSpellAllowed(&aoe,&bot)&&!ai.IsPlayerMasterSpellAllowed(&aoe,nullptr),"no independent area pull without player attack");master.victim=&primary;
 Spell pendingArea;pendingArea.m_spellInfo=&aoe;pendingArea.m_targets.guid=primary.guid;bot.spells[CURRENT_AUTOREPEAT_SPELL]=&pendingArea;
 Spell pendingDamage,pendingHeal;pendingDamage.m_spellInfo=&damage;pendingDamage.m_targets.guid=add.guid;pendingHeal.m_spellInfo=&heal;bot.spells[CURRENT_GENERIC_SPELL]=&pendingDamage;bot.spells[CURRENT_CHANNELED_SPELL]=&pendingHeal;
 ai.storage.units["current target"].value=&add;bot.victim=&add;add.victim=&bot;bot.attackers.insert(&add);
 Pet pet;pet.guid=ObjectGuid(3);pet.victim=&add;bot.pet=&pet;
 ai.UpdatePlayerMasterAssist();check(bot.victim==nullptr&&ai.storage.units["current target"].value==nullptr,"stale target and autoattack cleared");
 check(ai.storage.units["dps target"].resets==1&&ai.storage.units["tank target"].resets==1,"native selectors invalidated");
 check(ai.interrupted==1&&bot.spells[CURRENT_CHANNELED_SPELL]==&pendingHeal,"wrong damage interrupted; healing preserved");
 check(bot.spells[CURRENT_AUTOREPEAT_SPELL]==&pendingArea,"focused area cast continues despite attacking add");
 check(!pet.victim&&pet.motion.followed==1&&pet.brain.react==REACT_PASSIVE,"pet stopped and autonomous acquisition disabled");
 check(ai.warnings.empty(),"first attack does not warn immediately");
 WorldTimer::now=2099;ai.UpdatePlayerMasterAssist();check(ai.warnings.empty(),"no warning before two seconds");
 WorldTimer::now=2100;ai.UpdatePlayerMasterAssist();
 check(ai.warnings.size()==1&&ai.warnings[0].find("I'm being attacked by Add")!=std::string::npos,"warning at two seconds");
 ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==1,"no per-tick warning spam");
 WorldTimer::now=12099;ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==1,"reminder waits ten seconds");
 WorldTimer::now=12100;ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==2,"bounded reminder");
 bot.attackers.clear();ai.UpdatePlayerMasterAssist();check(!ai.playerMasterAggroObserved&&ai.lastPlayerMasterAggroWarningMs==0,"warning resets after aggro ends");
 bot.attackers.insert(&add);WorldTimer::now=13000;ai.UpdatePlayerMasterAssist();
 bot.attackers.clear();WorldTimer::now=14000;ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==2,"brief attack remains quiet");
 bot.attackers.insert(&add);WorldTimer::now=15000;ai.UpdatePlayerMasterAssist();
 WorldTimer::now=16999;ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==2,"new attack restarts delay");
 WorldTimer::now=17000;ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==3,"new sustained attack warns");
 bot.attackers.clear();ai.UpdatePlayerMasterAssist();
 WorldTimer::now=UINT32_MAX-999;bot.attackers.insert(&add);ai.UpdatePlayerMasterAssist();
 WorldTimer::now=999;ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==3,"timer wrap before delay");
 WorldTimer::now=1000;ai.UpdatePlayerMasterAssist();check(ai.warnings.size()==4,"timer wrap completes delay");
 ai.pvp=true;ai.UpdatePlayerMasterAssist();check(!ai.playerMasterAggroObserved,"leaving assist scope resets delay");ai.pvp=false;
 bot.attackers.clear();ai.UpdatePlayerMasterAssist();
 ai.storage.units["current target"].value=&primary;bot.victim=&primary;master.victim=nullptr;ai.UpdatePlayerMasterAssist();check(!bot.victim,"stop when player stops");
 // Defensive fallbacks and selected/fleeing priority use native group attackers.
 master.victim=nullptr;master.selection=ObjectGuid();ai.storage.attackers.value={add.guid};
 ai.storage.units["current target"].value=nullptr;
 check(ai.GetPlayerMasterAttackTarget()==&add&&ai.IsPlayerMasterAttackAllowed(&add),"no master target permits self/group defense");
 check(ai.IsPlayerMasterSpellAllowed(&damage,&add)&&ai.IsPlayerMasterSpellAllowed(&aoe,&bot),"defensive damage including area allowed");
 master.selection=primary.guid;check(ai.GetPlayerMasterAttackTarget()==&add,"idle selected enemy does not prevent defense or cause pull");
 ai.storage.attackers.value.push_back(primary.guid);
 check(ai.GetPlayerMasterAttackTarget()==&primary,"selected engaged group enemy takes priority without master autoattack");
 master.selection=ObjectGuid();ai.storage.units["current target"].value=&primary;
 check(ai.GetPlayerMasterAttackTarget()==&primary,"defense retains current engaged target");
 ai.ranged=true;primary.fleeing=true;master.selection=add.guid;master.victim=&add;
 check(ai.GetPlayerMasterAttackTarget()==&primary,"fleeing current target survives leader switch");
 check(ai.IsPlayerMasterAttackAllowed(&primary)&&!ai.IsPlayerMasterAttackAllowed(&add),"attack guard shares fleeing lock");
 bot.victim=&primary;ai.UpdatePlayerMasterAssist();check(bot.victim==&primary,"update does not stop fleeing target attack");
 primary.cc=true;check(!ai.GetPlayerMasterAttackTarget(),"CC pauses fleeing pursuit without switching");primary.cc=false;
 primary.los=false;check(!ai.GetPlayerMasterAttackTarget(),"LOS blocks fleeing pursuit");
 ai.UpdatePlayerMasterAssist();check(ai.storage.units["current target"].value==&primary&&!ai.GetPlayerMasterAttackTarget(),"paused runner survives successive updates without switching");primary.los=true;
 primary.near=false;check(!ai.GetPlayerMasterAttackTarget(),"range bounds fleeing pursuit");primary.near=true;
 primary.fleeing=false;check(ai.GetPlayerMasterAttackTarget()==&add,"runner stops fleeing: selected engaged target resumes");
 primary.fleeing=true;primary.alive=false;check(ai.GetPlayerMasterAttackTarget()==&add,"dead runner releases lock");primary.alive=true;
 primary.fleeing=false;master.victim=nullptr;master.selection=ObjectGuid();ai.storage.units["current target"].value=nullptr;
 add.cc=true;primary.cc=true;check(!ai.GetPlayerMasterAttackTarget(),"defense excludes crowd-controlled targets");add.cc=primary.cc=false;
 add.attackable=false;primary.attackable=false;check(!ai.GetPlayerMasterAttackTarget(),"defense excludes native unattackable targets");add.attackable=primary.attackable=true;
 ai.storage.attackers.value.clear();check(!ai.GetPlayerMasterAttackTarget(),"no group attackers: no autonomous pull");
 // Melee never pursues/attacks a runner; ranged finishers cannot chase it either.
 master.selection=primary.guid;master.victim=&primary;primary.fleeing=true;ai.ranged=false;
 ai.storage.units["current target"].value=&primary;ai.storage.attackers.value={primary.guid,add.guid};
 check(!ai.CanPlayerMasterChase(&primary)&&ai.CanPlayerMasterChase(&add),"runner movement blocked; other targets unaffected");
 check(!ai.IsPlayerMasterAttackAllowed(&primary)&&!ai.IsPlayerMasterSpellAllowed(&damage,&primary),"melee runner damage denied");
 check(ai.GetPlayerMasterAttackTarget()==&add,"melee defends another attacker instead of locking runner");
 ai.playerMasterPursuitTarget=primary.guid;bot.victim=&primary;bot.chase=&primary;int stopped=ai.movementStops;
 pet.victim=&primary;int followed=pet.motion.followed;
 ai.UpdatePlayerMasterAssist();check(ai.movementStops==stopped+1&&!bot.chase&&pet.motion.followed>followed,"active native/path pursuit and pet attack cancelled");
 check(ai.playerMasterPursuitTarget==ObjectGuid(),"StopMoving clears pursuit provenance");
 ai.ranged=true;ai.storage.units["current target"].value=&primary;
 check(ai.GetPlayerMasterAttackTarget()==&primary&&ai.IsPlayerMasterAttackAllowed(&primary)&&!ai.CanPlayerMasterChase(&primary),"ranged keeps attack target without movement permission");
 ai.playerMasterPursuitTarget=primary.guid;bot.chase=nullptr;stopped=ai.movementStops;
 ai.UpdatePlayerMasterAssist();check(ai.movementStops==stopped+1,"hazard-routed pursuit cancelled without native chase reference");
 master.alive=false;ai.ranged=false;check(!ai.IsPlayerMasterAttackAllowed(&primary)&&!ai.CanPlayerMasterChase(&primary),"dead leader does not reopen melee pursuit");master.alive=true;
 primary.friendly=true;check(ai.CanPlayerMasterChase(&primary)&&ai.IsPlayerMasterSpellAllowed(&heal,&primary),"friendly healing movement remains allowed");primary.friendly=false;
 ai.activeMaster=false;check(ai.CanPlayerMasterChase(&primary),"free bot pursuit unchanged");ai.activeMaster=true;
 ai.pvp=true;check(ai.CanPlayerMasterChase(&primary),"PvP pursuit unchanged");ai.pvp=false;
 primary.fleeing=false;check(ai.CanPlayerMasterChase(&primary),"pursuit allowed after fleeing ends");
 // Healer damage reserve is independent of leader focus and leaves wand/melee intact.
 ai.ranged=true;bot.healer=true;master.alive=false;
 SpellEntry nuke,dot,mixed,wand,utility;nuke.Effect[0]=2;dot.Effect[0]=6;dot.EffectApplyAuraName[0]=3;mixed.Effect[0]=2;mixed.positive=true;wand.Effect[0]=2;wand.wand=true;
 for(float mana:{0.0f,49.9f,50.0f}){bot.manaPercent=mana;
 check(!ai.IsPlayerMasterSpellAllowed(&nuke,&primary)&&!ai.IsPlayerMasterSpellAllowed(&dot,&primary),"healer direct damage and DoTs blocked at or below 50 percent");
 check(!ai.IsPlayerMasterSpellAllowed(&mixed,&bot),"mixed area heal/damage cannot bypass cutoff");
 check(ai.IsPlayerMasterSpellAllowed(&wand,&primary)&&ai.IsPlayerMasterSpellAllowed(&heal,&bot)&&ai.IsPlayerMasterSpellAllowed(&utility,&primary),"wand, healing and utility allowed");
 check(ai.IsPlayerMasterAttackAllowed(&primary),"normal melee attack permission preserved");}
 bot.manaPercent=50.1f;check(ai.IsPlayerMasterSpellAllowed(&nuke,&primary),"damage resumes strictly above 50 percent");
 bot.manaPercent=10;bot.healer=false;check(ai.IsPlayerMasterSpellAllowed(&nuke,&primary),"non-healer rotation unchanged");bot.healer=true;
 ai.activeMaster=false;check(ai.IsPlayerMasterSpellAllowed(&nuke,&primary),"free bot reserve unchanged");ai.activeMaster=true;
 ai.real=true;check(ai.IsPlayerMasterSpellAllowed(&nuke,&primary),"real player unchanged");ai.real=false;
 bot.hasMana=false;check(ai.IsPlayerMasterSpellAllowed(&nuke,&primary),"non-mana form unchanged");bot.hasMana=true;
 ai.pvp=true;check(!ai.IsPlayerMasterSpellAllowed(&nuke,&primary),"human-owned healer reserve also applies outside PvE focus");ai.pvp=false;
 master.alive=true;check(!ai.IsPlayerMasterSpellAllowed(&nuke,&primary),"living leader reserve");

 bot.healer=false;bot.manaPercent=100;master.alive=true;
 ai.pvp=true;ai.UpdatePlayerMasterAssist();check(pet.brain.react==REACT_DEFENSIVE,"pet react restored outside policy");
 // The backup melee Paladin peels engaged melee from a healer, then returns focus.
 ai.pvp=false;ai.real=false;ai.ranged=false;ai.offheal=true;bot.cls=CLASS_PALADIN;bot.healer=false;
 master.victim=&primary;master.selection=primary.guid;primary.alive=true;primary.safe=true;primary.near=true;primary.fleeing=false;
 Player priest;priest.healer=true;add.victim=&priest;add.meleeReach=true;
 ai.storage.attackers.value={primary.guid,add.guid};ai.storage.units["current target"].value=&primary;
 check(ai.GetPlayerMasterAttackTarget()==&add,"peel engaged melee from priest despite master focus");
 add.victim=&bot;ai.storage.units["current target"].value=&add;
 check(ai.GetPlayerMasterAttackTarget()==&add,"hold peel until tank reacquires");
 add.victim=&master;check(ai.GetPlayerMasterAttackTarget()==&primary,"return focus after tank reacquires");
 add.victim=&priest;add.meleeReach=false;check(ai.GetPlayerMasterAttackTarget()==&primary,"never chase distant shooter for peel");
 add.meleeReach=true;ai.storage.attackers.value={primary.guid};check(ai.GetPlayerMasterAttackTarget()==&primary,"no unengaged peel pull");
 ai.storage.attackers.value={primary.guid,add.guid};ai.offheal=false;check(ai.GetPlayerMasterAttackTarget()==&primary,"other roles retain focus");
 std::cout<<"Native master-focus policy, target transitions, spell guards, pet lifecycle and warning cases passed\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;} }
