#include <cstdint>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
using uint64=uint64_t;using uint32=uint32_t;
constexpr int SMSG_LOG_XPGAIN=1,SMSG_PARTYKILLLOG=2,SKILL_NONE=0,SKILL_SKINNING=393;
struct ObjectGuid{uint64 raw=0;explicit operator bool()const{return raw!=0;}bool IsPlayer()const{return raw!=0&&(raw>>48)==0;}bool IsCreature()const{return (raw>>48)==0xf130;}bool operator==(ObjectGuid b)const{return raw==b.raw;}};
struct WorldPacket{
 int opcode=SMSG_PARTYKILLLOG;std::vector<uint8_t> bytes;size_t cursor=0;
 int GetOpcode()const{return opcode;}size_t size()const{return bytes.size();}void rpos(size_t v){cursor=v;}
 WorldPacket&operator>>(ObjectGuid&g){if(cursor+8>bytes.size())throw std::runtime_error("packet overread");g.raw=0;for(unsigned n=0;n<8;++n)g.raw|=uint64(bytes[cursor++])<<(n*8);return *this;}
 void append(uint64 v){for(unsigned n=0;n<8;++n)bytes.push_back(uint8_t(v>>(n*8)));}
};
struct Event{WorldPacket packet;WorldPacket const&getPacket(){return packet;}};
struct Group{};
struct Player{bool alive=true,inWorld=true;Group*group=nullptr;bool IsAlive(){return alive;}bool IsInWorld(){return inWorld;}Group*GetGroup(){return group;}};
struct BotState{static constexpr int BOT_STATE_NON_COMBAT=0;};
struct PlayerbotAI{Player*bot;Player*master;bool active=true,loot=true,gather=true;uint32 skill=SKILL_NONE;
 bool HasActivePlayerMaster(){return active;}Player*GetMaster(){return master;}bool HasStrategy(const char*name,int){return std::string(name)=="loot"?loot:gather;}
};
PlayerbotAI*currentAI=nullptr;
struct LootObject{uint32 skillId;LootObject(Player*,ObjectGuid):skillId(currentAI->skill){}};
struct ResetValue{int count=0;void Reset(){++count;}}cached;
#define RESET_AI_VALUE(T,N) cached.Reset()
struct Action{virtual~Action()=default;virtual bool Execute(Event&){return false;}};
struct Admission{int calls=0;bool allow=true;Player*requester=nullptr;ObjectGuid victim;};
Admission admission;
struct AddAllLootAction:Action{
 PlayerbotAI*ai;Player*bot;AddAllLootAction(PlayerbotAI*a,std::string):ai(a),bot(a->bot){}
 bool AddLoot(Player*p,ObjectGuid g){++admission.calls;admission.requester=p;admission.victim=g;return admission.allow;}
};
#ifdef MANGOSBOT_ZERO
#include "native-action.h"
#include "native-action.inc"
#endif
struct PacketHandlers{std::map<int,std::string> names;std::map<int,bool> delayed;void AddHandler(int op,std::string name,bool delay){names[op]=name;delayed[op]=delay;}};
PacketHandlers bindings(){PacketHandlers botOutgoingPacketHandlers;
#include "native-binding.inc"
return botOutgoingPacketHandlers;}
void check(bool x,const char*m){if(!x)throw std::runtime_error(m);}
struct Fixture{Group group,other;Player bot,master;PlayerbotAI ai{&bot,&master};Event event;
 Fixture(){bot.group=master.group=&group;currentAI=&ai;admission={};cached.count=0;event.packet.append(9008);event.packet.append(0xf1300000710139eaULL);}
};
#ifdef MANGOSBOT_ZERO
void valid(){Fixture f;f.event.packet.cursor=7;PartyKillLootAction action(&f.ai);
 check(action.Execute(f.event),"grey party kill must admit without an XP event");check(admission.calls==1&&admission.requester==&f.master,"normal admission receives actual master");check(admission.victim.raw==0xf1300000710139eaULL,"second wire GUID is victim");check(cached.count==1,"new admission invalidates availability cache");check(f.event.packet.cursor==7,"original packet cursor retained");
 admission.allow=false;check(!action.Execute(f.event),"existing admission refusal or duplicate is retained");check(cached.count==1,"refused/duplicate adds do not reset cache");
}
void malformed(){for(int mode=0;mode<6;++mode){Fixture f;switch(mode){case 0:f.event.packet.bytes.resize(15);break;case 1:f.event.packet.opcode=99;break;case 2:f.event.packet.bytes.clear();f.event.packet.append(0);f.event.packet.append(0xf1300000710139eaULL);break;case 3:f.event.packet.bytes.clear();f.event.packet.append(0xf1300000710139eaULL);f.event.packet.append(0xf1300000710139eaULL);break;case 4:f.event.packet.bytes.clear();f.event.packet.append(9008);f.event.packet.append(0xf1100000710139eaULL);break;case 5:f.event.packet.bytes.clear();f.event.packet.append(9008);f.event.packet.append(9009);break;}
 PartyKillLootAction action(&f.ai);check(!action.Execute(f.event)&&admission.calls==0,"malformed or non-corpse packet must not enter admission");}}
void owner(){for(int mode=0;mode<7;++mode){Fixture f;switch(mode){case 0:f.bot.alive=false;break;case 1:f.ai.active=false;break;case 2:f.ai.loot=false;break;case 3:f.ai.master=nullptr;break;case 4:f.master.inWorld=false;break;case 5:f.bot.group=nullptr;break;case 6:f.master.group=&f.other;break;}
 PartyKillLootAction action(&f.ai);check(!action.Execute(f.event)&&admission.calls==0,"inactive/disabled/dead/other-group controls");}}
void policy(){Fixture f;admission.allow=false;PartyKillLootAction action(&f.ai);check(!action.Execute(f.event)&&admission.calls==1&&cached.count==0,"native ownership/range/safety/bag refusal is retained");}
void profession(){Fixture f;f.ai.skill=SKILL_SKINNING;f.ai.gather=false;PartyKillLootAction action(&f.ai);check(!action.Execute(f.event)&&admission.calls==0,"skinning still requires gather");f.ai.skill=SKILL_NONE;check(action.Execute(f.event),"ordinary body does not require profession gather");}
#endif
void wiring(){auto handlers=bindings();check(handlers.names[SMSG_LOG_XPGAIN]=="xpgain","XP registration preserved");
#ifdef MANGOSBOT_ZERO
 check(handlers.names[SMSG_PARTYKILLLOG]=="party kill"&&handlers.delayed[SMSG_PARTYKILLLOG],"ZERO must retain delayed native party-kill events");
#else
 check(!handlers.names.count(SMSG_PARTYKILLLOG),"other variants retain existing event registrations");
#endif
}
int main(int argc,char**argv){try{std::string mode=argc>1?argv[1]:"wiring";
#ifdef MANGOSBOT_ZERO
 if(mode=="valid")valid();else if(mode=="malformed")malformed();else if(mode=="owner")owner();else if(mode=="policy")policy();else if(mode=="profession")profession();else
#endif
 wiring();std::cout<<"PASS "<<mode<<'\n';}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
