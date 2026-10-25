#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using uint32=uint32_t;
using uint64=uint64_t;
constexpr int UNIT_NPC_FLAG_NONE=0,AURA_INTERRUPT_FLAG_TALK=1,CREATURE_FLAG_EXTRA_NO_MOVEMENT_PAUSE=2,EMOTE_STATE_DANCE=3;
constexpr int QUEST_STATUS_NONE=0,QUEST_STATUS_INCOMPLETE=3,QUEST_STATUS_COMPLETE=1;
constexpr int GOSSIP_ICON_TALK=0,GOSSIP_SENDER_MAIN=1,GOSSIP_ACTION_INFO_DEF=1000,MAX_GOSSIP_TEXT_OPTIONS=1;
#define DEBUG_LOG(...) ((void)0)
void Check(bool condition,char const* message){if(!condition)throw std::runtime_error(message);}
struct ObjectGuid
{
    uint64 value=0;
    explicit operator bool()const{return value!=0;}
    bool operator==(ObjectGuid rhs)const{return value==rhs.value;}
    bool operator!=(ObjectGuid rhs)const{return !(*this==rhs);}
    bool IsAnyTypeCreature()const{return true;}
    bool IsGameObject()const{return false;}
    std::string GetString()const{return std::to_string(value);}
};
struct WorldPacket
{
    std::vector<uint64> values;size_t position=0;
    bool empty()const{return values.empty();}
    void rpos(size_t p){position=p;}
    WorldPacket& operator<<(ObjectGuid g){values.push_back(g.value);return *this;}
    WorldPacket& operator<<(uint32 v){values.push_back(v);return *this;}
    WorldPacket& operator<<(std::string const&){return *this;}
    WorldPacket& operator>>(ObjectGuid& g){g.value=values.at(position++);return *this;}
    WorldPacket& operator>>(uint32& v){v=values.at(position++);return *this;}
    WorldPacket& operator>>(std::string& v){v.clear();return *this;}
};
struct Player;
struct GameObject{};
struct CharmInfo{bool IsFollowing()const{return false;}};
struct CreatureInfo{uint32 GossipMenuId=41162,entry=60858;};
struct Creature
{
    ObjectGuid guid{77};CreatureInfo info;bool scripted=true,interactable=true;
    int pauses=0,facings=0;
    CreatureInfo const* GetCreatureInfo()const{return &info;}
    uint32 GetDefaultGossipMenuId()const{return info.GossipMenuId;}
    uint32 GetEntry()const{return info.entry;}
    ObjectGuid GetObjectGuid()const{return guid;}
    ObjectGuid GetGUID()const{return guid;}
    std::string GetName()const{return "Test NPC";}
    bool IsQuestGiver()const{return true;}
    void SetFacingToObject(Player*){++facings;}
    bool HasExtraFlag(int)const{return false;}
    void PauseOutOfCombatMovement(){++pauses;}
    bool IsSpiritGuide()const{return false;}
    void SendAreaSpiritHealerQueryOpcode(Player*){}
    void HandleEmote(int){}
    CharmInfo* GetCharmInfo(){return nullptr;}
};
struct GossipMenuItem{std::string m_gMessage;uint32 sender,action;bool coded=false;};
struct GossipMenu
{
    uint32 id=0;std::vector<GossipMenuItem> items;
    unsigned int MenuItemCount()const{return items.size();}
    GossipMenuItem const& GetItem(unsigned int n){return items.at(n);}
    uint32 GetMenuId(){return id;}
};
struct PlayerMenu
{
    GossipMenu menu;
    GossipMenu& GetGossipMenu(){return menu;}
    void ClearMenus(){menu.items.clear();}
    bool GossipOptionCoded(unsigned int n){return n<menu.items.size()&&menu.items[n].coded;}
    uint32 GossipOptionSender(unsigned int n){return n<menu.items.size()?menu.items[n].sender:0;}
    uint32 GossipOptionAction(unsigned int n){return n<menu.items.size()?menu.items[n].action:0;}
};
struct WorldSession
{
    Player* _player;ObjectGuid m_currentGossipGUID;int hello=0,select=0;
    explicit WorldSession(Player* p):_player(p){}
    Player* GetPlayer(){return _player;}
    ObjectGuid GetCurrentGossipGUID()const{return m_currentGossipGUID;}
    void HandleGossipHelloOpcode(WorldPacket&);
    void HandleGossipSelectOptionOpcode(WorldPacket&);
};
struct Player
{
    Creature* npc=nullptr;ObjectGuid selection;PlayerMenu menus;PlayerMenu* PlayerTalkClass=&menus;
    WorldSession session{this};int quest=QUEST_STATUS_INCOMPLETE,letters=1,credit=0,saves=0,talked=0,dbSelections=0,auras=0;
    bool emptyDatabaseMenu=false;
    ObjectGuid GetSelectionGuid(){return selection;}
    Creature* GetNPCIfCanInteractWith(ObjectGuid guid,int){return npc&&npc->interactable&&npc->guid==guid?npc:nullptr;}
    WorldSession* GetSession(){return &session;}
    PlayerMenu* GetPlayerMenu(){return PlayerTalkClass;}
    void RemoveAurasWithInterruptFlags(int){++auras;}
    void TalkedToCreature(uint32,ObjectGuid){++talked;}
    int GetQuestStatus(uint32){return quest;}
    bool HasItemCount(uint32,int count,bool){return letters>=count;}
    void PrepareQuestMenu(ObjectGuid){}
    void ADD_GOSSIP_ITEM(int,int text,uint32 sender,uint32 action){menus.menu.items.push_back({std::to_string(text),sender,action});}
    void SEND_GOSSIP_MENU(uint32 id,ObjectGuid){menus.menu.id=id;}
    void DestroyItemCount(uint32,int count,bool){letters-=count;}
    void SaveInventoryAndGoldToDB(){++saves;}
    void KilledMonster(CreatureInfo const* info,ObjectGuid){Check(info->entry==60381,"wrong quest credit");++credit;quest=QUEST_STATUS_COMPLETE;}
    uint32 GetGossipTextId(uint32 id,Creature*){return id;}
    void PrepareGossipMenu(Creature* npc,uint32 id){menus.ClearMenus();menus.menu.id=id;if(!emptyDatabaseMenu)menus.menu.items.push_back({"Native DB option",1,10});}
    void SendPreparedGossip(Creature*){}
    GameObject* GetGameObjectIfCanInteractWith(ObjectGuid){return nullptr;}
    void OnGossipSelect(Creature*,uint32){++dbSelections;}
    void OnGossipSelect(GameObject*,uint32){}
};
struct GossipText{struct Option{std::string Text_0,Text_1;};Option Options[1]={{"Native text",""}};};
using GossipMenuItemsMapBounds=std::pair<int,int>;
struct ObjectMgr
{
    bool hasDatabaseOptions=false;CreatureInfo dummy{0,60381};GossipText text;
    GossipMenuItemsMapBounds GetGossipMenuItemsMapBounds(uint32){return {0,hasDatabaseOptions?1:0};}
    GossipText const* GetGossipText(uint32){return &text;}
    CreatureInfo const* GetCreatureTemplate(uint32 id){return id==60381?&dummy:nullptr;}
}sObjectMgr;
struct World{bool IsAprilFools(){return false;}}sWorld;
int randomCalls=0;
uint32 urand(uint32 low,uint32 high){Check(high>=low,"underflowed random bound");++randomCalls;return low;}
bool GossipHello_npc_maltimor_gartside(Player*,Creature*);
bool GossipSelect_npc_maltimor_gartside(Player*,Creature*,uint32,uint32);
struct ScriptMgr
{
    bool OnGossipHello(Player* player,Creature* npc)
    {
        ++player->session.hello;
        if(!npc->scripted)return false;
        player->menus.ClearMenus();
        return GossipHello_npc_maltimor_gartside(player,npc);
    }
    bool OnGossipSelect(Player* player,Creature* npc,uint32 sender,uint32 action,char const*)
    {
        ++player->session.select;
        if(!npc->scripted)return false;
        player->menus.ClearMenus();
        return GossipSelect_npc_maltimor_gartside(player,npc,sender,action);
    }
    bool OnGossipSelect(Player*,GameObject*,uint32,uint32,char const*){return false;}
}sScriptMgr;
struct DevAIMgr{bool OnGossipHello(Player* p,Creature* c){return sScriptMgr.OnGossipHello(p,c);}}sScriptDevAIMgr;
struct Facade{void SetFacingTo(Player*,Creature*){}}sServerFacade;
struct PlayerbotAI
{
    Player* owner;Player* master;std::vector<std::string> messages;
    void TellPlayerNoFacing(Player*,std::string const& text){messages.push_back(text);}
    void TellError(Player*,std::string const& text){messages.push_back(text);}
};
struct Event
{
    std::string source="talk",param;Player* owner=nullptr;WorldPacket packet;
    Player* getOwner(){return owner;}
    WorldPacket& getPacket(){return packet;}
    std::string getParam(){return param;}
    std::string getSource(){return source;}
};
namespace ai
{
struct ChatCommandAction
{
    PlayerbotAI* ai;Player* bot;
    ChatCommandAction(PlayerbotAI* p,char const*):ai(p),bot(p->owner){}
    virtual ~ChatCommandAction()=default;
    virtual bool Execute(Event&)=0;
    Player* GetMaster(){return ai->master;}
};
}
#include "NativeGossipActionClass.inc"
#include "NativeMaltimor.inc"
#include "NativeGossipHandlers.inc"
using namespace ai;
#include "NativeGossipAction.inc"

