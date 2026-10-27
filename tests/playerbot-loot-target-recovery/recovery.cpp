#include <algorithm>
#include <cmath>
#include <iostream>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
using uint8=unsigned char;
constexpr float INTERACTION_DISTANCE=5;
enum {CORPSE=1,CMSG_LOOT_RELEASE=2};
enum class SizeFactor {None};
struct ObjectGuid {int id=0;bool operator==(ObjectGuid b)const{return id==b.id;}bool operator!=(ObjectGuid b)const{return id!=b.id;}unsigned long GetRawValue()const{return id;}};
struct Map {}; Map worldMap, otherMap;
struct HostileRefs {bool present=false;bool isEmpty(){return !present;}};
struct Unit {
 bool alive=true,inWorld=true,combat=false;Map*map=&worldMap;Unit*victim=nullptr;HostileRefs refs;std::vector<Unit*>attackers;
 bool IsInWorld(){return inWorld;}Map*GetMap(){return map;}Unit*GetVictim(){return victim;}
 HostileRefs&GetHostileRefManager(){return refs;}std::vector<Unit*>&getAttackers(){return attackers;}
};
struct Pet:Unit {};
struct Group {struct Slot{ObjectGuid guid;};std::vector<Slot>slots;auto const&GetMemberSlots(){return slots;}};
struct Player;
struct Creature:Unit {float botDistance=20,masterDistance=24,z=0;int state=CORPSE;bool IsWithinDistInMap(Player*,float range,bool,SizeFactor){return std::hypot(botDistance,z)<range;}};
struct WorldPacket {ObjectGuid released;WorldPacket(int,int){}WorldPacket&operator<<(ObjectGuid g){released=g;return *this;}};
struct Session {int releases=0;ObjectGuid released;void HandleLootReleaseOpcode(WorldPacket&p){++releases;released=p.released;}};
struct Player:Unit {bool grouped=true;Group*group=nullptr;Pet*pet=nullptr;float position=0;Session session;
 Group*GetGroup(){return grouped?group:nullptr;}Pet*GetPet(){return pet;}
 float GetMaxLootDistance(Creature*){return 5;}Session*GetSession(){return &session;}const char*GetName(){return "fixture";}};
struct ObjectManager {std::map<int,Player*>players;Player*GetPlayer(ObjectGuid guid){auto i=players.find(guid.id);return i==players.end()?nullptr:i->second;}}sObjectMgr;
struct LootObject {ObjectGuid guid;bool eligible=false,safe=true;bool IsEmpty()const{return !guid.id;}bool IsLootPossible(Player*){return eligible&&!IsEmpty();}bool IsApproachSafe(Player*){return safe;}};
struct LootObjectStack {std::vector<LootObject> candidates;std::vector<LootObject> OrderByDistance(float){return candidates;}};
struct Context;
struct PlayerbotAI {
 Player*master=nullptr;bool leader=false,active=true,real=false,pvp=false;Creature*corpse=nullptr;ObjectGuid corpseGuid{1};
 Player*GetMaster(){return master;}Creature*GetCreature(ObjectGuid g){return corpse&&g==corpseGuid?corpse:nullptr;}
 bool IsRealPlayer(){return real;}bool IsInPvp(){return pvp;}bool IsGroupLeader(){return leader;}bool HasActivePlayerMaster(){return active;}bool HasStrategy(const char*,int){return false;}
};
struct Config {float lootDistance=90,groupMemberLootDistanceWithActiveMaster=25,groupMemberLootDistance=10,followDistance=1,reactDistance=60;}sPlayerbotAIConfig;
struct Facade {Player*master=nullptr;int GetDeathState(Creature*c){return c->state;}float GetDistance2d(Player*p,Creature*c){return p==master?c->masterDistance:c->botDistance;}bool IsDistanceLessOrEqualThan(float a,float b){return a<=b;}
 bool IsAlive(Unit*u){return u->alive;}bool IsInCombat(Unit*u){return u->combat;}
 float GetDistance2d(Player*a,Player*b){return std::abs(a->position-b->position);}}sServerFacade;
