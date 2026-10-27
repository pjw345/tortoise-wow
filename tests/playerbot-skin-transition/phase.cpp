#include <cstdint>
#include <iostream>
#include <stdexcept>
using uint32=uint32_t;
enum SkillType{SKILL_NONE=0,SKILL_SKINNING=393};
enum{UNIT_DYNAMIC_FLAGS=10,UNIT_FIELD_FLAGS=11,UNIT_DYNFLAG_LOOTABLE=1,UNIT_FLAG_SKINNABLE=2,ALIVE=0,CORPSE=1};
struct ObjectGuid{uint32 value=0;ObjectGuid()=default;ObjectGuid(uint32 v):value(v){}bool IsEmpty()const{return value==0;}unsigned long GetRawValue()const{return value;}};
struct Player;
struct Creature{
 bool lootable=true,skinnable=true,tapped=true;int death=CORPSE;uint32 level=5;
 struct Loot{bool empty=false;bool isLooted()const{return empty;}}loot;
 struct Info{uint32 GetRequiredLootSkill(){return SKILL_SKINNING;}}info;
 bool HasFlag(int field,int){return field==UNIT_DYNAMIC_FLAGS?lootable:skinnable;}
 bool IsTappedBy(Player*)const{return tapped;}
 uint32 GetLevel()const{return level;}
 Info*GetCreatureInfo(){return &info;}
};
struct PlayerbotAI{Creature*creature=nullptr;bool trained=true;Creature*GetCreature(ObjectGuid){return creature;}bool HasSkill(SkillType){return trained;}};
struct Player{PlayerbotAI*ai;uint32 skill=9;uint32 GetSkillValue(uint32){return skill;}const char*GetName(){return "fixture";}};
PlayerbotAI*GetBotAI(Player*p){return p->ai;}
struct Facade{int GetDeathState(Creature*c){return c->death;}}sServerFacade;
struct Log{template<class...T>void outLoot(T...){}}sLog;
struct LootObject{
 ObjectGuid guid;uint32 skillId=SKILL_NONE,reqSkillValue=0,reqItem=0;
 void Refresh(Player*,ObjectGuid,bool debug=false);
};
#include "native-creature-refresh.inc"
void check(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
int main(){try{
 Creature corpse;PlayerbotAI ai{&corpse};Player player{&ai};LootObject object;
 object.Refresh(&player,ObjectGuid{1});
 check(!object.guid.IsEmpty()&&object.skillId==SKILL_NONE,"non-empty normal body stays first");
 corpse.loot.empty=true;object.Refresh(&player,ObjectGuid{1});
#ifdef MANGOSBOT_ZERO
 check(!object.guid.IsEmpty()&&object.skillId==SKILL_SKINNING,"empty body with lootable flag must classify as skinning");
 player.skill=0;object.Refresh(&player,ObjectGuid{1});check(object.guid.IsEmpty(),"missing skin skill remains ineligible");player.skill=9;
 ai.trained=false;object.Refresh(&player,ObjectGuid{1});check(object.guid.IsEmpty(),"untrained profession remains ineligible");ai.trained=true;
 corpse.skinnable=false;object.Refresh(&player,ObjectGuid{1});check(object.guid.IsEmpty(),"empty unskinnable body stays empty");corpse.skinnable=true;
#else
 check(!object.guid.IsEmpty()&&object.skillId==SKILL_NONE,"other variants retain flag-only body classification");
#endif
 corpse.loot.empty=false;corpse.tapped=false;corpse.skinnable=false;object.Refresh(&player,ObjectGuid{1});check(object.guid.IsEmpty(),"untapped non-skin body stays ineligible");
 corpse.tapped=true;corpse.skinnable=true;corpse.lootable=false;corpse.loot.empty=true;object.Refresh(&player,ObjectGuid{1});check(object.skillId==SKILL_SKINNING&&!object.guid.IsEmpty(),"normal skin-only corpse remains supported");
 corpse.death=ALIVE;object.Refresh(&player,ObjectGuid{1});check(object.guid.IsEmpty(),"living target stays ineligible");
 std::cout<<"PASS native corpse phase\n";
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
