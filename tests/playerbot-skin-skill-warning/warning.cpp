#include <algorithm>
#include <cmath>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using uint32=uint32_t;using uint8=uint8_t;
enum SkillType{SKILL_NONE=0,SKILL_SKINNING=393};
enum{UNIT_DYNAMIC_FLAGS=10,UNIT_FIELD_FLAGS=11,UNIT_DYNFLAG_LOOTABLE=1,UNIT_FLAG_SKINNABLE=2,ALIVE=0,CORPSE=1,INTERACTION_DISTANCE=5};
struct ObjectGuid{uint32 value=0;ObjectGuid()=default;ObjectGuid(uint32 v):value(v){}explicit operator bool()const{return value!=0;}bool IsEmpty()const{return !value;}bool operator<(ObjectGuid rhs)const{return value<rhs.value;}unsigned long GetRawValue()const{return value;}};
struct WorldObject{float z=0;virtual~WorldObject()=default;float GetPositionZ()const{return z;}};
struct Group{};struct PlayerbotAI;struct Player;
struct Creature:WorldObject{
 bool lootable=true,skinnable=true,tapped=true,rights=true,lootForSkin=false;int death=CORPSE;uint32 level=16;
 struct Loot{bool empty=true;bool isLooted()const{return empty;}}loot;
 struct Info{uint32 GetRequiredLootSkill(){return SKILL_SKINNING;}}info;
 bool HasFlag(int field,int){return field==UNIT_DYNAMIC_FLAGS?lootable:skinnable;}
 bool IsTappedBy(Player*)const{return tapped;}
 bool IsSkinnableBy(Player*)const{return skinnable&&rights;}
 uint32 GetLevel()const{return level;}Info*GetCreatureInfo(){return &info;}
};
struct Player:WorldObject{PlayerbotAI*ai=nullptr;Group*group=nullptr;uint32 skill=52;bool knife=true,inWorld=true;
 uint32 GetSkillValue(uint32){return skill;}bool HasItemCount(int,int)const{return knife;}
 Group*GetGroup(){return group;}bool IsInWorld()const{return inWorld;}const char*GetName(){return "fixture";}};
struct ChatHelper{static std::string formatSkill(uint32,Player*){return "[Skinning]";}};
struct PlayerbotSecurityLevel{static constexpr int PLAYERBOT_SECURITY_ALLOW_ALL=0;};
struct PlayerbotAI{
 Player*master=nullptr;Creature*corpse=nullptr;bool trained=true,active=true,real=false,leader=false,silent=false,allowed=true;
 bool HasSkill(SkillType){return trained;}bool HasActivePlayerMaster(){return active;}bool IsRealPlayer(){return real;}
 bool IsGroupLeader(){return leader;}Player*GetMaster(){return master;}Creature*GetCreature(ObjectGuid g){return g?corpse:nullptr;}
 ChatHelper chat;ChatHelper*GetChatHelper(){return &chat;}
 struct Message{Player*recipient;std::string text;bool privateMessage;};std::vector<Message>messages;
 bool TellPlayerNoFacing(Player*p,std::string s,int,bool privacy,bool=true,bool=false){if(silent||!allowed)return false;messages.push_back({p,s,privacy});return true;}
};
PlayerbotAI*GetBotAI(Player*p){return p->ai;}
struct Facade{bool los=true;float distance=2;int GetDeathState(Creature*c){return c->death;}
 bool IsDistanceGreaterThan(float a,float b){return a>b;}float GetDistance2d(WorldObject*,WorldObject*){return distance;}
 bool IsWithinLOSInMap(WorldObject*,WorldObject*){return los;}}sServerFacade;
struct Log{template<class...T>void outLoot(T...){}}sLog;
struct Config{uint32 lootTargetInspectDelay=120;float gatheringDistance=50,groupMemberGatheringDistanceWithActiveMaster=15;}sPlayerbotAIConfig;
bool approachSafe=true;int normalAdmissionCalls=0;
struct LootObject{
 ObjectGuid guid;uint32 skillId=SKILL_NONE,reqSkillValue=0,reqItem=0;
 LootObject()=default;LootObject(Player*p,ObjectGuid g){Refresh(p,g);}
 bool IsEmpty(){return !guid;}
 void Refresh(Player*,ObjectGuid,bool=false);
 WorldObject*GetWorldObject(Player*p){return guid?static_cast<WorldObject*>(p->ai->GetCreature(guid)):nullptr;}
 bool IsApproachSafe(Player*){return approachSafe;}
};
#include "native-phase.inc"
#include "native-list.h"
time_t clockNow=100;time_t fixtureTime(time_t*){return clockNow;}
#define time fixtureTime
#include "native-list.inc"
struct AddGatheringLootAction{
 PlayerbotAI*ai;Player*bot;LootTargetList skinningSkillWarnings;
 bool AddLoot(Player*,ObjectGuid,bool=false);
#ifdef MANGOSBOT_ZERO
 void TellInsufficientSkinningSkill(ObjectGuid,LootObject const&);
#endif
};
#ifdef MANGOSBOT_ZERO
#include "native-warning.inc"
#endif
#include "native-entrance.inc"
#undef time
void check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
struct Fixture{
 Group group,other;Player bot,master;Creature corpse;PlayerbotAI ai;AddGatheringLootAction action{&ai,&bot};
 Fixture(){clockNow=100;approachSafe=true;normalAdmissionCalls=0;sServerFacade={};sPlayerbotAIConfig={};bot.ai=&ai;bot.group=master.group=&group;ai.master=&master;ai.corpse=&corpse;}
 bool attempt(uint32 guid=1){return action.AddLoot(&master,ObjectGuid{guid});}
};
void message(){Fixture f;check(!f.attempt(),"insufficient skinning is still refused");check(normalAdmissionCalls==0,"warning cannot admit loot");
 check(f.ai.messages.size()==1,"low skill should whisper");auto&m=f.ai.messages[0];
 check(m.recipient==&f.master&&m.privateMessage,"warning goes privately to actual human master");
 check(m.text=="Not enough [Skinning] skill to skin. Required skill: 60; my skill: 52.","native skill numbers and no duplicated bot name");}
