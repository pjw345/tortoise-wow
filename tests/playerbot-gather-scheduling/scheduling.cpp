#include <list>
#include <vector>
#include <map>
#include <string>
#include <utility>
#include <cstdarg>
#include <ctime>
#include <iostream>
#include <stdexcept>
using uint32=unsigned;
struct Event{};
struct ActionNode{std::string name;ActionNode(std::string n):name(n){}std::string const&getName(){return name;}};
#include "native-queue-types.inc"
struct NextAction{std::string name;float relevance;NextAction(std::string n,float r):name(n),relevance(r){}static NextAction**array(uint32,...);};
struct TriggerNode{std::string name;NextAction** handlers;TriggerNode(std::string n,NextAction**h):name(n),handlers(h){}~TriggerNode(){for(unsigned i=0;handlers[i];++i)delete handlers[i];delete[]handlers;}};
struct GatherStrategy{void InitNonCombatTriggers(std::list<TriggerNode*>&);};
struct LootNonCombatStrategy{void InitNonCombatTriggers(std::list<TriggerNode*>&);};
struct FollowMasterStrategy{void InitNonCombatTriggers(std::list<TriggerNode*>&);};
struct Item{};
struct Player{bool alive=true,sitting=false,casting=false;bool IsAlive(){return alive;}bool IsSitState(){return sitting;}bool IsNonMeleeSpellCasted(bool){return casting;}};
enum class BotState{BOT_STATE_NON_COMBAT};enum class BotCheatMask{item};
struct AI{bool food=false,cheat=false;bool HasStrategy(std::string,BotState){return food;}bool HasCheat(BotCheatMask){return cheat;}};
struct Trigger{bool active=false;bool IsActive(){return active;}};
struct Action{bool useful=true,possible=true;bool isUseful(){return useful;}bool isPossible(){return possible;}};
struct Context{std::map<std::string,Trigger>triggers;std::map<std::string,Action>actions;std::map<std::string,std::list<Item*>>items;
 Trigger*GetTrigger(std::string s){return &triggers[s];}Action*GetAction(std::string s){return &actions[s];}std::list<Item*>Inventory(std::string s){return items[s];}};
#define AI_VALUE2(type,name,qualifier) context->Inventory(qualifier)
struct AddAllLootAction{bool isUseful();};
struct AddGatheringLootAction:AddAllLootAction{Player*bot;AI*ai;Context*context;bool isUseful();};
#include "native-scheduling.inc"
void check(bool value,char const*message){if(!value)throw std::runtime_error(message);}
void push(Queue&q,std::string name,float relevance){q.Push(new ActionBasket(new ActionNode(name),relevance,false,Event()));}
std::string pop(Queue&q){ActionNode*n=q.Pop();if(!n)return "";auto result=n->getName();delete n;return result;}
float relevance(std::list<TriggerNode*>const&nodes,std::string name){for(auto node:nodes)for(unsigned i=0;node->handlers[i];++i)if(node->handlers[i]->name==name)return node->handlers[i]->relevance;throw std::runtime_error(name);}
int main(){
 std::list<TriggerNode*>gather,follow,loot;GatherStrategy().InitNonCombatTriggers(gather);FollowMasterStrategy().InitNonCombatTriggers(follow);LootNonCombatStrategy().InitNonCombatTriggers(loot);
 float g=relevance(gather,"add gathering loot"),f=relevance(follow,"follow");
 Queue q;push(q,"follow",f);push(q,"add gathering loot",g);
 check(pop(q)=="add gathering loot","eligible gather discovery must not starve behind follow");
 check(pop(q)=="follow","failed/no-candidate discovery leaves native follow available");
 check(gather.front()->name=="timer","existing one-second trigger retained");
 check(relevance(loot,"add all loot")==2,"ordinary broad-loot discovery unchanged");
 for(auto name:{"loot","move to loot","open loot"}){push(q,"add gathering loot",g);push(q,name,relevance(loot,name));check(pop(q)==name,"queued loot stages keep priority over scanning");pop(q);}
 push(q,"add gathering loot",g);push(q,"emergency",ACTION_EMERGENCY);check(pop(q)=="emergency","emergency outranks scan");pop(q);
 Player bot;AI ai;Context context;AddGatheringLootAction scan;scan.bot=&bot;scan.ai=&ai;scan.context=&context;
 check(scan.isUseful(),"healthy discovery permitted");
 bot.alive=false;check(!scan.isUseful(),"dead scan denied");bot.alive=true;
 bot.sitting=true;check(!scan.isUseful(),"active recovery/sitting protected");bot.sitting=false;
 bot.casting=true;check(!scan.isUseful(),"cast protected");bot.casting=false;
 Item supply;ai.food=true;
 for(auto pair:{std::make_pair("low health","food"),std::make_pair("high mana","drink")}){
  context.triggers[pair.first].active=true;context.items[pair.second]={&supply};
  check(!scan.isUseful(),"ready native recovery protected");
  context.items[pair.second].clear();check(scan.isUseful(),"missing supplies do not deadlock gathering");
  ai.cheat=true;check(!scan.isUseful(),"native cheat recovery protected");ai.cheat=false;
  context.items[pair.second]={&supply};context.actions[pair.second].useful=false;check(scan.isUseful(),"native not-useful recovery does not block");context.actions[pair.second].useful=true;
  context.actions[pair.second].possible=false;check(scan.isUseful(),"native unavailable recovery does not block");context.actions[pair.second].possible=true;
  ai.food=false;check(scan.isUseful(),"disabled food strategy unchanged");ai.food=true;
  context.triggers[pair.first].active=false;context.items[pair.second].clear();
 }
 for(auto list:{gather,follow,loot})for(auto n:list)delete n;
 std::cout<<"PASS: native queue/strategy ordering, follow fallback, loot priorities and delegated recovery guards\n";
}
