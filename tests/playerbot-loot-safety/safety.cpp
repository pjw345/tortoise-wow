#include <list>
#include <stdexcept>
#include <string>
#include <iostream>
struct Event {};
struct Unit {};
struct WorldObject {};
struct ObjectGuid { int id=0; unsigned long GetRawValue() const {return id;} bool IsCreature() const {return true;} };
struct PlayerbotAI {
    bool leader=false, activeMaster=true; int scans=0; std::list<Unit*> hostiles;
    bool IsGroupLeader() {return leader;}
    bool HasActivePlayerMaster() {return activeMaster;}
    bool HasStrategy(const char*, int) {return false;}
    std::list<Unit*> GetAllHostileNPCNonPetUnitsAroundWO(WorldObject*,float) {++scans; return hostiles;}
};
struct Player {bool grouped=true; PlayerbotAI* ai; void* GetGroup() {return grouped?this:nullptr;} const char* GetName(){return "test";}};
PlayerbotAI* GetBotAI(Player* p) {return p->ai;}
struct Config {float lootHostileDistance=30; unsigned lootTargetInspectDelay=10,lootTargetRetryDelay=5;} sPlayerbotAIConfig;
struct Log {template<class... T> void outLoot(T...) {}} sLog;
struct BotState {static const int BOT_STATE_NON_COMBAT=0;};
struct LootObject {
    ObjectGuid guid; WorldObject* object=nullptr;
    WorldObject* GetWorldObject(Player*) {return object;}
    bool IsLootPossible(Player*, bool* suppress) {*suppress=false;return true;}
    bool IsApproachSafe(Player*,Unit** blocker=nullptr);
};
struct LootObjectStack {int defers=0, removes=0; void Ignore(ObjectGuid,unsigned){} void Remove(ObjectGuid){++removes;} void Defer(ObjectGuid,unsigned){++defers;}} stack;
LootObject current; int nativeMoves=0;
template<class T> T value(const char*);
template<> LootObject value<LootObject>(const char*) {return current;}
template<> LootObjectStack* value<LootObjectStack*>(const char*) {return &stack;}
#define AI_VALUE(type,name) value<type>(name)
#define RESET_AI_VALUE(type,name) current=LootObject()
struct MoveToLootAction {Player* bot;PlayerbotAI* ai;ObjectGuid trackedLootGuid;bool Execute(Event&);};
#include "native-safety.inc"
void check(bool ok,const char* name) {if(!ok)throw std::runtime_error(name);}
int main() {
    PlayerbotAI ai;Player bot{true,&ai}; WorldObject corpse;Unit hostile;Event event;
    current.guid.id=1;current.object=&corpse;LootObject loot=current;Unit* blocker=&hostile;
    check(loot.IsApproachSafe(&bot,&blocker)&&blocker==nullptr,"safe loot clears blocker");
    ai.hostiles={&hostile};
    check(!loot.IsApproachSafe(&bot,&blocker)&&blocker==&hostile,"active master cannot bypass safety");
    ai.activeMaster=false;check(!loot.IsApproachSafe(&bot),"inactive master still protected");
    ai.activeMaster=true;
    MoveToLootAction move{&bot,&ai};
    check(!move.Execute(event)&&nativeMoves==0&&stack.defers==1&&current.object==nullptr,"new hostile blocks queued movement and defers target");
    ai.hostiles.clear();current=loot;
    check(move.Execute(event)&&nativeMoves==1,"safe target can approach after danger clears");
    int scans=ai.scans;
    sPlayerbotAIConfig.lootHostileDistance=0;ai.hostiles={&hostile};
    check(loot.IsApproachSafe(&bot)&&ai.scans==scans,"zero radius preserves explicit opt-out");
    sPlayerbotAIConfig.lootHostileDistance=30;bot.grouped=false;
    check(loot.IsApproachSafe(&bot)&&ai.scans==scans,"solo policy unchanged");
    bot.grouped=true;ai.leader=true;
    check(loot.IsApproachSafe(&bot)&&ai.scans==scans,"leader policy unchanged");
    ai.leader=false;loot.object=nullptr;
    check(!loot.IsApproachSafe(&bot),"missing target cannot approach");
    std::cout<<"Native safety and queued movement regression cases passed\n";
}
