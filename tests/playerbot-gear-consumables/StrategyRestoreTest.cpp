#include <cstdint>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
using uint8=uint8_t;
enum class BotState {BOT_STATE_COMBAT,BOT_STATE_NON_COMBAT,BOT_STATE_DEAD,BOT_STATE_REACTION,BOT_STATE_ALL};
struct Engine {bool initMode=false;std::set<std::string>strategies;void removeAllStrategies(){strategies.clear();}
 void addStrategy(std::string s){strategies.insert(s);}void Init(){}};
struct Player {char const*GetName(){return "Flore";}};
struct PlayerbotAI {Player player;Player*bot=&player;Engine storage[4];Engine*engines[4]={&storage[0],&storage[1],&storage[2],&storage[3]};Engine*reactionEngine=engines[3];
 bool active=true,real=false,pvp=false;int m_forcedRole=0;bool HasPlayerRelation(){return true;}bool HasActivePlayerMaster(){return active;}bool IsRealPlayer(){return real;}bool IsInPvp(){return pvp;}
 bool HasStrategy(std::string n,BotState s){return engines[int(s)]->strategies.count(n);}void ResetStrategies(bool);
};
struct Config {bool autoBuffConsumables=true;}sPlayerbotAIConfig;
struct AiFactory {static void AddDefaultCombatStrategies(Player*,PlayerbotAI*,Engine*){}static void AddDefaultNonCombatStrategies(Player*,PlayerbotAI*,Engine*){}
 static void AddDefaultDeadStrategies(Player*,PlayerbotAI*,Engine*){}static void AddDefaultReactionStrategies(Player*,PlayerbotAI*,Engine*){}};
struct DbStore {bool savedConsumables=false;void Load(PlayerbotAI*a){a->engines[1]->removeAllStrategies();a->engines[1]->addStrategy("follow");if(savedConsumables)a->engines[1]->addStrategy("consumables");}}sPlayerbotDbStore;
struct Logger {template<class...T>void outBasic(T...){}}sLog;
#include "NativeStrategyRestore.inc"
void check(bool b,char const*s){if(!b)throw std::runtime_error(s);}
int main(){try{
 PlayerbotAI ai;ai.ResetStrategies(true);check(ai.HasStrategy("consumables",BotState::BOT_STATE_NON_COMBAT)&&ai.HasStrategy("follow",BotState::BOT_STATE_NON_COMBAT),"legacy saved strategies gain buff use without losing follow");
 sPlayerbotAIConfig.autoBuffConsumables=false;ai.ResetStrategies(true);check(!ai.HasStrategy("consumables",BotState::BOT_STATE_NON_COMBAT),"automatic default can be disabled in configuration");
 sPlayerbotDbStore.savedConsumables=true;ai.ResetStrategies(true);check(ai.HasStrategy("consumables",BotState::BOT_STATE_NON_COMBAT),"explicit saved strategy retained when automatic addition disabled");sPlayerbotDbStore.savedConsumables=false;
 sPlayerbotAIConfig.autoBuffConsumables=true;
 ai.active=false;ai.ResetStrategies(true);check(!ai.HasStrategy("consumables",BotState::BOT_STATE_NON_COMBAT),"free bot defaults unaffected");ai.active=true;
 ai.pvp=true;ai.ResetStrategies(true);check(!ai.HasStrategy("consumables",BotState::BOT_STATE_NON_COMBAT),"PvP defaults unaffected");ai.pvp=false;
 ai.real=true;ai.ResetStrategies(true);check(!ai.HasStrategy("consumables",BotState::BOT_STATE_NON_COMBAT),"real player defaults unaffected");
 std::cout<<"Native strategy restoration and config scope passed\n";
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
