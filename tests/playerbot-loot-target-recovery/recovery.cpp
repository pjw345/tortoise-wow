#include <algorithm>
#include <cmath>
#include <iostream>
#include <list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
using uint8=unsigned char;
constexpr float INTERACTION_DISTANCE=5;
enum {CORPSE=1,CMSG_LOOT_RELEASE=2};
enum class SizeFactor {None};
struct ObjectGuid {int id=0;bool operator==(ObjectGuid b)const{return id==b.id;}bool operator!=(ObjectGuid b)const{return id!=b.id;}unsigned long GetRawValue()const{return id;}};
struct Player;
struct Creature {float botDistance=20,masterDistance=24,z=0;int state=CORPSE;bool IsWithinDistInMap(Player*,float range,bool,SizeFactor){return std::hypot(botDistance,z)<range;}};
struct WorldPacket {ObjectGuid released;WorldPacket(int,int){}WorldPacket&operator<<(ObjectGuid g){released=g;return *this;}};
struct Session {int releases=0;ObjectGuid released;void HandleLootReleaseOpcode(WorldPacket&p){++releases;released=p.released;}};
struct Player {bool grouped=true;Session session;void*GetGroup(){return grouped?this:nullptr;}float GetMaxLootDistance(Creature*){return 5;}Session*GetSession(){return &session;}const char*GetName(){return "fixture";}};
struct LootObject {ObjectGuid guid;bool eligible=false,safe=true;bool IsEmpty()const{return !guid.id;}bool IsLootPossible(Player*){return eligible&&!IsEmpty();}bool IsApproachSafe(Player*){return safe;}};
struct LootObjectStack {std::vector<LootObject> candidates;std::vector<LootObject> OrderByDistance(float){return candidates;}};
struct Context;
struct PlayerbotAI {
 Player*master=nullptr;bool leader=false,active=true;Creature*corpse=nullptr;ObjectGuid corpseGuid{1};
 Player*GetMaster(){return master;}Creature*GetCreature(ObjectGuid g){return corpse&&g==corpseGuid?corpse:nullptr;}
 bool IsGroupLeader(){return leader;}bool HasActivePlayerMaster(){return active;}bool HasStrategy(const char*,int){return false;}
};
struct Config {float lootDistance=90,groupMemberLootDistanceWithActiveMaster=25,groupMemberLootDistance=10,followDistance=1;}sPlayerbotAIConfig;
struct Facade {Player*master=nullptr;int GetDeathState(Creature*c){return c->state;}float GetDistance2d(Player*p,Creature*c){return p==master?c->masterDistance:c->botDistance;}bool IsDistanceLessOrEqualThan(float a,float b){return a<=b;}}sServerFacade;
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
 Player bot,master;Creature corpse;PlayerbotAI ai;Context context;LootAvailableTrigger available;FarFromCurrentLootTrigger far;LootAction select;
 Fixture():available{{&bot,&ai,&context}},far{{&bot,&ai,&context}},select{{&bot,&ai,&context}} {
  values=Values{};sPlayerbotAIConfig=Config{};ai.master=&master;ai.corpse=&corpse;activeAI=&ai;sServerFacade.master=&master;
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
 values.current.eligible=false;values.combat=true;check(!f.available.IsActive(),"combat still prevents reselection");values.combat=false;
 values.mounted=true;check(!f.available.IsActive(),"mount still prevents reselection");values.mounted=false;
 values.available=false;check(!f.available.IsActive(),"no available loot stays inactive");values.available=true;
 values.current={{1},true,true};f.corpse.masterDistance=26;values.stack.candidates={{{1},true,true},{{2},true,false}};
 check(f.available.IsActive()&&!f.select.Execute(event)&&values.current.guid.id==1,"native candidate safety cannot be bypassed by recovery trigger");
 values.stack.candidates={};check(!f.select.Execute(event),"no candidate does not force a target");
 values.stack.candidates={{{1},true,true},{{2},true,true}};f.corpse.masterDistance=10;
 check(f.select.Execute(event)&&values.current.guid.id==1,"native corpse-first order remains intact");
}
int main(int argc,char**argv){try{std::string mode=argc>1?argv[1]:"";if(mode=="range")ranges();else recovery();std::cout<<"PASS "<<mode<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
