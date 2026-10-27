#include <list>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>
#include <iostream>
using uint32=unsigned;using int32=int;
enum class SizeFactor{None,BoundingRadius};
enum{CONFIG_FLOAT_RATE_CREATURE_AGGRO,SPELL_AURA_MOD_DETECT_RANGE,SPELL_AURA_MOD_DETECTED_RANGE};
constexpr int MAX_LEVEL_DIFF_FOR_AGGRO_RANGE=25;
constexpr float INTERACTION_DISTANCE=5;
constexpr uint32 SKILL_NONE=0;
struct Event {};
struct WorldObject{float visibility=60;float GetVisibilityDistance(){return visibility;}float boundingRadius=0;bool los=true;};
struct Creature;struct Player;
struct Unit:WorldObject{
 int losQueries=0;bool combat=false;unsigned level=27;float distance=0,zDistance=0;int detectedMod=0;Player* owner=nullptr;
 virtual ~Unit()=default;virtual Creature*ToCreature(){return nullptr;}virtual bool IsPet()const{return false;}
 bool IsInCombat(){return combat;}bool IsWithinLOSInMap(WorldObject*,bool){++losQueries;return los;}
 bool IsWithinDistInMap(WorldObject* o,float radius,bool=true,SizeFactor factor=SizeFactor::BoundingRadius){float allowance=factor==SizeFactor::BoundingRadius?boundingRadius+o->boundingRadius:0;return std::hypot(distance,zDistance)<radius+allowance;}
 unsigned GetLevel()const{return level;}unsigned GetLevelForTarget(Unit const*)const{return level;}
 Player*GetCharmerOrOwnerPlayer()const{return owner;}int GetTotalAuraModifier(int)const{return detectedMod;}
 const char*GetName(){return "hostile";}
};
struct Map{float GetVisibilityDistance(){return 60;}};
struct Aura{struct Spell{unsigned MaxTargetLevel=70;}spell;struct Mod{int m_amount=-10;}mod;Spell const*GetSpellProto(){return &spell;}Mod const*GetModifier(){return &mod;}};
using AuraList=std::list<Aura*>;
struct Creature:Unit{bool initiates=true;float detection=20;AuraList auras;Creature*ToCreature()override{return this;}bool CanInitiateAttack(){return initiates;}float GetDetectionRange()const{return detection;}AuraList const&GetAurasByType(int)const{return auras;}float GetAttackDistance(Unit const*)const;};
struct Pet:Unit{bool enabled=true;bool IsPet()const override{return true;}bool IsEnabled()const{return enabled;}};
struct World{float rate=1;float getConfig(int){return rate;}}sWorld;
struct ObjectGuid { int id=0; unsigned long GetRawValue() const {return id;} bool IsCreature() const {return true;} };
struct PlayerbotAI {
    bool leader=false, activeMaster=true; int scans=0; float lastRadius=0; std::list<Unit*> hostiles;
    bool IsGroupLeader() {return leader;}
    bool HasActivePlayerMaster() {return activeMaster;}
    bool HasStrategy(const char*, int) {return false;}
    std::list<Unit*> GetAllHostileNPCNonPetUnitsAroundWO(WorldObject*,float radius) {++scans;lastRadius=radius;std::list<Unit*> result;for(Unit*u:hostiles)if(std::hypot(u->distance,u->zDistance)<=radius)result.push_back(u);return result;}
};
struct Player:Unit {Map map;Map*GetMap(){return &map;}bool grouped=true; PlayerbotAI* ai; void* GetGroup() {return grouped?this:nullptr;} const char* GetName(){return "test";}};
PlayerbotAI* GetBotAI(Player* p) {return p->ai;}
struct Config {float lootHostileDistance=30; unsigned lootTargetInspectDelay=10,lootTargetRetryDelay=5;} sPlayerbotAIConfig;
struct Log {template<class... T> void outLoot(T...) {}} sLog;
struct BotState {static const int BOT_STATE_NON_COMBAT=0;};
struct LootObject {
    uint32 skillId=SKILL_NONE;
    ObjectGuid guid; WorldObject* object=nullptr;
    WorldObject* GetWorldObject(Player*) {return object;}
    bool IsLootPossible(Player*, bool* suppress) {*suppress=false;return true;}
    bool IsApproachSafe(Player*,Unit** blocker=nullptr);
    static bool IsWorldObjectApproachSafe(Player*,WorldObject*,Unit** blocker=nullptr);
};
struct LootObjectStack {int defers=0, removes=0; void Ignore(ObjectGuid,unsigned,bool=false,bool=false){} void Remove(ObjectGuid){++removes;} void Defer(ObjectGuid,unsigned){++defers;}} stack;
LootObject current; int nativeMoves=0;
template<class T> T value(const char*);
template<> LootObject value<LootObject>(const char*) {return current;}
template<> LootObjectStack* value<LootObjectStack*>(const char*) {return &stack;}
#define AI_VALUE(type,name) value<type>(name)
#define RESET_AI_VALUE(type,name) current=LootObject()
struct MoveToLootAction {Player* bot;PlayerbotAI* ai;ObjectGuid trackedLootGuid;bool Execute(Event&);};
#include "native-aggro.inc"
#include "native-safety.inc"
void check(bool ok,const char* name) {if(!ok)throw std::runtime_error(name);}
int main() {try{
    PlayerbotAI ai;Player bot;bot.ai=&ai; WorldObject corpse;Creature hostile;Event event;
    current.guid.id=1;current.object=&corpse;LootObject loot=current;Unit* blocker=&hostile;
    check(loot.IsApproachSafe(&bot,&blocker)&&blocker==nullptr,"safe loot clears blocker");
    check(LootObject::IsWorldObjectApproachSafe(&bot,&corpse,&blocker)&&blocker==nullptr,"reporting target uses native safe result");
    ai.hostiles={&hostile};
    check(!LootObject::IsWorldObjectApproachSafe(&bot,&corpse,&blocker)&&blocker==&hostile,"reporting target uses native threat blocker");
    check(!loot.IsApproachSafe(&bot,&blocker)&&blocker==&hostile,"active master cannot bypass safety");
    ai.activeMaster=false;check(!loot.IsApproachSafe(&bot),"inactive master still protected");
    hostile.los=false;
    check(loot.IsApproachSafe(&bot,&blocker)&&blocker==nullptr,"idle enemy separated by wall/floor does not block destination");
    hostile.combat=true;
    check(!loot.IsApproachSafe(&bot,&blocker)&&blocker==&hostile,"engaged enemy remains conservative despite missing LOS");
    hostile.combat=false;hostile.los=true;
    check(!loot.IsApproachSafe(&bot),"visible same-floor or ramp threat still blocks destination");

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
    ai.leader=false;sPlayerbotAIConfig.lootHostileDistance=15;
    bot.level=27;hostile.level=13;hostile.distance=12;hostile.losQueries=0;
    check(hostile.GetAttackDistance(&bot)==6&&loot.IsApproachSafe(&bot),"level 27 can approach loot twelve yards from level 13 mob");
    check(hostile.losQueries==0,"expanded discovery does not cast LOS rays for out-of-aggro candidates");
    hostile.distance=9.9f;check(!loot.IsApproachSafe(&bot),"native minimum aggro plus interaction area remains protected");
    bot.level=10;hostile.level=20;hostile.distance=25;
    check(hostile.GetAttackDistance(&bot)==30&&!loot.IsApproachSafe(&bot)&&ai.lastRadius==60,"higher-level threat beyond old fifteen-yard scan is found");
    bot.level=20;hostile.level=10;hostile.distance=16;
    check(hostile.GetAttackDistance(&bot)==10&&loot.IsApproachSafe(&bot),"lower-level mob allows closer destination");
    hostile.distance=14.9f;check(!loot.IsApproachSafe(&bot),"interaction positions inside lower-level radius remain blocked");
    bot.level=1;hostile.level=60;check(hostile.GetAttackDistance(&bot)==45,"native low-player-level cap preserved");
    bot.level=60;hostile.level=1;check(hostile.GetAttackDistance(&bot)==5,"native minimum preserved");
    bot.level=10;hostile.level=20;Aura soothe;hostile.auras={&soothe};bot.detectedMod=3;sWorld.rate=2;hostile.distance=44;
    check(hostile.GetAttackDistance(&bot)==46&&!loot.IsApproachSafe(&bot),"native auras and server aggro rate used together");
    hostile.distance=52;check(loot.IsApproachSafe(&bot),"outside modified interaction boundary allowed");
    sWorld.rate=0;hostile.distance=1;check(loot.IsApproachSafe(&bot),"disabled native proximity aggro does not block idle loot");
    sWorld.rate=1;hostile.auras.clear();bot.detectedMod=0;hostile.detection=40;hostile.distance=50;
    check(!loot.IsApproachSafe(&bot),"custom native detection range retained");
    hostile.detection=0;check(loot.IsApproachSafe(&bot),"non-detecting idle creature does not block");hostile.detection=20;
    hostile.initiates=false;hostile.distance=1;check(loot.IsApproachSafe(&bot),"non-initiating creature does not block idle loot");hostile.initiates=true;
    bot.level=27;hostile.level=13;hostile.combat=true;hostile.los=false;hostile.distance=14;check(!loot.IsApproachSafe(&bot),"engaged threat still blocks without LOS inside configured combat radius");
    hostile.distance=16;check(loot.IsApproachSafe(&bot),"low-level engaged threat keeps configured combat minimum");
    bot.level=10;hostile.level=20;hostile.distance=25;check(!loot.IsApproachSafe(&bot),"higher-level engaged threat also expands beyond combat minimum");
    hostile.distance=36;check(loot.IsApproachSafe(&bot),"outside higher-level engaged threat remains allowed");
    hostile.combat=false;hostile.los=true;bot.level=27;hostile.level=13;hostile.distance=8;hostile.zDistance=8;
    check(loot.IsApproachSafe(&bot),"native three-dimensional separation retained");hostile.zDistance=0;
    check(!loot.IsApproachSafe(&bot),"same planar position inside effective radius blocks");
    hostile.combat=true;hostile.boundingRadius=2;corpse.boundingRadius=1;hostile.distance=17;
    check(!loot.IsApproachSafe(&bot),"engaged exclusion preserves native body-size allowance");
    hostile.combat=false;hostile.distance=11.5f;
    check(loot.IsApproachSafe(&bot),"idle native aggro uses centres rather than body-size expansion");
    hostile.boundingRadius=0;corpse.boundingRadius=0;
    Creature weak,strong;weak.level=13;weak.distance=12;strong.level=40;strong.distance=25;
    ai.hostiles={&weak,&strong};blocker=nullptr;
    check(!loot.IsApproachSafe(&bot,&blocker)&&blocker==&strong,"safe low-level candidate cannot mask a dangerous higher-level candidate");
    strong.detection=80;strong.distance=70;bot.visibility=120;ai.hostiles={&strong};
    check(!loot.IsApproachSafe(&bot)&&ai.lastRadius==120,"native visibility overrides expand candidate search too");
    bot.visibility=60;ai.hostiles={&hostile};
    bot.level=27;hostile.level=13;hostile.combat=false;hostile.distance=0;hostile.zDistance=4;hostile.los=false;
    check(loot.IsApproachSafe(&bot),"idle mob directly overhead behind a solid floor does not block lower-floor loot");
    hostile.los=true;check(!loot.IsApproachSafe(&bot),"open vertical connection inside native threat range remains unsafe");
    hostile.distance=4;hostile.zDistance=3;check(!loot.IsApproachSafe(&bot),"stairs or ramp at a different height are not blanket-excluded");
    hostile.distance=0;hostile.zDistance=30;check(loot.IsApproachSafe(&bot),"distant upper floor outside native three-dimensional threat range is safe");
    hostile.zDistance=4;hostile.los=false;hostile.combat=true;
    check(!loot.IsApproachSafe(&bot),"already-engaged enemies retain conservative protection across floors");
    hostile.combat=false;hostile.los=true;hostile.zDistance=0;
    ai.leader=false;loot.object=nullptr;
    check(!loot.IsApproachSafe(&bot),"missing target cannot approach");
    std::cout<<"Native safety and queued movement regression cases passed\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}
}
