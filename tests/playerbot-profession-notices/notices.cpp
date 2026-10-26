#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using uint16=uint16_t;using uint32=uint32_t;using int32=int32_t;
#include "native-enums.inc"
#define DEBUG_LOG(...) do{}while(0)
#define PLAYER_SKILL_VALUE_INDEX(pos) (pos)
#define SKILL_VALUE(data) ((data)&0xffffu)
#define SKILL_MAX(data) (((data)>>16)&0xffffu)
#define MAKE_SKILL_VALUE(value,max) (((max)<<16)|(value))
enum{SKILL_NEW,SKILL_CHANGED,SKILL_DELETED};
struct SkillEntry{int categoryId;std::string name;};
struct SkillStore{std::map<uint32,SkillEntry> entries;SkillEntry const*LookupEntry(uint32 id){auto i=entries.find(id);return i==entries.end()?nullptr:&i->second;}}sSkillLineStore;
using SkillLineEntry=SkillEntry;
#include "native-professions.inc"
struct Player;struct PlayerbotAI;struct Group{};
#include "native-hook.inc"
int dispatches=0,elunaCalls=0;
#include "native-enable-hooks.inc"
template<class TScript>struct ScriptRegistry{
 inline static std::vector<std::vector<TScript*>> EnabledHooks;
#include "native-dispatch.inc"
};
struct Observer:PlayerScript{
 void OnProfessionSkillGain(Player*,uint16,uint32,uint32,uint32) override {++dispatches;}
}observer;
struct SkillStatus{uint32 pos;int uState;};using SkillStatusMap=std::map<uint32,SkillStatus>;
struct Eluna{void OnSkillChange(Player*,uint32,uint32){++elunaCalls;}}eluna;
int32 rollValue=1;int32 irand(int,int){return rollValue;}
struct Player{
 std::string name;PlayerbotAI*ai=nullptr;Group*group=nullptr;bool inWorld=true;
 SkillStatusMap mSkillStatus;std::map<uint32,uint32>slots;
 explicit Player(std::string n):name(std::move(n)){}
 bool UpdateSkillPro(uint16,int32,uint32);
 uint32 GetUInt32Value(uint32 i){return slots[i];}void SetUInt32Value(uint32 i,uint32 v){slots[i]=v;}
 Eluna*GetEluna(){return &eluna;}Group*GetGroup(){return group;}bool IsInWorld(){return inWorld;}const char*GetName(){return name.c_str();}
 void learn(uint32 id,uint32 v=28,uint32 max=75){mSkillStatus[id]={id,SKILL_NEW};slots[id]=MAKE_SKILL_VALUE(v,max);}
 uint32 skill(uint32 id){return SKILL_VALUE(slots[id]);}
};
enum class PlayerbotSecurityLevel{PLAYERBOT_SECURITY_ALLOW_ALL};
struct ChatHelper{std::string formatSkill(uint32 id,Player*){auto*entry=sSkillLineStore.LookupEntry(id);return entry?entry->name:"unknown skill";}};
struct Message{Player*recipient;std::string text;bool privateMessage,noRepeat;};
std::vector<Message>messages;
struct PlayerbotAI{
 Player*master=nullptr;bool realPlayer=false,active=true,silent=false;ChatHelper chat;
 bool IsRealPlayer(){return realPlayer;}bool HasActivePlayerMaster(){return active;}Player*GetMaster(){return master;}ChatHelper*GetChatHelper(){return &chat;}
 bool TellPlayerNoFacing(Player*p,std::string text,PlayerbotSecurityLevel,bool privacy,bool repeat){if(silent)return false;messages.push_back({p,text,privacy,repeat});return true;}
};
PlayerbotAI*GetBotAI(Player*p){return p?p->ai:nullptr;}
struct BotScript:PlayerScript{
#include "native-bot-hook.inc"
}botScript;
#include "native-update.inc"
void check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
struct Fixture{
 Group group,otherGroup;Player master{"Alin"},bot{"Mercy"};PlayerbotAI ai;
 Fixture(){ScriptRegistry<PlayerScript>::EnabledHooks.assign(PLAYERHOOK_END,{});messages.clear();dispatches=elunaCalls=0;rollValue=1;ai.master=&master;bot.ai=&ai;master.group=bot.group=&group;
 std::vector<uint16> enabledHooks;
 TortoiseEnableAllHooksIfEmpty(enabledHooks,PLAYERHOOK_END);
 for(auto hook:enabledHooks){
  ScriptRegistry<PlayerScript>::EnabledHooks[hook].push_back(&observer);
#ifdef TEST_MODULE
  ScriptRegistry<PlayerScript>::EnabledHooks[hook].push_back(&botScript);
#endif
 }
 sSkillLineStore.entries.clear();
 for(uint32 id:{164u,165u,171u,182u,186u,197u,202u,333u,393u,755u,1234u})sSkillLineStore.entries[id]={int(SKILL_CATEGORY_PROFESSION),id==393?"Skinning":"Profession"+std::to_string(id)};
 for(uint32 id:{SKILL_COOKING,SKILL_FIRST_AID,SKILL_FISHING,SKILL_SURVIVAL2})sSkillLineStore.entries[id]={9,"Secondary"+std::to_string(id)};
 }
 void messageCount(unsigned count){
#ifdef TEST_MODULE
 check(messages.size()==count,"expected profession message count");
#else
 check(messages.empty(),"module absent must not send a bot message");
#endif
 }
};
void primary(){for(uint32 id:{164u,165u,171u,182u,186u,197u,202u,333u,393u,755u,1234u}){Fixture f;f.bot.learn(id);check(f.bot.UpdateSkillPro(id,1000,1),"native update result");check(f.bot.skill(id)==29,"native stored gain");check(dispatches==1,"exactly one hook dispatch");f.messageCount(1);
#ifdef TEST_MODULE
 check(messages[0].recipient==&f.master,"message goes to actual master");check(messages[0].privateMessage&&!messages[0].noRepeat,"private event notice without repeat suppression");check(messages[0].text.find("29/75")!=std::string::npos,"reports actual new value/max");
#endif
 }}
