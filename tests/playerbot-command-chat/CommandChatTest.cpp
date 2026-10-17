#include <string>
#include <vector>
#include <map>
#include <stdexcept>
#include <iostream>
struct Player{};
bool IsRealPlayer(Player*p){return p!=nullptr;}
struct WorldPacket{};
struct ChatHelper {
 static std::string parseValue(const char*,std::string s){const std::string p="[command:";return s.find(p)==0?s.substr(p.size(),s.size()-p.size()-1):"";}
 static bool parseable(std::string s){return s.find("Hitem:")!=std::string::npos;}
};
struct Trigger{int events=0;void ExternalEvent(std::string,Player*){++events;}void ExternalEventForce(std::string,Player*){++events;}};
struct AiObjectContext{std::map<std::string,Trigger> triggers;Trigger*GetTrigger(std::string s){auto i=triggers.find(s);return i==triggers.end()?nullptr:&i->second;}};
struct ExternalEventHelper{AiObjectContext*aiObjectContext;explicit ExternalEventHelper(AiObjectContext*p):aiObjectContext(p){}
#include "NativeCommandParser.inc"
};
struct {std::string commandPrefix,commandSeparator="||";}sPlayerbotAIConfig;
std::string trim(std::string s){auto a=s.find_first_not_of(" \t\r\n");if(a==std::string::npos)return "";return s.substr(a,s.find_last_not_of(" \t\r\n")-a+1);}
void split(std::vector<std::string>&out,std::string s,const char*sep){std::string delim(sep);size_t pos;while((pos=s.find(delim))!=std::string::npos){out.push_back(s.substr(0,pos));s=s.substr(pos+delim.size());}out.push_back(s);}
struct ChatFilter{std::string Filter(std::string s){if(s.find("@priest ")==0)return s.substr(8);return s;}};
struct PlayerbotAI{AiObjectContext*aiObjectContext;ChatFilter chatFilter;bool IsBotCommandMessage(std::string);};
#include "NativeCommandFilter.inc"
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{
 AiObjectContext context;for(auto key:{"co","nc","de","react","follow","corpse run","t","nt"})context.triggers[key];
 PlayerbotAI ai{&context};ExternalEventHelper helper(&context);Player owner;
 for(auto text:{"co ?","nc ?","co +holy,+offdps,?","de ?","react ?","follow","corpse run","nt [item]","#a co ?","@priest nc ?","nc ?||co ?","BOT\t#a co ?","[command:co ?]"})check(ai.IsBotCommandMessage(text),text);
 for(auto text:{"hello Mercy","how are you?","cooperative party","a question about nc ?","Hitem:123","[quest link]"})check(!ai.IsBotCommandMessage(text),text);
 for(auto&entry:context.triggers)check(entry.second.events==0,"recognition must not fire command triggers");
 check(helper.ParseChatCommand("co ?",&owner)&&context.triggers["co"].events==1,"real command executes exactly once");
 sPlayerbotAIConfig.commandPrefix="!";
 check(!ai.IsBotCommandMessage("co ?")&&ai.IsBotCommandMessage("!co ?"),"configured command prefix respected");
 check(ai.IsBotCommandMessage("!nc ?||!co ?"),"prefixed batch recognised");
 std::cout<<"Native command recognition and conversation separation passed\n";
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
