#include <algorithm>
#include <cstdint>
#include <iostream>
#include <list>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using uint32=uint32_t;using uint8=uint8_t;using int32=int32_t;
constexpr uint32 IN_MILLISECONDS=1000;
constexpr int SKILL_MINING=186,SKILL_HERBALISM=182,GAMEOBJECT_TYPE_CHEST=3,GAMEOBJECT_TYPE_GOOBER=10;
constexpr int GO_READY=1,GO_ACTIVATED=2,GO_JUST_DEACTIVATED=3,LOOT_CORPSE=1,LOOT_SKINNING=6;
constexpr int NOT_GROUP_TYPE_LOOT=0,SPELL_EFFECT_CREATE_ITEM=24,MAX_QUEST_LOG_SIZE=25;
enum class BotState{BOT_STATE_NON_COMBAT};
enum class ForceItemUsage{FORCE_USAGE_NONE,FORCE_USAGE_NEED,FORCE_USAGE_GREED};
ForceItemUsage forced=ForceItemUsage::FORCE_USAGE_NONE;
struct AiObjectContext{};
struct ObjectGuid {uint32 id=1;bool go=true;bool IsGameObject()const{return go;}};
struct ItemPrototype {uint32 MaxCount=0,StartQuest=0;std::string Name1="Test item";};
struct Quest {uint32 ReqItemId[4]={},ReqItemCount[4]={};};
struct LootItem {uint32 itemId=1,count=1;};
struct Loot {uint32 gold=0;int type=LOOT_CORPSE;std::vector<LootItem> items;uint32 GetGoldAmount(){return gold;}};
struct WorldObject {virtual ~WorldObject()=default;virtual bool IsGameObject(){return false;}virtual bool IsCreature(){return false;}};
struct Creature:WorldObject {Loot* m_loot=nullptr;bool IsCreature()override{return true;}};
struct GOInfo {uint32 lootId=1;uint32 GetLootId(){return lootId;}};
struct GameObject:WorldObject {
 int kind=GAMEOBJECT_TYPE_CHEST,state=GO_READY,skill=SKILL_MINING;bool spawned=true,inUse=false;Loot* m_loot=nullptr;GOInfo info;
 bool IsGameObject()override{return true;}int GetGoType(){return kind;}int GetLootState(){return state;}
 bool IsInUse(){return inUse;}GOInfo* GetGOInfo(){return &info;}uint32 GetEntry(){return 1731;}
 ObjectGuid GetObjectGuid(){return {};}uint32 GetSpellId(){return 0;}
};
WorldObject* current=nullptr;
struct Player {bool alive=true,combat=false;bool IsAlive(){return alive;}bool IsInCombat(){return combat;}
 uint32 GetInstanceId(){return 0;}bool HasItemCount(uint32,uint32,bool){return false;}uint32 GetQuestSlotQuestId(uint8){return 0;}};
struct LootObjectStack {int queued=0,defers=0;uint32 delay=0;void Defer(ObjectGuid,uint32 d){++defers;delay=d;}}stack;
struct PlayerbotAI {Player* bot;bool gather=true,admission=true;int admits=0;AiObjectContext context;
 Player* GetBot(){return bot;}Player* GetMaster(){return nullptr;}AiObjectContext* GetAiObjectContext(){return &context;}
 bool HasStrategy(std::string,BotState){return gather;}GameObject* GetGameObject(ObjectGuid){return dynamic_cast<GameObject*>(current);}};
struct AddGatheringLootAction {PlayerbotAI* ai;explicit AddGatheringLootAction(PlayerbotAI* a):ai(a){}
 bool AddLoot(Player*,ObjectGuid){++ai->admits;if(ai->admission){++stack.queued;return true;}return false;}};
