#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cwctype>
enum { LANG_COMMAND_TELE_NOTFOUND, LANG_COMMAND_TP_DELETED };
struct Session { std::string GetUsername(){return "GM_TEST";} };
struct Migration {std::string author;int calls=0;void SetAuthor(std::string a){author=a;++calls;}};
struct World {Migration migration;std::vector<std::string> deleted;Migration&GetMigration(){return migration;}void ExecuteUpdate(const char*,const char*name){deleted.push_back(name);}}sWorld;
bool Utf8toWStr(const std::string&s,std::wstring&w){if(s=="invalid-utf8")return false;w.assign(s.begin(),s.end());return true;}
void wstrToLower(std::wstring&w){for(auto&c:w)c=std::towlower(c);}
struct GameTele{std::string name;std::wstring wnameLow;};using GameTeleMap=std::map<int,GameTele>;
struct ObjectMgr{GameTeleMap m_GameTeleMap;bool DeleteGameTele(std::string const&name);}sObjectMgr;
struct ChatHandler{Session*m_session=nullptr;bool error=false;std::vector<int> messages;void SendSysMessage(int m){messages.push_back(m);}void SetSentErrorMessage(bool b){error=b;}bool HandleTeleDelCommand(char*);};
#include "native.inc"
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{std::string mode=argv[1];Session session;ChatHandler handler;handler.m_session=mode=="session"?&session:nullptr;
 if(mode=="empty"){char arg[]="";check(!handler.HandleTeleDelCommand(arg),"empty rejected");check(!sWorld.migration.calls&&sWorld.deleted.empty(),"no effects for empty");}
 else if(mode=="missing"||mode=="invalid"){std::string arg=mode=="invalid"?"invalid-utf8":"missing";check(!handler.HandleTeleDelCommand(arg.data()),"missing rejected");check(handler.error&&handler.messages.back()==LANG_COMMAND_TELE_NOTFOUND&&sWorld.deleted.empty(),"native missing feedback");}
 else {sObjectMgr.m_GameTeleMap[7]={"TestDestination",L"testdestination"};sObjectMgr.m_GameTeleMap[8]={"Unrelated",L"unrelated"};char arg[]="TESTdestination";check(handler.HandleTeleDelCommand(arg),"native deletion");check(sWorld.migration.author==(mode=="session"?"GM_TEST":"Console"),"author attribution");check(sObjectMgr.m_GameTeleMap.size()==1&&sObjectMgr.m_GameTeleMap.count(8),"cache removes only destination");check(sWorld.deleted==std::vector<std::string>{"TestDestination"},"native database deletion retained");check(handler.messages.back()==LANG_COMMAND_TP_DELETED,"success feedback");check(!handler.HandleTeleDelCommand(arg)&&handler.error,"repeat reports missing");check(sWorld.deleted.size()==1,"no duplicate delete");}
 std::cout<<"PASS "<<mode<<'\n';return 0;}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
