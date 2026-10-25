#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
using uint32=uint32_t;
using ObjectGuid=int;
using SkillType=int;
using LockType=int;
enum { GO_READY, GAMEOBJECT_TYPE_FISHINGNODE, LOCK_KEY_SKILL,
       SKILL_MINING=186, SKILL_HERBALISM=182, SKILL_FISHING=356 };
enum class BotState { BOT_STATE_NON_COMBAT };
uint32 SkillByLockType(LockType type) { return type; }
struct LockEntry { uint32 Type[8]={LOCK_KEY_SKILL}, Index[8]={SKILL_MINING}, Skill[8]={75}; } lockEntry;
struct LockStore { bool present=true; const LockEntry* LookupEntry(uint32) { return present?&lockEntry:nullptr; } } sLockStore;
struct Config { float grindDistance=100, gatheringDistance=5; } sPlayerbotAIConfig;
struct Group { uint32 id=1; uint32 GetId() { return id; } } group;
struct Info { uint32 GetLockId() { return 1; } } info;
struct GameObject {
 bool spawned=true, inUse=false, validInfo=true;
 int lootState=GO_READY, type=0; float distance=20, x=10, y=20;
 Info* GetGOInfo() { return validInfo?&info:nullptr; }
 bool IsInUse() { return inUse; } int getLootState() { return lootState; }
 int GetGoType() { return type; } ObjectGuid GetObjectGuid() { return 42; }
 float GetPositionX() { return x; } float GetPositionY() { return y; }
} node;
struct Player {
 bool world=true, teleport=false, alive=true, combat=false;
 uint32 map=0, instance=0, skill=75; Group* groupPtr=&group;
 bool IsInWorld() { return world; } bool IsBeingTeleported() { return teleport; }
 bool IsAlive() { return alive; } Group* GetGroup() { return groupPtr; }
 uint32 GetMapId() { return map; } uint32 GetInstanceId() { return instance; }
 uint32 GetSkillValue(uint32) { return skill; }
 bool IsWithinDistInMap(GameObject* go,float range) { return go->distance<=range; }
};
struct Facade {
 bool isSpawned(GameObject* go) { return go->spawned; }
 float GetDistance2d(Player*,GameObject* go) { return go->distance; }
 bool IsDistanceLessOrEqualThan(float a,float b) { return a<=b; }
 bool IsInCombat(Player* p) { return p->combat; }
} sServerFacade;
struct PlayerbotAI;
namespace ai { bool CanRevealGatheringItem(PlayerbotAI*,GameObject*); }
struct PlayerbotAI {
 Player* bot; bool reveal=true, hasSkill=true, nodeAvailable=true;
 int pings=0; float lastX=0;
 struct GatheringPingState { ObjectGuid guid=0; uint32 map=0,instance=0,group=0,delay=0,remaining=0; } gatheringPing;
 explicit PlayerbotAI(Player* p):bot(p) {}
 Player* GetBot() { return bot; }
 bool HasSkill(SkillType) { return hasSkill; }
 bool HasStrategy(const char*,BotState) { return reveal; }
 bool markers = false;
 bool HasResourceMarkers() const { return markers; }
 bool HasPendingGatheringPings() const { return gatheringPing.remaining!=0; }
 GameObject* GetGameObject(ObjectGuid guid) { return nodeAvailable&&guid==42?&node:nullptr; }
 void Ping(float x,float) { ++pings; lastX=x; }
 void StartGatheringPings(GameObject*); void UpdateGatheringPings(uint32);
};
#include "NativeGatheringPings.inc"
void require(bool b,const char* name) { if(!b) { std::cerr<<name<<'\n'; std::exit(1); } }
void reset() { node={}; group={}; sLockStore.present=true; lockEntry={}; }
int main() {
 { reset(); Player p; PlayerbotAI a(&p); a.markers=true; a.StartGatheringPings(&node);
   require(a.pings==0,"active icons suppress starting resource pings");
   a.markers=false; a.StartGatheringPings(&node); a.markers=true; a.UpdateGatheringPings(3000);
   require(a.pings==1 && !a.HasPendingGatheringPings(),"icon lease cancels pending repeats"); }

 reset(); Player p; PlayerbotAI a(&p); a.StartGatheringPings(&node);
 require(a.pings==1 && a.gatheringPing.remaining==3,"first ping immediate");
 a.UpdateGatheringPings(2999); require(a.pings==1,"pause between pings");
 node.x=30; a.UpdateGatheringPings(1); require(a.pings==2 && a.lastX==30,"fresh object lookup and exact boundary");
 a.StartGatheringPings(&node); require(a.pings==2,"do not restart an active sequence");
 a.UpdateGatheringPings(3000); a.UpdateGatheringPings(3000); a.UpdateGatheringPings(3000);
 require(a.pings==4 && !a.HasPendingGatheringPings(),"bounded four-ping sequence");
 reset(); Player q; PlayerbotAI delayed(&q); delayed.StartGatheringPings(&node); delayed.UpdateGatheringPings(20000);
 require(delayed.pings==2 && delayed.gatheringPing.delay==3000,"no catch-up burst");
 for(int variant=0;variant<19;++variant) {
  reset(); Player b; PlayerbotAI ai(&b); ai.StartGatheringPings(&node);
  switch(variant) {
   case 0:b.groupPtr=nullptr;break; case 1:group.id=2;break; case 2:b.map=1;break;
   case 3:b.instance=1;break; case 4:b.combat=true;break; case 5:b.alive=false;break;
   case 6:b.world=false;break; case 7:b.teleport=true;break; case 8:ai.reveal=false;break;
   case 9:ai.nodeAvailable=false;break; case 10:node.spawned=false;break;
   case 11:node.inUse=true;break; case 12:node.lootState=99;break;
   case 13:b.skill=74;break; case 14:ai.hasSkill=false;break;
   case 15:node.distance=101;break; case 16:node.distance=5;break;
   case 17:sLockStore.present=false;break; case 18:node.validInfo=false;break;
  }
  ai.UpdateGatheringPings(3000);
  require(ai.pings==1 && !ai.HasPendingGatheringPings(),"cancel invalid lifecycle or ineligible node");
 }
 reset(); Player herb; PlayerbotAI herbs(&herb); lockEntry.Index[0]=SKILL_HERBALISM;
 herbs.StartGatheringPings(&node); require(herbs.pings==1,"herbalism supported");
 reset(); Player fish; PlayerbotAI fishing(&fish); sLockStore.present=false; node.type=GAMEOBJECT_TYPE_FISHINGNODE;
 fishing.StartGatheringPings(&node); require(fishing.pings==1,"fishing preserved");
 std::cout<<"Gathering ping timing, eligibility and lifecycle cases passed\n";
}