std::set<uint32> alwaysLoot,needed;std::list<uint32> sourceItems;std::set<uint32> skipItems;
struct {std::list<uint32> autoLootSkipItems={20516,51751};uint32 lootTargetRetryDelay=5,lootDelay=1000;bool syncQuestWithPlayer=false;}sPlayerbotAIConfig;
struct {bool isSpawned(GameObject* go){return go->spawned;}}sServerFacade;
struct {ItemPrototype proto;ItemPrototype const* GetItemPrototype(uint32){return &proto;}Quest const* GetQuestTemplate(uint32){return nullptr;}}sObjectMgr;
struct ItemUsageValue {static bool IsNeededForQuest(Player*,uint32 id){return needed.count(id)!=0;}};
struct ItemQualifier {uint32 id;explicit ItemQualifier(uint32 i):id(i){}explicit ItemQualifier(LootItem* i):id(i->itemId){}uint32 GetId(){return id;}};
struct LootStrategyValue {static bool CanLoot(ItemQualifier&,PlayerbotAI*){return true;}};
struct WorldPosition {explicit WorldPosition(Player*){}};
struct GuidPosition {GuidPosition(uint64_t,WorldPosition){}explicit operator bool(){return current!=nullptr;}WorldObject* GetWorldObject(uint32){return current;}};
struct LootObject {int skillId;LootObject(Player*,ObjectGuid):skillId(dynamic_cast<GameObject*>(current)->skill){}};
struct LootAccess {Loot* loot;explicit LootAccess(Loot* l):loot(l){}int lootMethod(){return NOT_GROUP_TYPE_LOOT;}
 bool isChecked(){return true;}int lootType(){return loot->type;}std::vector<LootItem*> GetLootContentFor(Player*){std::vector<LootItem*> out;for(auto& i:loot->items)out.push_back(&i);return out;}};
struct SpellEntry {int Effect[3]={};uint32 EffectItemType[3]={};};
struct SpellStore {template<class T>T const* LookupEntry(uint32){return nullptr;}}spellStore;
SpellStore* GetSpellStore(){return &spellStore;}
template<class T>T value(std::string);
template<>std::set<uint32>& value<std::set<uint32>&>(std::string n){return n=="skip loot list"?skipItems:alwaysLoot;}
template<>LootObjectStack* value<LootObjectStack*>(std::string){return &stack;}
template<class T,class K>T value2(std::string n,K){return n=="item count"?0:100;}
#define AI_VALUE(type,name) value<type>(name)
#define AI_VALUE2(type,name,key) value2<type>(name,key)
#define AI_VALUE2_EXISTS(type,name,key,fallback) ::forced
#define GAI_VALUE2(type,name,key) sourceItems
struct StoreLootAction {PlayerbotAI* ai;Player* bot;static bool IsLootExcludedByDefault(uint32,PlayerbotAI*);
 static bool IsLootAllowed(ItemQualifier&,PlayerbotAI*);bool QueueNextMiningAttempt(Player*,ObjectGuid);};
