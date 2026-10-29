#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <iostream>
#include "enum.inc"
#include "result-name.inc"
std::map<std::string,std::string> texts;
std::string text(std::string key,std::map<std::string,std::string> args){std::string value=texts.count(key)?texts[key]:key;for(auto const&arg:args){size_t p;while((p=value.find(arg.first))!=std::string::npos)value.replace(p,arg.first.size(),arg.second);}return value;}
#define BOT_TEXT2(k,a) text(k,a)
#include "formatter.inc"
struct Bot{bool trade=false;bool GetTrader(){return trade;}}botStorage,*bot=&botStorage;
int unitTarget,goTarget,requesterValue;void*target=&unitTarget;void*gameObjectTarget=nullptr;void*itemTarget=nullptr;void*requester=&requesterValue;int spell=8613,castCalls=0;
struct AI{SpellCastResult result=SPELL_FAILED_LOW_CASTLEVEL;bool allowed=false;bool write=true;int units=0,objects=0;std::vector<std::string> replies;
 bool CanCastSpell(int,void*,int,bool,void*,bool,bool,bool,SpellCastResult*r){++units;if(write)*r=result;return allowed;}
 bool CanCastSpell(int,void*,int,bool,bool,bool,bool,SpellCastResult*r){++objects;if(write)*r=result;return allowed;}
 void TellPlayerNoFacing(void*,std::string s){replies.push_back(s);}}aiStorage,*ai=&aiStorage;
std::map<std::string,std::string> replyArgs{{"%spell","[Skinning]"}};
#include "guard.inc"
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{std::string mode=argv[1];texts["cast_spell_command_error"]="Failed to cast %spell. Reason: %fail_reason";
 if(mode=="mapping"){check(std::string(GetSpellCastResultString(SPELL_FAILED_LOW_CASTLEVEL))=="SPELL_FAILED_LOW_CASTLEVEL","native symbolic skill mapping");check(std::string(GetSpellCastResultString(SPELL_FAILED_TARGET_NOT_LOOTED))=="SPELL_FAILED_TARGET_NOT_LOOTED","native readiness mapping");}
 else if(mode=="translation"){texts["SPELL_FAILED_LOW_CASTLEVEL"]="Localized skill failure";check(FormatManualCastFailure(SPELL_FAILED_LOW_CASTLEVEL)=="Localized skill failure","existing translation wins");texts["SPELL_FAILED_LOW_CASTLEVEL"]="Localized %spell failure";check(FormatManualCastFailure(SPELL_FAILED_LOW_CASTLEVEL,{{"%spell","[Skinning]"}})=="Localized [Skinning] failure","caller placeholders preserved");texts["SPELL_FAILED_LOW_CASTLEVEL"]="";check(FormatManualCastFailure(SPELL_FAILED_LOW_CASTLEVEL)=="My skill is not high enough.","empty translation falls back");}
 else if(mode=="reasons"){for(auto result:{SPELL_FAILED_LOW_CASTLEVEL,SPELL_FAILED_MIN_SKILL,SPELL_FAILED_TOTEMS,SPELL_FAILED_BAD_TARGETS,SPELL_FAILED_TARGET_NOT_LOOTED,SPELL_FAILED_TARGET_UNSKINNABLE,SPELL_FAILED_OUT_OF_RANGE,SPELL_FAILED_LINE_OF_SIGHT,SPELL_FAILED_NOT_READY,SPELL_FAILED_NO_POWER,SPELL_FAILED_REAGENTS}){auto message=FormatManualCastFailure(result);check(!message.empty()&&message.find("SPELL_")==std::string::npos&&message.find('%')==std::string::npos,"human reason without missing key");}check(FormatManualCastFailure(SPELL_FAILED_TARGET_NOT_LOOTED).find("looted")!=std::string::npos,"readiness not mislabelled skill");check(FormatManualCastFailure(static_cast<SpellCastResult>(254)).find("254")!=std::string::npos,"unknown includes numeric result");}
 else if(mode=="allowed"||mode=="trade"){ai->allowed=mode=="allowed";bot->trade=mode=="trade";check(CheckManualCast()&&castCalls==1&&ai->replies.empty(),"existing successful and trade paths retained");}
 else {gameObjectTarget=mode=="gameobject"?&goTarget:nullptr;ai->result=mode=="gameobject"?SPELL_FAILED_LINE_OF_SIGHT:SPELL_FAILED_LOW_CASTLEVEL;if(mode=="unwritten")ai->write=false;check(!CheckManualCast()&&!castCalls,"rejected cast not started");check(ai->replies.size()==1,"one normal reply");auto message=ai->replies[0];check(message.find("[Skinning]")!=std::string::npos&&message.find('%')==std::string::npos,"spell link and formatted reply");check(message.back()!=' '&&message.find("SPELL_")==std::string::npos,"nonempty readable reason");check((gameObjectTarget?ai->objects:ai->units)==1,"native target overload retained");}
 std::cout<<"PASS "<<mode<<'\n';return 0;}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