struct BotState {static constexpr int BOT_STATE_NON_COMBAT=0;};
struct Log {template<class...T>void outLoot(T...){}}sLog;
struct Values {
 bool available=true,combat=false,mounted=false,canLoot=false;float distance=20;LootObject current;LootObjectStack stack;std::list<ObjectGuid> targets{{99}};
 template<class T>T get(const char*name) {
  std::string n(name);
  if constexpr(std::is_same_v<T,bool>) {if(n=="has available loot")return available;if(n=="combat")return combat;if(n=="mounted")return mounted;return canLoot;}
  else if constexpr(std::is_same_v<T,float>)return distance;
  else if constexpr(std::is_same_v<T,LootObject>)return current;
  else if constexpr(std::is_same_v<T,LootObjectStack*>)return &stack;
  else if constexpr(std::is_same_v<T,std::list<ObjectGuid>>)return targets;
 }
}values;
struct LootValue {void Set(LootObject obj){values.current=obj;}};
struct Context {LootValue value;template<class T>LootValue*GetValue(const char*){return &value;}};
#define AI_VALUE(type,name) values.get<type>(name)
#define AI_VALUE2(type,name,param) values.get<type>(name)
PlayerbotAI* GetBotAI(Player*);
struct ActionBase {Player*bot;PlayerbotAI*ai;Context*context;};
struct LootAvailableTrigger:ActionBase {bool IsActive();};
struct FarFromCurrentLootTrigger:ActionBase {bool IsActive();};
struct CanLootTrigger:ActionBase {bool IsActive();};
struct Event {};
struct LootAction:ActionBase {bool Execute(Event&);};
#include "native-triggers.inc"
#include "native-selector.inc"
PlayerbotAI* activeAI=nullptr;
PlayerbotAI* GetBotAI(Player*){return activeAI;}
void check(bool value,const char*msg){if(!value)throw std::runtime_error(msg);}
struct Fixture {
 Group group,other;Unit enemy;Pet pet;Player bot,master,member;Creature corpse;PlayerbotAI ai;Context context;LootAvailableTrigger available;FarFromCurrentLootTrigger far;LootAction select;
 Fixture():available{{&bot,&ai,&context}},far{{&bot,&ai,&context}},select{{&bot,&ai,&context}} {
  values=Values{};sPlayerbotAIConfig=Config{};bot.group=master.group=member.group=&group;
  group.slots={{{1}},{{2}},{{3}}};sObjectMgr.players={{1,&master},{2,&member},{3,&bot}};member.position=4;
  ai.master=&master;ai.corpse=&corpse;activeAI=&ai;sServerFacade.master=&master;
  values.current={{1},true,true};values.stack.candidates={values.current,{{2},true,true}};
 }
};
void ranges(){
 Fixture f;
 check(f.far.IsActive(),"selected corpse inside configured 25-yard range must be approachable beyond follow offset");
 f.corpse.masterDistance=25;check(f.far.IsActive(),"inclusive configured boundary");
 f.corpse.masterDistance=25.01f;check(!f.far.IsActive(),"outside active-master corpse range stays blocked");
 f.ai.active=false;f.corpse.masterDistance=11;check(!f.far.IsActive(),"inactive master uses configured inactive range");
 f.corpse.masterDistance=10;check(f.far.IsActive(),"inactive-master boundary");
 f.bot.grouped=false;f.corpse.masterDistance=80;check(f.far.IsActive(),"ungrouped uses global selection range");
 f.bot.grouped=true;f.ai.leader=true;check(f.far.IsActive(),"leader uses global range");
 f.corpse.masterDistance=91;check(!f.far.IsActive(),"global range remains bounded");
 f.ai.master=nullptr;check(f.far.IsActive(),"no master retains solo movement");
 f.ai.master=&f.bot;check(f.far.IsActive(),"self-master retains solo movement");
 f.ai.master=&f.master;f.ai.leader=false;f.ai.active=true;f.corpse.masterDistance=20;f.corpse.botDistance=4;f.corpse.z=4;values.distance=4;
 check(f.far.IsActive(),"native 3D corpse reach preserved despite close 2D distance");
 f.corpse.z=0;check(!f.far.IsActive(),"corpse inside native reach does not move");
 values.current.eligible=false;check(!f.far.IsActive(),"ineligible selected loot never moves");
 values.current={{2},true,true};values.distance=6;check(f.far.IsActive(),"GO interaction distance unchanged");values.distance=4;check(!f.far.IsActive(),"GO already in reach unchanged");
}
void recovery(){
 Fixture f;Event event;
 f.corpse.masterDistance=26;
 check(f.available.IsActive(),"outside-range selected corpse must permit queued herb reselection with nearby targets");
 check(f.select.Execute(event)&&values.current.guid.id==2,"native selector skips out-of-range corpse and chooses herb");
 check(f.bot.session.releases==1&&f.bot.session.released.id==1,"native release preserves target transition");
 values.current={{1},false,true};f.corpse.masterDistance=10;
 check(f.available.IsActive(),"invalid selected loot permits reselection");
 values.stack.candidates={{{2},true,true}};check(f.select.Execute(event)&&values.current.guid.id==2,"valid queued herb selected after invalid old target");
 values.current={{1},true,true};values.distance=20;f.corpse.masterDistance=10;
 check(!f.available.IsActive(),"existing valid far target and nearby-enemy admission retained");
 values.targets.clear();check(f.available.IsActive(),"ordinary no-enemy selection preserved");
 values.targets={{99}};values.distance=4;check(f.available.IsActive(),"near-current-target selection preserved");
 values.current.eligible=false;values.combat=true;f.bot.combat=true;check(!f.available.IsActive(),"own combat still prevents reselection");values.combat=false;f.bot.combat=false;
 values.mounted=true;check(!f.available.IsActive(),"mount still prevents reselection");values.mounted=false;
 values.available=false;check(!f.available.IsActive(),"no available loot stays inactive");values.available=true;
 values.current={{1},true,true};f.corpse.masterDistance=26;values.stack.candidates={{{1},true,true},{{2},true,false}};
 check(f.available.IsActive()&&!f.select.Execute(event)&&values.current.guid.id==1,"native candidate safety cannot be bypassed by recovery trigger");
 values.stack.candidates={};check(!f.select.Execute(event),"no candidate does not force a target");
 values.stack.candidates={{{1},true,true},{{2},true,true}};f.corpse.masterDistance=10;
 check(f.select.Execute(event)&&values.current.guid.id==1,"native corpse-first order remains intact");
}
void inherited(){
 Fixture f;values.current.eligible=false;values.combat=true;f.master.combat=true;
#ifdef MANGOSBOT_ZERO
 check(f.available.IsActive(),"a master flag without combat links must not delay loot");
 Event event;values.stack.candidates={{{2},true,true}};
 check(f.select.Execute(event)&&values.current.guid.id==2,"native queued gathering selection works during inherited flag");
 values.current.eligible=false;values.stack.candidates={{{1},true,true}};
 check(f.available.IsActive()&&f.select.Execute(event)&&values.current.guid.id==1,"ordinary corpse selection works during inherited flag");
#else
 check(!f.available.IsActive(),"other variants keep inherited combat gate");
#endif
}
void actualCombat(){
 Fixture f;values.current.eligible=false;values.combat=true;f.master.combat=true;f.bot.combat=true;
 check(!f.available.IsActive(),"own native combat still blocks");f.bot.combat=false;
 f.master.refs.present=true;check(!f.available.IsActive(),"native hostile references keep party combat active");f.master.refs.present=false;
 f.master.victim=&f.enemy;check(!f.available.IsActive(),"living current victim keeps party combat active");f.enemy.alive=false;
 check(f.available.IsActive(),"dead former victim alone cannot hold loot");f.master.victim=nullptr;f.enemy.alive=true;
 f.master.attackers={&f.enemy};check(!f.available.IsActive(),"living attacker including ranged or controlled enemy keeps combat active");
 f.enemy.alive=false;check(f.available.IsActive(),"dead attacker entries alone do not hold selection");
}
void groupCombat(){
 Fixture f;values.current.eligible=false;values.combat=true;f.member.combat=true;f.member.refs.present=true;
 check(!f.available.IsActive(),"another nearby companion's real fight blocks");f.member.inWorld=false;
 check(f.available.IsActive(),"offline member cannot hold local loot");f.member.inWorld=true;f.member.map=&otherMap;
 check(f.available.IsActive(),"different-map combat is outside local party scope");f.member.map=&worldMap;f.member.position=61;
 check(f.available.IsActive(),"distant member retains native react-range boundary");f.member.position=60;
 check(!f.available.IsActive(),"inclusive native nearby boundary still blocks a real fight");
}
void petCombat(){
 Fixture f;values.current.eligible=false;values.combat=true;f.master.combat=true;f.master.pet=&f.pet;f.pet.refs.present=true;
 check(!f.available.IsActive(),"pet threat references keep group combat active");f.pet.refs.present=false;f.pet.victim=&f.enemy;
 check(!f.available.IsActive(),"pet living victim keeps group combat active");f.enemy.alive=false;
 check(f.available.IsActive(),"ended pet combat releases inherited hold");f.enemy.alive=true;f.pet.inWorld=false;
 check(f.available.IsActive(),"absent pet does not hold loot");f.pet.inWorld=true;f.pet.map=&otherMap;
 check(f.available.IsActive(),"different-map pet does not hold local loot");
}
void scope(){for(int n=0;n<8;++n){Fixture f;values.current.eligible=false;values.combat=true;f.master.combat=true;
 switch(n){case 0:f.ai.active=false;break;case 1:f.ai.real=true;break;case 2:f.ai.pvp=true;break;case 3:f.ai.master=nullptr;break;case 4:f.master.inWorld=false;break;case 5:f.master.map=&otherMap;break;case 6:f.bot.grouped=false;break;case 7:f.master.group=&f.other;break;}
 check(!f.available.IsActive(),"non-player-led, PvP or mismatched master context retains old gate");}}
