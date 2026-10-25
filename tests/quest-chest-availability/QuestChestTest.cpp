#include <map>
#include <vector>
#include <set>
#include <string>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <ctime>
using uint8=unsigned char;using uint32=unsigned;using int32=int;using ObjectGuid=unsigned;
const unsigned MAX_NR_LOOT_ITEMS=16;
enum {GAMEOBJECT_TYPE_CHEST=3,GAMEOBJECT_TYPE_DOOR=0,GAMEOBJECT_TYPE_FISHINGNODE=17,GAMEOBJECT_TYPE_FISHINGHOLE=25};
enum {GO_READY,GO_ACTIVATED,GO_JUST_DEACTIVATED,GO_NOT_READY,GO_STATE_READY,GAMEOBJECT_DYN_FLAGS,CONFIG_FLOAT_RATE_MINING_AMOUNT,CONFIG_FLOAT_RATE_MINING_NEXT,SKILL_MINING,CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY};
struct Player;struct GameObject;struct Group;struct PlayerbotAI;
struct QuestItem{unsigned index;bool is_looted=false;QuestItem(unsigned i):index(i){}};
using QuestItemList=std::vector<QuestItem>;using QuestItemMap=std::map<unsigned,QuestItemList*>;
struct LootItem{unsigned itemid=2640;bool is_looted=false,is_blocked=false,freeforall=false;unsigned conditionId=0;bool AllowedForPlayer(Player const*,void const*)const;};
struct Loot{std::vector<LootItem>items,m_questItems;QuestItemMap m_playerQuestItems,m_playerFFAItems,m_playerNonQuestNonFFAConditionalItems;unsigned gold=0,unlootedCount=0;
 const auto&GetPlayerQuestItems(){return m_playerQuestItems;}
 void const*GetLootTarget()const{return nullptr;}
 bool HasQuestLootForPlayer(Player const*)const;QuestItemList*FillQuestLoot(Player*);
 LootItem*LootItemInSlot(uint32,uint32,QuestItem** = nullptr,QuestItem** = nullptr,QuestItem** = nullptr);
 uint32 GetMaxSlotInLootFor(uint32)const;
 void NotifyQuestItemRemoved(unsigned){}void NotifyItemRemoved(unsigned){}
 void clear(){for(auto const&p:m_playerQuestItems)delete p.second;m_playerQuestItems.clear();m_questItems.clear();items.clear();gold=unlootedCount=0;}
 ~Loot(){clear();}
 #include "Exhaustion.inc"
};
struct Player{unsigned id=1;bool alive=true,inworld=true,eligible=true;float distance=1;std::set<unsigned>needs{2640};Group*group=nullptr;PlayerbotAI*ai=nullptr;
 bool IsInWorld()const{return inworld;}unsigned GetGUIDLow()const{return id;}bool IsAlive(){return alive;}unsigned GetMapId(){return 0;}
 Group*GetGroup(){return group;}unsigned GetObjectGuid(){return id;}unsigned GetSkillValue(unsigned){return 100;}
 void SendNotifyLootItemRemoved(unsigned){}struct Map*GetMap();
};
bool LootItem::AllowedForPlayer(Player const*p,void const*)const{return p->eligible&&p->needs.count(itemid);}
struct Group{struct Member{unsigned guid;};using MemberSlotList=std::vector<Member>;using member_citerator=MemberSlotList::const_iterator;MemberSlotList members;const auto&GetMemberSlots(){return members;}};
struct PlayerbotAI{bool active=true;bool HasActivePlayerMaster(){return active;}};
PlayerbotAI*GetBotAI(Player*p){return p->ai;}
struct AIHook{void OnUse(Player*){}};
struct GOInfo{struct{unsigned minSuccessOpens=0,maxSuccessOpens=0,eventId=0,lockId=0,consumable=1,chestRestockTime=0;}chest;struct{unsigned minSuccessOpens=0,maxSuccessOpens=0;}fishinghole;};
struct Map{GameObject*go=nullptr;GameObject*GetGameObject(int){return go;}void ScriptsStart(int,unsigned,unsigned,unsigned){}}worldMap;
Map*Player::GetMap(){return &worldMap;}
struct GameObject{Loot loot;GOInfo info;GOInfo*m_goInfo=&info;unsigned type=GAMEOBJECT_TYPE_CHEST,state=GO_ACTIVATED,goState=GO_STATE_READY,uses=0;long m_cooldownTime=0,m_respawnTime=0,m_respawnDelayTime=120;bool m_spawnedByDefault=true,visible=true;unsigned saved=0,despawns=0;
 unsigned GetGoType(){return type;}unsigned getLootState(){return state;}void SetLootState(unsigned v){state=v;}void SetGoState(unsigned v){goState=v;}
 GOInfo const*GetGOInfo(){return &info;}unsigned GetEntry(){return 271;}unsigned GetGUIDLow(){return 13286;}unsigned GetMapId(){return 0;}unsigned GetObjectGuid(){return 1;}
 bool ActivateToQuest(Player*p){return !p->needs.empty();}Map*GetMap(){return &worldMap;}AIHook*AI(){return nullptr;}
 void UseDoorOrButton(){}GameObject*FindNearestGameObject(unsigned,float){return nullptr;}void Despawn(){}void UpdateObjectVisibility(){visible=m_respawnTime==0;}void AddUse(){++uses;}unsigned GetUseCount(){return uses;}
 void ForceValuesUpdateAtIndex(unsigned){}unsigned ComputeRespawnDelay(){return m_respawnDelayTime;}void SaveRespawnTime(){++saved;}void JustDespawnedWaitingRespawn(){++despawns;}void TickDeactivated();
};
struct ObjMgr{std::map<unsigned,Player*>players;bool IsGameObjectForQuests(unsigned){return true;}Player*GetPlayer(unsigned g){return players[g];}}sObjectMgr;
struct Facade{float GetDistance2d(Player*p,GameObject*){return p->distance;}}sServerFacade;
struct Config{float groupMemberLootDistanceWithActiveMaster=25,groupMemberLootDistance=10;}sPlayerbotAIConfig;
struct World{float getConfig(int){return 1;}}sWorld;
struct ScriptMgr{bool OnProcessEvent(unsigned,Player*,GameObject*,bool){return false;}}sScriptMgr;int sEventScripts;
struct LockEntry{int Skill[1]={175};};struct LockStore{LockEntry const*LookupEntry(unsigned){return nullptr;}}sLockStore;
bool roll_chance_f(float){return true;}unsigned urand(unsigned a,unsigned){return a;}
#define DEBUG_LOG(...)
namespace ai{bool CanOpenActivatedQuestChest(Player*,GameObject*);}
using namespace ai;
struct WorldSession{Player*_player;void Release();};
#ifdef BEFORE
#include "Before.inc"
#else
#include "Native.inc"
#endif
void check(bool b,char const*m){if(!b)throw std::runtime_error(m);}
int Run(){
 Player cria,alon,flore;cria.id=1;alon.id=2;flore.id=3;flore.needs={2640,7249};PlayerbotAI a,b,c;cria.ai=&a;alon.ai=&b;flore.ai=&c;Group group;group.members={{1},{2},{3}};for(Player*p:{&cria,&alon,&flore}){p->group=&group;sObjectMgr.players[p->id]=p;}
 GameObject go;worldMap.go=&go;go.loot.m_questItems={{2640},{7249}};
 check(go.loot.HasQuestLootForPlayer(&cria)&&go.loot.unlootedCount==0&&go.loot.m_playerQuestItems.empty(),"query before view has no reservation or count side effects");
 go.loot.FillQuestLoot(&flore);check(go.loot.unlootedCount==2,"native shared quest count");Take(&go.loot,&flore,0);flore.needs.erase(2640);
 check(!CanOpenActivatedQuestChest(&cria,&go),"shared crate empty for Cria must not reopen");
 check(!CanOpenActivatedQuestChest(&alon,&go)&&CanOpenActivatedQuestChest(&flore,&go),"remaining gem belongs only to eligible bot");
 check(GetPersonalQuestChestTurn(&flore,&go)==&flore,"empty Cria and Alon cannot reserve remaining gem turn");
 WorldSession session{&flore};session.Release();check(go.state==GO_ACTIVATED&&go.loot.unlootedCount==1,"another player's remaining item prevents premature despawn");
 Take(&go.loot,&flore,1);session.Release();check(go.state==GO_JUST_DEACTIVATED&&go.loot.isLooted(),"final shared item triggers native deactivation");
 long before=time(nullptr);go.TickDeactivated();check(!go.visible&&go.m_respawnTime>=before+120&&go.despawns==1&&go.saved==1,"native configured respawn scheduled after final loot");
 go.m_respawnTime=0;go.visible=true;go.state=GO_READY;go.loot.m_questItems={{2640}};check(go.loot.HasQuestLootForPlayer(&cria),"fresh generated spawn available without stale suppression");
 check(GetPersonalQuestChestTurn(&alon,&go)==&cria,"READY chest retains original stable order");
 // Actual per-player FFA copies remain available to later eligible players.
 go.state=GO_ACTIVATED;go.loot.m_questItems[0].freeforall=true;go.loot.FillQuestLoot(&cria);go.loot.FillQuestLoot(&alon);check(go.loot.unlootedCount==2,"native FFA per-player count");
 Take(&go.loot,&cria,0);check(!CanOpenActivatedQuestChest(&cria,&go)&&CanOpenActivatedQuestChest(&alon,&go),"FFA take excludes only its own player");
 check(GetPersonalQuestChestTurn(&alon,&go)==&alon,"FFA next player gets turn");Take(&go.loot,&alon,0);session.Release();check(go.state==GO_JUST_DEACTIVATED,"last FFA copy deactivates");
 // Native restocking and no-respawn objects retain their separate lifecycle.
 go.info.chest.consumable=0;go.info.chest.chestRestockTime=45;go.TickDeactivated();check(go.state==GO_NOT_READY&&go.m_cooldownTime>=time(nullptr)+44,"restock timer retained");
 go.info.chest.chestRestockTime=0;go.m_respawnDelayTime=0;go.m_respawnTime=0;unsigned despawns=go.despawns;go.TickDeactivated();check(go.despawns==despawns&&!go.m_respawnTime,"zero-delay objects not forced onto a new timer");
 go.loot.clear();go.loot.m_questItems={{2640}};cria.eligible=false;check(!go.loot.HasQuestLootForPlayer(&cria),"native current eligibility retained");cria.eligible=true;cria.inworld=false;check(!go.loot.HasQuestLootForPlayer(&cria),"offline player rejected");cria.inworld=true;
 go.loot.items.resize(MAX_NR_LOOT_ITEMS);check(!go.loot.HasQuestLootForPlayer(&cria)&&!go.loot.FillQuestLoot(&cria),"native slot capacity agrees");go.loot.items.clear();
 go.loot.FillQuestLoot(&cria);go.loot.m_questItems.push_back(LootItem{7249});cria.needs.insert(7249);Take(&go.loot,&cria,0);check(!go.loot.HasQuestLootForPlayer(&cria),"existing view does not promise unexposed new quest slots");
 check(!go.loot.HasQuestLootForPlayer(nullptr),"null player rejected");
 std::cout<<"PASS shared/FFA quest loot, turn selection, native release, respawn and restock contracts\n";
 return 0;
}
int main(){try{return Run();}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
