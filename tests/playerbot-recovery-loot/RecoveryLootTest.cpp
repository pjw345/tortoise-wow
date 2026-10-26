#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
using uint32 = uint32_t;
using ObjectGuid = int;
enum { POWER_MANA, UNIT_STAND_STATE_STAND, GAMEOBJECT_TYPE_CHEST = 4, GO_STATE_READY,
       SKILL_MINING = 186, SKILL_HERBALISM = 182, SKILL_LOCKPICKING = 633 };
using SkillType = int;
enum class BotState { BOT_STATE_NON_COMBAT };
enum class PlayerbotSecurityLevel { PLAYERBOT_SECURITY_ALLOW_ALL };
struct Player {
 bool world=true, teleport=false, alive=true, sit=true, combat=false, cast=false, mana=true;
 uint32 hp=100, maxHp=100, mp=100, maxMp=100, skill=0;
 int stands=0;
 bool IsInWorld() { return world; } bool IsBeingTeleported() { return teleport; }
 bool IsAlive() { return alive; } bool IsSitState() { return sit; }
 bool IsNonMeleeSpellCasted(bool) { return cast; } bool HasMana() { return mana; }
 uint32 GetHealth() { return hp; } uint32 GetMaxHealth() { return maxHp; }
 uint32 GetPower(int) { return mp; } uint32 GetMaxPower(int) { return maxMp; }
 uint32 GetSkillValue(int) { return skill; }
 void SetStandState(int) { sit=false; ++stands; }
};
struct GameObject {
 bool spawned=true, inUse=false; int state=GO_STATE_READY, type=GAMEOBJECT_TYPE_CHEST;
 uint32 skill=0, required=1;
 ObjectGuid GetObjectGuid() { return 42; } bool IsInUse() { return inUse; }
 int GetGoState() { return state; } int GetGoType() { return type; }
};
struct Facade {
 bool IsInCombat(Player* p) { return p->combat; }
 bool isSpawned(GameObject* p) { return p->spawned; }
} sServerFacade;
constexpr float INTERACTION_DISTANCE=5;
float distanceToObject=0;
// Keep the distance adapter separate from gameplay policy.
struct Bot : Player { float GetDistance(GameObject*) { return distanceToObject; } };
struct Event {};
struct Chat { std::string formatGameobject(GameObject*) { return "node"; } };
struct LootObject {
 uint32 skillId=0, reqSkillValue=0;
 LootObject() = default;
 LootObject(Player*, ObjectGuid);
};
GameObject* selectedObject=nullptr;
LootObject::LootObject(Player*, ObjectGuid) { skillId=selectedObject->skill; reqSkillValue=selectedObject->required; }
bool CanOpenActivatedQuestChest(Player*, GameObject*) { return false; }
struct PlayerbotAI {
 Bot* bot; Player* master=nullptr;
 bool recoveringFood=false, recoveringDrink=false, isWaiting=false, stay=false, hasSkill=false, openResult=true;
 uint32 aiInternalUpdateDelay=24000;
 int opens=0, messages=0, selections=0;
 std::string lastMessage;
 explicit PlayerbotAI(Bot* b):bot(b) {}
 void BeginConsumableRecovery(bool, bool); void UpdateConsumableRecovery();
 bool HasStrategy(const char*, BotState) { return stay; }
 void SetAIInternalUpdateDelay(uint32 value) { aiInternalUpdateDelay=value; }
 bool HasSkill(SkillType) { return hasSkill; }
 bool DoSpecificAction(const char*, Event&, bool) { ++opens; return openResult; }
 void TellPlayerNoFacing(Player*, const std::string& message, PlayerbotSecurityLevel, bool) { ++messages; lastMessage=message; }
};
#define SET_AI_VALUE(type, name, value) do { ++ai->selections; } while(0)
struct UseAction { Bot* bot; PlayerbotAI* ai; Chat* chat; bool UseGameObject(Player*, Event&, GameObject*); };
#include "NativeMethods.inc"
void require(bool condition, const char* name) { if(!condition) { std::cerr<<name<<'\n'; std::exit(1); } }
int main() {
 Bot b; PlayerbotAI ai(&b);
 ai.UpdateConsumableRecovery(); require(b.stands==0 && ai.aiInternalUpdateDelay==24000,"untracked wait untouched");
 ai.BeginConsumableRecovery(false,true); b.mp=99; ai.UpdateConsumableRecovery();
 require(b.stands==0 && ai.recoveringDrink,"wait for full mana");
 b.mp=100; ai.UpdateConsumableRecovery(); require(b.stands==1 && ai.aiInternalUpdateDelay==0,"finish drinking and wake");
 ai.UpdateConsumableRecovery(); require(b.stands==1,"completion idempotent");
 for(int variant=0;variant<14;++variant) {
  Bot p; Player master; PlayerbotAI a(&p); a.BeginConsumableRecovery(true,true);
  switch(variant) {
   case 0:p.hp=99;break; case 1:p.mp=99;break; case 2:p.cast=true;break;
   case 3:p.combat=true;break; case 4:p.alive=false;break; case 5:p.sit=false;break;
   case 6:p.world=false;break; case 7:p.teleport=true;break;
   case 8:a.master=&master;break; case 9:a.stay=true;break; case 10:a.isWaiting=true;break;
   case 11:a.aiInternalUpdateDelay=0;break; case 12:p.mana=false;break;
   case 13:a.master=&master;master.sit=false;break;
  }
  a.UpdateConsumableRecovery();
  require(p.stands==((variant==12 || variant==13)?1:0),"recovery state/intent/food guards");
 }
 { Bot p; p.hp=50; PlayerbotAI a(&p); a.BeginConsumableRecovery(false,true); a.UpdateConsumableRecovery(); require(p.stands==1,"drink-only does not wait for absent food"); }
 { Bot p; p.mp=50; PlayerbotAI a(&p); a.BeginConsumableRecovery(true,false); a.UpdateConsumableRecovery(); require(p.stands==1,"food-only does not wait for absent drink"); }
 for(int skill : {int(SKILL_MINING),int(SKILL_HERBALISM),int(SKILL_LOCKPICKING),0}) {
  for(int state=0;state<6;++state) {
   Bot p; PlayerbotAI a(&p); Chat chat; Event event;
   GameObject go; go.skill=skill; go.required=75; selectedObject=&go;
   a.hasSkill=state!=0; p.skill=state==1?74:75; a.openResult=state!=3;
   if(state==4) p.skill=76;
   if(state==5) { go.required=0; p.skill=0; }
   UseAction action{&p,&a,&chat};
   bool result=action.UseGameObject(nullptr,event,&go);
   bool skip=(skill==SKILL_MINING||skill==SKILL_HERBALISM)&&state<2;
   require(a.opens==(skip?0:1) && a.selections==(skip?0:1),"skip ineligible profession without changing selected loot");
   bool profession=skill==SKILL_MINING||skill==SKILL_HERBALISM;
   require(a.messages==((profession && state==0)?0:1),"only missing profession stays silent");
   if(profession && state==1) {
    std::string expected=skill==SKILL_MINING
      ? "Failed to mine node: requires Mining 75; my Mining is 74."
      : "Failed to gather node: requires Herbalism 75; my Herbalism is 74.";
    require(a.lastMessage==expected,"skill shortage includes formatted node and native current/required skill");
   }
   if(!skip) require(a.lastMessage==(state==3?"Failed to loot node":"Looting node"),"preserve normal open feedback");
   require(result==(!skip && state!=3),"preserve open result");
  }
 }
 std::cout<<"Recovery and profession-command cases passed\n";
}