void conditions(){Fixture f;Event event;values.current.eligible=false;values.combat=true;f.master.combat=true;
 values.mounted=true;check(!f.available.IsActive(),"mounted condition retained");values.mounted=false;values.available=false;
 check(!f.available.IsActive(),"availability condition retained");values.available=true;
 values.stack.candidates={{{2},true,false}};check(f.available.IsActive()&&!f.select.Execute(event),"native candidate safety remains authoritative");
 values.stack.candidates={};check(!f.select.Execute(event),"no candidate is invented");}
void transition(){Fixture f;values.current.eligible=false;values.combat=true;f.master.combat=true;f.master.refs.present=true;
 check(!f.available.IsActive(),"actual fight held initially");f.master.refs.present=false;
 check(f.available.IsActive(),"native link removal releases loot without changing lingering flag or global combat value");
 check(values.combat&&f.master.combat,"loot check cannot mutate native or cached combat state");
 f.master.attackers={nullptr};check(f.available.IsActive(),"null stale attacker is ignored safely");
 f.master.attackers={&f.enemy};f.enemy.inWorld=false;check(f.available.IsActive(),"out-of-world attacker cannot hold local selection");}
int main(int argc,char**argv){try{std::string mode=argc>1?argv[1]:"";if(mode=="range")ranges();else if(mode=="inherited")inherited();else if(mode=="actual_combat")actualCombat();else if(mode=="group_combat")groupCombat();else if(mode=="pet_combat")petCombat();else if(mode=="scope")scope();else if(mode=="conditions")conditions();else if(mode=="transition")transition();else recovery();std::cout<<"PASS "<<mode<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
