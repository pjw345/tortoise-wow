#include <algorithm>
#include <string>
#include <stdexcept>
#include <iostream>
#include <ctime>
#include <cmath>
using uint32=unsigned;enum{SKILL_NONE,SKILL_MINING,SKILL_HERBALISM};constexpr float INTERACTION_DISTANCE=5;
struct Player{const char*GetName(){return "test";}bool grouped=true;void*GetGroup(){return grouped?this:nullptr;}};
struct Event{Player*owner=nullptr;Player*getOwner(){return owner;}};
struct ObjectGuid{int id=0;bool operator!=(ObjectGuid o)const{return id!=o.id;}bool IsCreature(){return !IsGameObject();}bool IsGameObject(){return id==10;}unsigned long GetRawValue(){return id;}};
struct WorldObject{}object;struct WorldPosition{explicit WorldPosition(WorldObject*){}};
struct LootObject{ObjectGuid guid;uint32 skillId=SKILL_NONE;bool safe=true,possible=true,exists=true;bool IsEmpty(){return guid.id==0;}bool IsLootPossible(Player*,bool*s){*s=false;return possible;}WorldObject*GetWorldObject(Player*){return exists?&object:nullptr;}bool IsApproachSafe(Player*){return safe;}}current;
struct Stack{int defers=0,removes=0,ignores=0;void Defer(ObjectGuid,uint32){++defers;}void Remove(ObjectGuid){++removes;}void Ignore(ObjectGuid,uint32,bool=false,bool=false){++ignores;}}stack;
struct AI{bool leader=false,moving=true;int stops=0;bool HasStrategy(const char*,int){return false;}bool IsGroupLeader(){return leader;}void StopMoving(){moving=false;++stops;}};
struct BotState{static constexpr int BOT_STATE_NON_COMBAT=0;};struct Log{template<class...T>void outLoot(T...){}}sLog;
struct Config{float contactDistance=0.5f,lootHostileDistance=15;uint32 lootTargetInspectDelay=120,lootTargetRetryDelay=5,globalCoolDown=1500;}sPlayerbotAIConfig;
struct Facade{float distance=30;bool los=true;float GetDistance2d(Player*,WorldObject*){return distance;}bool IsWithinLOSInMap(Player*,WorldObject*){return los;}}sServerFacade;
template<class T>T value(const char*);template<>LootObject value<LootObject>(const char*){return current;}template<>Stack*value<Stack*>(const char*){return &stack;}
using LootObjectStack=Stack;
#define AI_VALUE(type,name) value<type>(name)
#define RESET_AI_VALUE(type,name) current=LootObject()
struct Value{void Set(LootObject obj){current=obj;}}slot;struct Context{template<class T>Value*GetValue(const char*){return &slot;}}contextStorage;
struct MoveToLootAction{Player*bot;AI*ai;ObjectGuid trackedLootGuid;float bestLootDistance=0;time_t lootMoveStarted=0;uint32 duration=0;int moves=0;bool moveResult=true;
 void WaitForReach(float){duration=5000;}void SetDuration(uint32 x){duration=x;}uint32 GetDuration(){return duration;}
 bool MoveNear(WorldObject*,float){++moves;ai->moving=true;return moveResult;}bool MoveTo(WorldPosition){++moves;ai->moving=true;return moveResult;}bool Execute(Event&);};
struct OpenLootAction{Player*bot;AI*ai;Context*context=&contextStorage;int opens=0;bool DoLoot(LootObject&){++opens;return true;}bool Execute(Event&);};
#include "native.inc"
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{std::string mode=argv[1];Player bot;AI ai;Event event;current.guid.id=10;MoveToLootAction move{&bot,&ai};OpenLootAction open{&bot,&ai};
 if(mode=="new_threat"){current.safe=false;check(!move.Execute(event)&&ai.stops==1&&!ai.moving&&!move.moves&&stack.defers==1&&current.IsEmpty(),"unsafe selected loot must stop existing movement and defer");}
 else if(mode=="recovery"){current.safe=false;move.Execute(event);current=LootObject();current.guid.id=10;check(move.Execute(event)&&move.moves==1&&ai.moving,"cleared threat permits native approach again");}
 else if(mode=="recheck"){check(move.Execute(event)&&move.duration==sPlayerbotAIConfig.globalCoolDown,"protected approach wait bounded by existing GCD");current.safe=false;check(!move.Execute(event)&&ai.stops==1&&!ai.moving&&move.moves==1,"next action stops before relaunch");}
 else if(mode=="open_threat"){current.safe=false;check(!open.Execute(event)&&!open.opens&&ai.stops==1&&!ai.moving&&stack.defers==1&&current.IsEmpty(),"automatic in-range opening rechecks safety before interaction");}
 else if(mode=="manual"){event.owner=&bot;current.safe=false;check(open.Execute(event)&&open.opens==1&&!ai.stops,"explicit owner use semantics retained");}
 else if(mode=="open_safe"){check(open.Execute(event)&&open.opens==1&&stack.removes==1&&current.IsEmpty()&&!ai.stops,"safe ordinary container keeps normal completion");}
 else if(mode=="legacy"){for(int policy=0;policy<3;++policy){bot.grouped=policy!=0;ai.leader=policy==1;sPlayerbotAIConfig.lootHostileDistance=policy==2?0:15;check(move.Execute(event)&&move.duration==5000&&!ai.stops,"solo/leader/opt-out wait unchanged");}}
 else if(mode=="short_wait"){sPlayerbotAIConfig.globalCoolDown=6000;check(move.Execute(event)&&move.duration==5000,"never lengthen existing movement wait");}
 else if(mode=="no_move"){move.moveResult=false;move.duration=321;check(!move.Execute(event)&&move.duration==321,"failed move does not invent a wait");}
 std::cout<<"PASS "<<mode<<'\n';return 0;}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