void secondary(){for(uint32 id:{SKILL_COOKING,SKILL_FIRST_AID,SKILL_FISHING,SKILL_SURVIVAL2}){Fixture f;f.bot.learn(id);f.bot.UpdateSkillPro(id,1000,2);check(f.bot.skill(id)==30,"secondary gain preserved");f.messageCount(1);}}
void noGain(){Fixture f;f.bot.learn(393);check(!f.bot.UpdateSkillPro(393,0,1),"zero chance returns false");check(!f.bot.UpdateSkillPro(0,1000,1),"zero skill returns false");check(!f.bot.UpdateSkillPro(333,1000,1),"missing skill returns false");f.bot.mSkillStatus[393].uState=SKILL_DELETED;check(!f.bot.UpdateSkillPro(393,1000,1),"deleted skill returns false");f.bot.mSkillStatus[393].uState=SKILL_NEW;rollValue=1000;check(f.bot.UpdateSkillPro(393,1,1),"native failed roll still returns true");check(f.bot.skill(393)==28&&dispatches==0,"failed roll sends no gain");rollValue=1;f.bot.UpdateSkillPro(393,1000,0);check(dispatches==0,"zero-step success sends no gain");f.messageCount(0);}
void recipients(){
 for(int mode=0;mode<7;++mode){Fixture f;f.bot.learn(393);
 switch(mode){case 0:f.bot.ai=nullptr;break;case 1:f.ai.realPlayer=true;break;case 2:f.ai.active=false;break;case 3:f.ai.master=nullptr;break;case 4:f.master.inWorld=false;break;case 5:f.bot.group=nullptr;break;case 6:f.master.group=&f.otherGroup;break;}
 f.bot.UpdateSkillPro(393,1000,1);check(messages.empty(),"ineligible recipient must not get a message");check(f.bot.skill(393)==29,"recipient gating must not alter gain");
 }
 Fixture quiet;quiet.bot.learn(393);quiet.ai.silent=true;quiet.bot.UpdateSkillPro(393,1000,1);check(messages.empty(),"normal silent policy respected");
 Fixture player;player.master.learn(393);player.master.UpdateSkillPro(393,1000,1);check(messages.empty(),"human gets no duplicate bot message");
}
void cap(){Fixture f;f.bot.learn(393,74);f.bot.UpdateSkillPro(393,1000,10);check(f.bot.skill(393)==75,"native clamp retained");f.messageCount(1);check(!f.bot.UpdateSkillPro(393,1000,1),"cap returns false");f.messageCount(1);
#ifdef ENABLE_ELUNA
 check(elunaCalls==1,"native Eluna callback preserved exactly once for positive roll");
#else
 check(elunaCalls==0,"Eluna disabled remains supported");
#endif
 Fixture changed;changed.bot.learn(393);changed.bot.mSkillStatus[393].uState=SKILL_CHANGED;changed.bot.UpdateSkillPro(393,1000,1);check(changed.bot.mSkillStatus[393].uState==SKILL_CHANGED,"persistence dirty state retained");}
void nonProfession(){for(uint32 id:{43u,95u,98u,162u,633u,762u,999u}){Fixture f;sSkillLineStore.entries[id]={7,"Other skill"};f.bot.learn(id);f.bot.UpdateSkillPro(id,1000,1);check(f.bot.skill(id)==29,"other native skills still rise");check(dispatches==0&&messages.empty(),"weapon/defence/language/riding/unknown excluded");}}
int main(int argc,char**argv){try{std::string mode=argc>1?argv[1]:"";if(mode=="primary")primary();else if(mode=="secondary")secondary();else if(mode=="no_gain")noGain();else if(mode=="recipients")recipients();else if(mode=="cap")cap();else nonProfession();std::cout<<"PASS "<<mode<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