struct ShouldLootObject {PlayerbotAI* ai;Player* bot;std::string getQualifier(){return "1";}bool Calculate();};
#include "NativeLoot.inc"
void check(bool ok,char const* label){if(!ok)throw std::runtime_error(label);}
int main(){try{
 Player bot,master;PlayerbotAI ai{&bot};GameObject node;Loot empty;node.m_loot=&empty;current=&node;
 StoreLootAction store{&ai,&bot};ShouldLootObject selection{&ai,&bot};ObjectGuid guid;
 check(selection.Calculate(),"cleared loot on a still-ready mineral vein remains selectable");
 check(store.QueueNextMiningAttempt(&master,guid)&&stack.queued==1&&stack.defers==1&&stack.delay==1,"successful repeat uses loot pacing instead of failure backoff");
 for(auto const& test:std::vector<std::pair<uint32,uint32>>{{0,1},{750,1},{1000,1},{1500,2},{2500,3},{5000,5},{UINT32_MAX,4294968}}){
  sPlayerbotAIConfig.lootDelay=test.first;
  check(store.QueueNextMiningAttempt(&master,guid)&&stack.delay==test.second,"repeat pacing rounds up safely, keeps minimum, and honors configured loot delay");
  check(sPlayerbotAIConfig.lootTargetRetryDelay==5,"successful pacing does not change failure backoff");
 }
 sPlayerbotAIConfig.lootDelay=1000;
 for(int uses=0;uses<3;++uses)check(selection.Calculate()&&store.QueueNextMiningAttempt(&master,guid),"vein can repeat for its native remaining uses");
 node.state=GO_JUST_DEACTIVATED;int attempts=ai.admits;
 check(!store.QueueNextMiningAttempt(&master,guid)&&ai.admits==attempts&&!selection.Calculate(),"native depletion ends mining");
 node.state=GO_READY;node.spawned=false;check(!store.QueueNextMiningAttempt(&master,guid),"despawn stops retries");node.spawned=true;
 node.inUse=true;check(!store.QueueNextMiningAttempt(&master,guid),"in-use vein is not recast");node.inUse=false;
 node.state=GO_ACTIVATED;check(!store.QueueNextMiningAttempt(&master,guid),"pending loot must be released before retry");node.state=GO_READY;
 bot.combat=true;check(!store.QueueNextMiningAttempt(&master,guid),"combat stops retry");bot.combat=false;
 bot.alive=false;check(!store.QueueNextMiningAttempt(&master,guid),"death stops retry");bot.alive=true;
 ai.gather=false;check(!store.QueueNextMiningAttempt(&master,guid),"disabled gathering stays disabled");ai.gather=true;
 ai.admission=false;check(!store.QueueNextMiningAttempt(&master,guid),"normal admission can reject range/safety/skill/tools/bags");ai.admission=true;
 check(!store.QueueNextMiningAttempt(nullptr,guid),"missing requester stops retry");
 guid.go=false;check(!store.QueueNextMiningAttempt(&master,guid),"corpses never become mining retries");guid.go=true;
 node.skill=SKILL_HERBALISM;check(!store.QueueNextMiningAttempt(&master,guid),"herbs retain native one-use lifecycle");
 check(!selection.Calculate(),"empty herb is not treated as repeatable mining");
 node.skill=0;check(!store.QueueNextMiningAttempt(&master,guid)&&!selection.Calculate(),"empty ordinary containers do not loop");
 ItemQualifier apple(20516),bone(51751),drink(1205);
 check(!StoreLootAction::IsLootAllowed(apple,&ai)&&!StoreLootAction::IsLootAllowed(bone,&ai),"configured novelty exclusions apply to actual loot policy");
 check(StoreLootAction::IsLootAllowed(drink,&ai),"ordinary useful drink remains allowed");
 alwaysLoot.insert(20516);check(StoreLootAction::IsLootAllowed(apple,&ai),"existing always-loot override is retained");alwaysLoot.clear();
 needed.insert(51751);check(StoreLootAction::IsLootAllowed(bone,&ai),"active quest requirement takes precedence");needed.clear();
 skipItems.insert(51751);needed.insert(51751);check(!StoreLootAction::IsLootAllowed(bone,&ai),"explicit manual skip retains its existing priority");skipItems.clear();needed.clear();
 node.m_loot=nullptr;sourceItems={20516};check(!selection.Calculate(),"unopened source containing only excluded apples is not approached");
 sourceItems={51751};check(!selection.Calculate(),"unneeded bone-only source is not approached");
 needed.insert(51751);check(selection.Calculate(),"bone source remains available for active quest");needed.clear();
 sourceItems={20516,1205};check(selection.Calculate(),"mixed source still opens for useful loot");
 sourceItems={};check(selection.Calculate(),"unknown sources keep native inspection");
 sourceItems={20516};alwaysLoot.insert(20516);check(selection.Calculate(),"explicit always-loot permits source discovery too");alwaysLoot.clear();
 forced=ForceItemUsage::FORCE_USAGE_NEED;check(StoreLootAction::IsLootAllowed(apple,&ai)&&selection.Calculate(),"existing forced Need overrides novelty defaults for item and source");
 forced=ForceItemUsage::FORCE_USAGE_GREED;check(StoreLootAction::IsLootAllowed(apple,&ai)&&selection.Calculate(),"existing forced Greed overrides novelty defaults for item and source");
 skipItems.insert(20516);check(!StoreLootAction::IsLootAllowed(apple,&ai),"explicit skip still overrides forced usage");skipItems.clear();forced=ForceItemUsage::FORCE_USAGE_NONE;
 sPlayerbotAIConfig.autoLootSkipItems.clear();check(StoreLootAction::IsLootAllowed(apple,&ai)&&selection.Calculate(),"empty configuration restores native policy");
 std::cout<<"Native mining depletion/requeue, novelty policy and source-admission cases passed\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
