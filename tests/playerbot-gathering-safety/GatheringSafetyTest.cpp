#include <algorithm>
#include <cstdint>
#include <iostream>
#include <list>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using uint8=uint8_t;using uint32=uint32_t;
enum class SizeFactor{None,BoundingRadius};
constexpr float INTERACTION_DISTANCE=5;
struct Creature;
enum class BotState {BOT_STATE_NON_COMBAT};constexpr int SKILL_NONE=0;
struct ObjectGuid{uint64_t value=1;uint64_t GetRawValue(){return value;}};
struct WorldObject{float GetVisibilityDistance(){return 60;}bool los=true;std::string name="Copper Vein";char const* GetName(){return name.c_str();}};
struct Unit:WorldObject{virtual ~Unit()=default;virtual Creature*ToCreature(){return nullptr;}bool IsWithinDistInMap(WorldObject*,float r,bool=true,SizeFactor=SizeFactor::BoundingRadius){return distance<=r;}bool combat=false,hostileLos=true;bool IsInCombat(){return combat;}bool IsWithinLOSInMap(WorldObject*,bool){return hostileLos;}uint32 level=25;float distance=0;uint32 GetLevel(){return level;}};
struct Creature:Unit{Creature*ToCreature()override{return this;}bool CanInitiateAttack(){return true;}float GetAttackDistance(Unit const*){return 18;}};
struct Map{float GetVisibilityDistance(){return 60;}bool IsDungeon(){return false;}};
struct Group{};
struct Player:Unit{Group* group=nullptr;Map map;Group* GetGroup(){return group;}Map* GetMap(){return &map;}};
struct AI{bool leader=false,active=true,debug=false;float masterDistance=0;std::list<Unit*> hostiles;std::vector<float> scans;std::vector<std::string> errors;
 bool IsGroupLeader(){return leader;}bool HasActivePlayerMaster(){return active;}bool HasStrategy(std::string,BotState){return debug;}
 std::list<Unit*> GetAllHostileNPCNonPetUnitsAroundWO(WorldObject*,float radius){scans.push_back(radius);std::list<Unit*> out;for(auto u:hostiles)if(u->distance<=radius)out.push_back(u);return out;}
 void TellError(Player*,std::string s){errors.push_back(s);}template<class...T>void TellPlayer(T...){}
 bool CanLootSomethingFromWO(WorldObject*){return false;}bool HasQuestItemsInWOLootList(WorldObject*){return false;}bool DoSpecificAction(char const*){return false;}
};
using PlayerbotAI=AI;
AI* currentAI=nullptr;AI* GetBotAI(Player*){return currentAI;}
struct {float lootHostileDistance=20,gatheringDistance=15,groupMemberGatheringDistance=10,groupMemberGatheringDistanceWithActiveMaster=5;}sPlayerbotAIConfig;
struct {bool IsWithinLOSInMap(Player*,WorldObject* o){return o->los;}float GetDistance2d(Player*,WorldObject*){return currentAI->masterDistance;}bool IsDistanceGreaterThan(float a,float b){return a>b;}}sServerFacade;
struct {int count=0;template<class...T>void outLoot(T...){++count;}}sLog;
WorldObject node;bool possible=true,empty=false;uint8 bagSpace=0;int skill=186;
struct LootObject{int skillId;LootObject(Player*,ObjectGuid):skillId(skill){}bool IsEmpty(){return empty;}WorldObject* GetWorldObject(Player*){return &node;}bool IsLootPossible(Player*){return possible;}bool IsApproachSafe(Player*,Unit** = nullptr);};
struct AddAllLootAction{AI* ai;Player* bot;int parentCalls=0;bool AddLoot(Player*,ObjectGuid guid,bool){++parentCalls;LootObject loot(bot,guid);return loot.IsApproachSafe(bot);}};
struct AddGatheringLootAction:AddAllLootAction{bool AddLoot(Player*,ObjectGuid,bool=false);};
namespace PlayerbotSecurityLevel{constexpr int PLAYERBOT_SECURITY_ALLOW_ALL=0;}
#define AI_VALUE(type,name) bagSpace
#include "NativeGathering.inc"
void check(bool result,char const* message){if(!result)throw std::runtime_error(message);}
int main(){try{
 Player bot,master;Group group;bot.group=&group;AI ai;currentAI=&ai;AddGatheringLootAction action;action.ai=&ai;action.bot=&bot;ObjectGuid guid;
 Creature distant,near;distant.distance=25;near.distance=10;ai.hostiles={&distant};
 check(action.AddLoot(&master,guid),"follower gathers when only hostile is beyond configured safety radius");
 check(ai.scans.size()==1&&ai.scans[0]==60&&action.parentCalls==1,"follower delegates to existing configured admission safety once");
 distant.combat=true;sPlayerbotAIConfig.lootHostileDistance=30;check(!action.AddLoot(&master,guid),"larger configured safety radius still blocks that hostile");sPlayerbotAIConfig.lootHostileDistance=20;distant.combat=false;
 ai.active=false;ai.masterDistance=8;ai.hostiles.clear();check(action.AddLoot(&master,guid),"inactive-master follower retains configured ten-yard gathering range");ai.active=true;ai.masterDistance=0;
 ai.scans.clear();ai.hostiles={&near};check(!action.AddLoot(&master,guid),"hostile inside safety radius still blocks gathering");
 sPlayerbotAIConfig.lootHostileDistance=0;ai.scans.clear();check(action.AddLoot(&master,guid)&&ai.scans.empty(),"configured safety opt-out is respected");sPlayerbotAIConfig.lootHostileDistance=20;
 ai.hostiles.clear();ai.masterDistance=6;int before=action.parentCalls;check(!action.AddLoot(&master,guid)&&action.parentCalls==before,"five-yard master gathering limit is preserved");
 ai.masterDistance=0;node.los=false;check(!action.AddLoot(&master,guid),"LOS remains required");node.los=true;
 possible=false;check(!action.AddLoot(&master,guid),"native skill, tool and loot policy remain required");possible=true;
 skill=SKILL_NONE;check(!action.AddLoot(&master,guid),"ordinary containers do not enter gathering path");skill=186;
 bagSpace=100;check(!action.AddLoot(&master,guid),"full bags still block automatic gathering");bagSpace=0;
 bot.group=nullptr;near.distance=10;distant.distance=25;ai.hostiles={&near,&distant};ai.scans.clear();
 check(!action.AddLoot(&master,guid)&&ai.scans.size()==1&&ai.scans[0]==30,"solo retains two-strong-hostiles native thirty-yard policy");
 ai.hostiles={&near};check(action.AddLoot(&master,guid),"solo retains one-hostile behavior");
 bot.group=&group;ai.leader=true;ai.hostiles={&near,&distant};check(!action.AddLoot(&master,guid),"group leader retains native strong-hostile policy");
 ai.leader=false;ai.hostiles.clear();ai.debug=true;node.los=false;int logs=sLog.count;
 check(!action.AddLoot(&master,guid)&&sLog.count==logs+1,"opt-in diagnostics record LOS rejection");node.los=true;
 ai.masterDistance=6;logs=sLog.count;check(!action.AddLoot(&master,guid)&&sLog.count==logs+1,"opt-in diagnostics record master range rejection");
 ai.debug=false;logs=sLog.count;check(!action.AddLoot(&master,guid)&&sLog.count==logs,"disabled diagnostics stay quiet");
 std::cout<<"Native gathering admission, shared safety, legacy solo policy and rejection diagnostics passed\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
