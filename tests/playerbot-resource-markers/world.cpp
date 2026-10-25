#include "ResourceMarkers.h"
#include <list>
#include <map>
#include <cmath>
#include <stdexcept>
#include <iostream>
using uint32 = uint32_t;
constexpr int HIGHGUID_PLAYER=1, GO_READY=1, GAMEOBJECT_TYPE_CHEST=3;
constexpr int LOCK_KEY_SKILL=1, SKILL_MINING=186, SKILL_HERBALISM=182;
enum SkillType { DummySkill=0 };
using LockType=int;
int SkillByLockType(int i){return i;}
struct ObjectGuid {
 uint64_t value;
 ObjectGuid(uint32, uint32 low):value(low){} ObjectGuid(uint64_t raw):value(raw){}
 uint64_t GetRawValue()const{return value;}
};
struct Group{};
struct Info {uint32 lock=1;uint32 GetLockId()const{return lock;}};
struct LockEntry { int Type[8]={LOCK_KEY_SKILL}; int Index[8]={SKILL_MINING}; uint32 Skill[8]={1}; } lock;
struct Store { LockEntry* LookupEntry(uint32 i){return i?&lock:nullptr;} } sLockStore;
struct Player;
struct GameObject {
 Info info; bool spawned=true,inUse=false,visible=true,world=true; int state=GO_READY,type=GAMEOBJECT_TYPE_CHEST;
 float x=50,y=50; uint32 entry=1731; uint64_t guid=17370383791809394007ULL;
 Info* GetGOInfo(){return &info;} bool isSpawned(){return spawned;} bool IsInUse(){return inUse;}
 int getLootState(){return state;} int GetGoType(){return type;}
 bool IsVisibleForInState(Player*,Player*,bool){return visible&&world;}
 float GetPositionX(){return x;}float GetPositionY(){return y;}
 uint32 GetEntry(){return entry;}ObjectGuid GetObjectGuid(){return ObjectGuid(guid);}
};
struct Player {
 std::string name="Alon";uint32 id=1,zone=38,map=0; Group* group=nullptr; bool world=true,alive=true,teleport=false,real=true;
 float x=50,y=50; std::map<uint32,uint32> skills{{SKILL_MINING,100}}; std::vector<std::string> packets;
 bool IsInWorld(){return world;}bool IsAlive(){return alive;}bool IsBeingTeleported(){return teleport;}
 bool IsInMap(Player* p){return map==p->map;}uint32 GetZoneId(){return zone;}
 float GetPositionX(){return x;}float GetPositionY(){return y;}
 float GetDistance(GameObject* go){return std::hypot(go->x-x,go->y-y);}
 bool IsWithinDistInMap(GameObject* go,float r){return GetDistance(go)<=r;}
 bool HasSkill(uint32 skill){return skills[skill]>0;}uint32 GetSkillValue(uint32 skill){return skills[skill];}Group* GetGroup(){return group;}
 const char* GetName(){return name.c_str();}void SendAddonMessage(std::string prefix,std::string message){if(prefix!="RESBOT")throw std::runtime_error("prefix");packets.push_back(message);}
};
bool IsRealPlayer(Player* p){return p->real;}
struct PlayerbotAI {
 Player* bot;Player* master;time_t resourceMarkersUntil=0;
 Player* GetBot(){return bot;}Player* GetMaster(){return master;}
 bool HasSkill(SkillType skill){return bot->GetSkillValue(uint32(skill))>0;}
};
struct Manager {Player* online=nullptr;Player* GetPlayer(ObjectGuid guid){return online&&online->id==guid.value?online:nullptr;}} sObjectMgr;
struct Config {float grindDistance=60,reactDistance=70;} sPlayerbotAIConfig;
bool zoneWorks=true;
bool Map2ZoneCoordinates(float&,float&,uint32){return zoneWorks;}
std::list<GameObject*> nodes;
int scans=0;
namespace MaNGOS {
 struct AllGameObjectsInRange {Player* p;float range;AllGameObjectsInRange(Player* b,float r):p(b),range(r){}bool operator()(GameObject*go){return p->IsWithinDistInMap(go,range);}};
 template<class Check>struct GameObjectListSearcher {std::list<GameObject*>& out;Check& check;GameObjectListSearcher(std::list<GameObject*>&o,Check&c):out(o),check(c){}};
}
namespace Cell {template<class Search>void VisitAllObjects(Player*,Search&s,float){++scans;for(auto go:nodes)if(s.check(go))s.out.push_back(go);}}
constexpr uint32 CHAT_MSG_GUILD=4,CHAT_MSG_PARTY=2,LANG_ADDON=0xffffffff;
struct GateBase {virtual bool CanUseGroupChat(Player*,uint32,uint32,std::string&){return true;}};
#include "native-resource.inc"
void check(bool condition,const char* msg){if(!condition)throw std::runtime_error(msg);}
int main(){
 ChatGate gate;std::string self="RESSELF\tR:1:1:both",normal="ordinary guild chat";
 check(!gate.CanUseGroupChat(nullptr,CHAT_MSG_GUILD,LANG_ADDON,self),"self request never reaches guild broadcast");
 check(gate.CanUseGroupChat(nullptr,CHAT_MSG_PARTY,LANG_ADDON,self),"other channel remains native");
 check(gate.CanUseGroupChat(nullptr,CHAT_MSG_GUILD,0,self),"ordinary chat language remains native");
 check(gate.CanUseGroupChat(nullptr,CHAT_MSG_GUILD,LANG_ADDON,normal),"other addon messages remain native");
 Group group,other;Player bot,master;bot.real=false;bot.id=2;bot.group=master.group=&group;
 PlayerbotAI ai{&bot,&master};sObjectMgr.online=&master;GameObject node;nodes={&node};
 auto send=[&]{master.packets.clear();return ai::resources::SendQueuedSnapshot(&ai,"1:R:1:1:both");};
 check(send()&&master.packets.size()==1&&master.packets[0].find(",1731,O,50.000,50.000")!=std::string::npos,"eligible ore reported");
 auto empty=[&](const char*msg){check(send()&&master.packets.size()==1&&master.packets[0]=="S:1:1:Alon:38:1:1:",msg);};
 bot.skills[SKILL_MINING]=0;empty("missing skill");bot.skills[SKILL_MINING]=1;
 check(send()&&master.packets[0].find(",1731,O,")!=std::string::npos,"native skill one allowed");
 lock.Skill[0]=2;empty("insufficient skill");lock.Skill[0]=1;
 node.spawned=false;empty("depleted");node.spawned=true;
 node.inUse=true;empty("in use");node.inUse=false;
 node.state=0;empty("loot state");node.state=GO_READY;
 node.type=1;empty("not a gathering chest");node.type=GAMEOBJECT_TYPE_CHEST;
 node.visible=false;empty("native visibility");node.visible=true;
 node.x=111;empty("discovery range");node.x=50;
 master.x=-11;empty("master range");master.x=50;
 bot.map=1;empty("different map");bot.map=0;
 bot.alive=false;empty("dead bot");bot.alive=true;
 master.teleport=true;empty("teleport");master.teleport=false;
 zoneWorks=false;empty("unsupported zone");zoneWorks=true;
 lock.Index[0]=SKILL_HERBALISM;bot.skills[SKILL_HERBALISM]=10;
 check(send()&&master.packets[0].find(",1731,H,")!=std::string::npos,"herbs supported");
 check(ai::resources::ResourceKind(&bot,&node,"ore")==0,"ore filter");lock.Index[0]=SKILL_MINING;
 int oldScans=scans;master.group=&other;check(!send()&&scans==oldScans,"queued group revalidation");master.group=&group;
 sObjectMgr.online=nullptr;check(!send()&&scans==oldScans,"logged out requester");sObjectMgr.online=&master;
 GameObject many[40];nodes.clear();for(int i=0;i<40;++i){many[i].x=50+i;many[i].guid=node.guid+i;nodes.push_back(&many[i]);}
 check(send()&&master.packets.size()==16,"nearest 32 nodes bounded");
 check(master.packets.back().find(std::to_string(node.guid+31))!=std::string::npos,"nearest nodes retained");
 check(master.packets.back().find(std::to_string(node.guid+32))==std::string::npos,"excess node omitted");
 // The human's snapshot is independent of bot professions and group state.
 nodes={&node};master.name="Alin";master.group=nullptr;master.packets.clear();
 bot.skills.clear();master.skills={{SKILL_HERBALISM,10}};lock.Index[0]=SKILL_HERBALISM;
 check(ai::resources::SendPlayerSnapshot(&master,"R:1:7:both") && master.packets[0].find("S:1:7:Alin:38:")==0 && master.packets[0].find(",1731,H,")!=std::string::npos,"solo human herbalism");
 master.packets.clear();master.skills.clear();
 check(ai::resources::SendPlayerSnapshot(&master,"R:1:8:both") && master.packets[0]=="S:1:8:Alin:38:1:1:","human without profession reports no resource");
 check(!ai::resources::SendPlayerSnapshot(&bot,"R:1:9:both"),"machine cannot claim human feed");
 std::cout<<"PASS: source-extracted owner authorization, native skill/availability/visibility/range, lifecycle and bounded selection\n";
}
