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
constexpr int SPELL_EFFECT_INTERRUPT_CAST=68, UNIT_STAT_FLEEING=1;
constexpr float PET_FOLLOW_DIST=2, PET_FOLLOW_ANGLE=1.57f;
struct SpellEntry { uint32 Id=1; bool positive=false,area=false,interrupt=false; bool IsAreaOfEffectSpell()const{return area;} };
bool IsPositiveSpell(SpellEntry const* s){return s->positive;}
bool IsSpellHaveEffect(SpellEntry const* s,int){return s->interrupt;}
struct Spell { SpellEntry* m_spellInfo=nullptr; bool canInterrupt=true; struct { ObjectGuid guid; ObjectGuid getUnitTargetGuid(){return guid;} }m_targets; bool CanBeInterrupted(){return canInterrupt;} };
struct Motion { int followed=0; template<class T> void MoveFollow(T*,float,float){++followed;} };
struct CreatureAI { ReactStates react=REACT_DEFENSIVE; ReactStates GetReactState(){return react;} void SetReactState(ReactStates r){react=r;} };
struct Pet;
struct Unit { ObjectGuid guid; bool alive=true,friendly=false,safe=true,world=true,near=true,los=true,cc=false,attackable=true,fleeing=false; void* map=reinterpret_cast<void*>(1); Unit* victim=nullptr; std::string name="Add"; int stops=0; bool HasUnitState(int){return fleeing;} bool IsWithinLOSInMap(Unit*u){return u->los;} Unit* GetVictim(){return victim;} void AttackStop(){victim=nullptr;++stops;} void* GetMap(){return map;} bool IsInWorld(){return world;} bool IsWithinDistInMap(Unit*,float,bool){return near;} ObjectGuid GetObjectGuid(){return guid;} char const* GetName(){return name.c_str();} };
struct Player : Unit { ObjectGuid selection; ObjectGuid GetSelectionGuid(){return selection;} bool grouped=true; Pet* pet=nullptr; std::set<Unit*> attackers; std::map<int,Spell*> spells; bool IsInGroup(Player*){return grouped;} Pet* GetPet(){return pet;} std::set<Unit*>& getAttackers(){return attackers;} Spell* GetCurrentSpell(CurrentSpellTypes t){return spells[t];} void InterruptSpell(CurrentSpellTypes t){spells[t]=nullptr;} };
struct Pet:Unit { CreatureAI brain; Motion motion; CreatureAI* AI(){return &brain;} Motion* GetMotionMaster(){return &motion;} };
struct PossibleAttackTargetsValue {static bool IsPossibleTarget(Unit*u,Player*,float,bool){return u->near&&u->attackable&&!u->cc;}};
struct Facade { bool IsAlive(Unit* u){return u->alive;} bool IsFriendlyTo(Unit*,Unit* u){return u->friendly;} }sServerFacade;
struct Config { bool assistPlayerMaster=true; float sightDistance=60; }sPlayerbotAIConfig;
struct WorldTimer { static uint32 now; static uint32 getMSTime(){return now;} static uint32 getMSTimeDiff(uint32 a,uint32 b){return b-a;} }; uint32 WorldTimer::now=100;
enum class PlayerbotSecurityLevel { PLAYERBOT_SECURITY_ALLOW_ALL };
template<class T> struct Value { T value{}; int resets=0; T Get(){return value;} void Set(T v){value=v;} void Reset(){++resets;} };
struct Context { std::map<std::string,Value<Unit*>> units; Value<ObjectGuid> attack; Value<std::list<ObjectGuid>> attackers; template<class T> Value<T>* GetValue(std::string n); };
template<> Value<Unit*>* Context::GetValue<Unit*>(std::string n){return &units[n];}
template<> Value<ObjectGuid>* Context::GetValue<ObjectGuid>(std::string){return &attack;}
template<> Value<std::list<ObjectGuid>>* Context::GetValue<std::list<ObjectGuid>>(std::string){return &attackers;}
struct PlayerbotAI { Player *bot=nullptr,*master=nullptr; Context storage; Context* aiObjectContext=&storage; bool activeMaster=true,pvp=false,real=false,silent=false; bool playerMasterAggroObserved=false; uint32 playerMasterAggroStartedMs=0; uint32 lastPlayerMasterAggroWarningMs=0; ObjectGuid playerMasterFocusPet; uint8 playerMasterFocusPetReact=0; std::map<int,Unit*> world; std::vector<std::string> warnings; int interrupted=0;
 bool HasActivePlayerMaster(){return master && activeMaster;} bool IsRealPlayer(){return real;} bool IsInPvp(){return pvp;} bool IsSafe(Player* p){return p&&p->safe;} static bool IsSafe(Player*,Unit* p){return p&&p->safe;}
 Unit* GetUnit(ObjectGuid g){return world[g.value];} void SpellInterrupted(uint32){++interrupted;}
 bool TellPlayerNoFacing(Player*,std::string text,PlayerbotSecurityLevel,bool,bool){if(silent)return false;warnings.push_back(text);return true;}
 bool ShouldAssistPlayerMaster(); Unit* GetPlayerMasterAttackTarget(); bool IsPlayerMasterAttackAllowed(Unit*); bool IsPlayerMasterSpellAllowed(SpellEntry const*,Unit*); void UpdatePlayerMasterAssist();
};
#include "NativeMasterFocus.inc"
void check(bool b,char const* label){if(!b)throw std::runtime_error(label);}
int main(){try{
 Player bot,master; Unit primary,add; primary.guid=ObjectGuid(1); add.guid=ObjectGuid(2); PlayerbotAI ai; ai.bot=&bot;ai.master=&master;ai.world[1]=&primary;ai.world[2]=&add;
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
 primary.fleeing=true;master.selection=add.guid;master.victim=&add;
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
 ai.pvp=true;ai.UpdatePlayerMasterAssist();check(pet.brain.react==REACT_DEFENSIVE,"pet react restored outside policy");
 std::cout<<"Native master-focus policy, target transitions, spell guards, pet lifecycle and warning cases passed\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;} }