struct Fixture
{
    Creature npc;Player bot,master;PlayerbotAI ai{&bot,&master,{}};GossipHelloAction action{&ai};
    Fixture(){bot.npc=&npc;master.selection=npc.guid;sObjectMgr.hasDatabaseOptions=false;}
    bool Talk(std::string text="",std::string source="talk",bool packet=false)
    {Event event;event.owner=&master;event.param=text;event.source=source;if(packet)event.packet<<npc.guid;return action.Execute(event);}
};
int main()
{
    try
    {
        for(int i=0;i<4;++i)
        {
            Fixture f;
            Check(f.Talk(),"script-only Talk must open native hello");
            Check(f.bot.session.GetCurrentGossipGUID()==f.npc.guid,"native gossip identity not set");
            Check(f.bot.menus.menu.MenuItemCount()==1,"script did not build its menu");
            Check(f.bot.letters==1&&f.bot.credit==0,"greeting completed quest early");
            for(int step=1;step<=4;++step)
            {
                Check(f.Talk("1"),"native dialogue selection failed");
                Check(f.bot.credit==(step==4?1:0),"dialogue credit timing differs");
                Check(f.bot.letters==(step==4?0:1),"letter consumed before final step");
            }
            Check(f.bot.saves==1&&f.bot.quest==QUEST_STATUS_COMPLETE,"native quest completion did not run");
            int select=f.bot.session.select;
            Check(!f.Talk("1"),"empty final menu accepted a repeat");
            Check(f.bot.credit==1&&f.bot.session.select==select,"final reward was repeated");
        }
        {Fixture f;Check(!f.Talk("","rpg action"),"idle RPG admitted script-only dialogue");Check(f.bot.session.hello==0&&f.bot.letters==1,"idle RPG progressed quest");}
        {Fixture f;f.bot.letters=0;Check(f.Talk(),"missing-letter greeting failed");Check(!f.Talk("1"),"missing letter allowed selection");Check(f.bot.credit==0,"missing letter got credit");}
        {Fixture f;f.bot.quest=QUEST_STATUS_NONE;Check(f.Talk(),"unaccepted greeting failed");Check(!f.Talk("1")&&f.bot.credit==0,"unaccepted quest got credit");}
        {Fixture f;f.npc.interactable=false;Check(!f.Talk()&&f.bot.session.hello==0,"out-of-range NPC admitted");}
        {Fixture f;f.master.selection={};Check(!f.Talk(),"missing target admitted");Check(f.Talk("","talk",true),"native event packet target failed");}
        {Fixture f;Check(f.Talk(),"open for invalid option");Check(!f.Talk("99")&&!f.Talk("-2"),"invalid index accepted");Check(f.bot.session.select==0&&f.bot.credit==0,"invalid index dispatched");}
        {Fixture f;Check(f.Talk(),"open for wrong target");f.bot.session.m_currentGossipGUID={999};Check(!f.Talk("1")&&f.bot.session.select==0,"another NPC's menu was reused");}
        {Fixture f;f.npc.scripted=false;sObjectMgr.hasDatabaseOptions=true;Check(f.Talk()&&f.Talk("1"),"database menu fallback broken");Check(f.bot.dbSelections==1,"native database fallback did not run");}
        {Fixture f;f.npc.scripted=false;sObjectMgr.hasDatabaseOptions=true;Check(f.Talk("","rpg action"),"existing DB-backed idle gossip broken");Check(f.bot.dbSelections==1,"idle native selection missing");}
        {Fixture f;f.npc.scripted=false;f.bot.emptyDatabaseMenu=true;sObjectMgr.hasDatabaseOptions=true;int before=randomCalls;Check(!f.Talk("","rpg action"),"empty idle menu admitted");Check(before==randomCalls,"empty menu entered random selection");}
        std::cout<<"PASS: four independent native Maltimor conversations, letter prerequisites/consumption/credit, native session handlers, database fallback, idle admission, empty/invalid/stale menus, packet targets and interaction checks\n";
    }
    catch(std::exception const& error){std::cerr<<error.what()<<'\n';return 1;}
}