void repeats(){Fixture f;f.attempt();clockNow=110;f.attempt();check(f.ai.messages.size()==1,"periodic rescan cannot repeat same corpse");
 f.attempt(2);check(f.ai.messages.size()==2,"independent corpse can be reported");clockNow=221;f.attempt();check(f.action.skinningSkillWarnings.size()==2,"expired timestamp entries are pruned");
 clockNow=231;f.attempt();check(f.action.skinningSkillWarnings.size()==1,"native list prunes old corpse without affecting queue");}
void ownership(){for(int n=0;n<6;++n){Fixture f;switch(n){case 0:f.ai.active=false;break;case 1:f.ai.real=true;break;case 2:f.ai.master=nullptr;break;case 3:f.master.inWorld=false;break;case 4:f.bot.group=nullptr;break;case 5:f.master.group=&f.other;break;}
 f.attempt();check(f.ai.messages.empty(),"inactive/unrelated masters must not receive warnings");}}
void readiness(){for(int n=0;n<8;++n){Fixture f;switch(n){case 0:f.ai.trained=false;break;case 1:f.corpse.death=ALIVE;break;case 2:f.corpse.skinnable=false;break;case 3:f.corpse.rights=false;break;case 4:f.corpse.loot.empty=false;break;case 5:f.corpse.lootForSkin=true;break;case 6:f.bot.knife=false;break;case 7:f.ai.corpse=nullptr;break;}
 f.attempt();check(f.ai.messages.empty(),"native training/body/skin/tool/readiness controls retained");}}
void range(){Fixture f;sServerFacade.distance=16;f.attempt();check(f.ai.messages.empty(),"follower warning respects gather range");sServerFacade.distance=15;f.attempt();check(f.ai.messages.size()==1,"native range boundary included");
 Fixture leader;leader.ai.leader=true;sServerFacade.distance=40;leader.attempt();check(leader.ai.messages.size()==1,"native group leader range retained");
 Fixture height;height.corpse.z=6;height.attempt();check(height.ai.messages.empty(),"different floor outside native height denied");
 Fixture los;sServerFacade.los=false;los.attempt();check(los.ai.messages.empty(),"native LOS remains required");}
void safety(){Fixture f;approachSafe=false;f.attempt();check(f.ai.messages.empty()&&f.action.skinningSkillWarnings.empty(),"unsafe candidate is not described as locally ready");approachSafe=true;f.attempt();check(f.ai.messages.size()==1,"safety recovery can report");}
void silent(){Fixture f;f.ai.silent=true;f.attempt();check(f.ai.messages.empty()&&f.action.skinningSkillWarnings.empty(),"silent mode is respected without consuming notice");f.ai.silent=false;f.attempt();check(f.ai.messages.size()==1,"unmuting permits normal private feedback");
 Fixture security;security.ai.allowed=false;security.attempt();check(security.ai.messages.empty(),"native message refusal retained");}
void recovery(){Fixture f;f.attempt();f.bot.skill=60;check(f.attempt()&&normalAdmissionCalls==1,"warning cache cannot defer newly eligible skinning");
 Fixture body;body.corpse.loot.empty=false;check(body.attempt()&&body.ai.messages.empty(),"ordinary loot still comes first");
 Fixture level;level.corpse.level=17;level.attempt();check(level.ai.messages[0].text.find("70")!=std::string::npos,"required points come from actual classifier");}
void legacy(){Fixture f;check(f.attempt()&&normalAdmissionCalls==1&&f.ai.messages.empty(),"other variants retain prior flag-only classification without new notice");}
int main(int argc,char**argv){try{std::string mode=argc>1?argv[1]:"message";if(mode=="message")message();else if(mode=="repeats")repeats();else if(mode=="ownership")ownership();else if(mode=="readiness")readiness();else if(mode=="range")range();else if(mode=="safety")safety();else if(mode=="silent")silent();else if(mode=="recovery")recovery();else legacy();std::cout<<"PASS "<<mode<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
