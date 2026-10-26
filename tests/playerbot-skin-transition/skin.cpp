#include <algorithm>
#include <ctime>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
using uint32=unsigned;using uint8=unsigned char;
constexpr uint32 SKILL_NONE=0,SKILL_SKINNING=393;
struct ObjectGuid {unsigned id=0;bool creature=true;ObjectGuid()=default;ObjectGuid(unsigned i,bool c=true):id(i),creature(c){}explicit operator bool()const{return id!=0;}bool IsCreature()const{return creature;}unsigned long GetRawValue()const{return id;}bool operator<(ObjectGuid b)const{return id<b.id;}bool operator==(ObjectGuid b)const{return id==b.id;}};
struct Player;struct Unit {};
struct WorldObject {float distance=2;virtual~WorldObject()=default;};
struct Creature:WorldObject {
 bool body=true,bodyRights=false,skinFlag=true,tapped=true,lootForSkin=false;unsigned skinningForOthersTimer=5000;
 struct Loot {bool empty=false;bool isLooted()const{return empty;}}loot;
 bool IsTappedBy(Player const*)const{return tapped;}
#include "native-skin-rights.inc"
};
struct PlayerbotAI {bool gather=true;std::map<ObjectGuid,Creature*> corpses;Creature*GetCreature(ObjectGuid g){auto i=corpses.find(g);return i==corpses.end()?nullptr:i->second;}bool HasStrategy(const char*name,int){return std::string(name)=="gather"&&gather;}};
struct Player {PlayerbotAI*ai=nullptr;bool skilled=true,knife=true;float GetDistance(WorldObject*w){return w->distance;}const char*GetName(){return "fixture";}};
PlayerbotAI*GetBotAI(Player*p){return p->ai;}
struct BotState {static constexpr int BOT_STATE_NON_COMBAT=0;};
struct Log {template<class...T>void outLoot(T...){}}sLog;
struct Config {uint32 lootTargetInspectDelay=120;}sPlayerbotAIConfig;
WorldObject gameObject;
struct LootObject {
 ObjectGuid guid;uint32 skillId=SKILL_NONE;Player*owner=nullptr;
 LootObject()=default;
 LootObject(Player*p,ObjectGuid g):guid(g),owner(p){if(g.IsCreature()){auto*c=p->ai->GetCreature(g);if(!c){guid={};return;}if(c->body)skillId=SKILL_NONE;else if(c->skinFlag)skillId=SKILL_SKINNING;else guid={};}}
 bool IsLootPossible(Player*p,bool*suppress=nullptr){if(suppress)*suppress=false;if(!guid)return false;if(!guid.IsCreature())return true;auto*c=p->ai->GetCreature(guid);if(!c)return false;if(skillId==SKILL_NONE){if(!c->bodyRights){if(suppress)*suppress=true;return false;}return true;}return p->skilled&&p->knife&&c->skinFlag;}
 WorldObject*GetWorldObject(Player*p){return guid.IsCreature()?static_cast<WorldObject*>(p->ai->GetCreature(guid)):&gameObject;}
};
#include "native-queue.h"
time_t clockNow=100;
time_t fixtureTime(time_t*){return clockNow;}
#define time fixtureTime
#include "native-queue.inc"
#undef time
void check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
struct Fixture {
 Player player;PlayerbotAI ai;Creature corpse;ObjectGuid guid{1};LootObjectStack queue;
 Fixture():queue(&player){clockNow=100;player.ai=&ai;ai.corpses[guid]=&corpse;}
 void ready(){corpse.body=false;corpse.loot.empty=true;}
 void suppressBody(){check(queue.Add(guid),"native initial corpse queue");check(queue.OrderByDistance().empty(),"native body rights rejection suppresses discovery");}
};
void transition(){
 Fixture f;f.suppressBody();f.ready();++clockNow;
 check(f.queue.Add(f.guid),"body-to-skin transition must not wait for the 120-second body ignore");
 check(f.queue.GetLoot().skillId==SKILL_SKINNING,"existing queue selects eligible skinning phase");
 Fixture own;own.ready();own.queue.Ignore(own.guid,120,true);++clockNow;
 check(own.queue.Add(own.guid),"own successful body storage may proceed to skinning");
}
void controls(){
 Fixture f;f.suppressBody();clockNow=110;check(!f.queue.Add(f.guid),"unchanged body remains ignored");
 f.ready();f.ai.gather=false;check(!f.queue.Add(f.guid),"gather disabled retains suppression");f.ai.gather=true;f.player.knife=false;check(!f.queue.Add(f.guid),"missing tool cannot break suppression");f.player.knife=true;
 f.player.skilled=false;check(!f.queue.Add(f.guid),"insufficient skill cannot break suppression");f.player.skilled=true;
 f.corpse.loot.empty=false;check(!f.queue.Add(f.guid),"body must be completely looted");f.corpse.loot.empty=true;
 f.corpse.tapped=false;check(!f.queue.Add(f.guid),"native outsider skin protection retained");f.corpse.skinningForOthersTimer=0;
 check(f.queue.Add(f.guid),"native outsider release may admit skinning");
 f.queue.Ignore(f.guid,10);clockNow=115;check(!f.queue.Add(f.guid),"real opening/skinning failure retains retry");clockNow=120;check(f.queue.Add(f.guid),"real retry expires normally");
 f.queue.Ignore(f.guid,120);clockNow=121;check(!f.queue.Add(f.guid),"completed skin inspection is not a body transition");
 Fixture skins;skins.suppressBody();skins.ready();skins.corpse.lootForSkin=true;check(!skins.queue.Add(skins.guid),"already generated skin loot cannot restart skinning");
 Fixture gone;gone.suppressBody();gone.ready();gone.ai.corpses.clear();check(!gone.queue.Add(gone.guid),"despawned corpse cannot recover");
 Fixture flags;flags.suppressBody();flags.ready();flags.corpse.skinFlag=false;check(!flags.queue.Add(flags.guid),"missing skin flag stays ignored");
 Fixture expiry;expiry.suppressBody();clockNow=220;check(expiry.queue.Add(expiry.guid),"normal body inspection duration still expires");
 Fixture clear;clear.suppressBody();clear.queue.Clear();check(clear.queue.Add(clear.guid),"clear removes ignored body metadata");clear.queue.Ignore(clear.guid,10);clear.ready();check(!clear.queue.Add(clear.guid),"new failed-skin retry cannot inherit stale body marker");
 Fixture objects;ObjectGuid go{2,false};objects.queue.Ignore(go,120,true);clockNow=101;check(!objects.queue.Add(go),"GO ignores never recover as corpses");
}
void legacy(){Fixture f;f.suppressBody();f.ready();clockNow=101;check(!f.queue.Add(f.guid),"non-ZERO keeps original ignore behavior");clockNow=220;check(f.queue.Add(f.guid),"non-ZERO timeout unchanged");}
int main(int argc,char**argv){try{std::string mode=argc>1?argv[1]:"";if(mode=="transition")transition();else if(mode=="controls")controls();else legacy();std::cout<<"PASS "<<mode<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
