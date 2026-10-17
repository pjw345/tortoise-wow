#include <algorithm>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <list>
#include <map>
#include <stdexcept>
#include <string>
using uint32=uint32_t;using int64=int64_t;using ObjectGuid=int;
constexpr int CMSG_RECLAIM_CORPSE=1,CMSG_SELF_RES=2,CORPSE_RESURRECTABLE_PVP=1,ALLIANCE=0;
struct Player;
struct WorldPacket {bool vacant=true;int opcode;explicit WorldPacket(int op=0):opcode(op){}bool empty(){return vacant;}int GetOpcode(){return opcode;}WorldPacket&operator<<(int){return *this;}};
struct Corpse {time_t ghost=std::time(nullptr)-120;time_t GetGhostTime(){return ghost;}int GetType(){return 0;}};
struct Unit {bool alive=true,safe=true,hostile=true;float distance=10,aggro=20;int guid=7;};
struct Session{Player*player;void HandleReclaimCorpseOpcode(WorldPacket&);void HandleSelfResOpcode(WorldPacket&);int reclaims=0,selfres=0;};
struct Player:Unit {bool world=true,teleport=false,grouped=true,combat=false,los=true,bg=false,stopped=true,nativeAccept=true;void*map=reinterpret_cast<void*>(1);Corpse*corpse=nullptr;Session session{this};
 bool IsInWorld(){return world;}bool IsBeingTeleported(){return teleport;}void*GetMap(){return map;}bool IsInGroup(Player*){return grouped;}bool InBattleGround(){return bg;}bool IsStopped(){return stopped;}
 bool IsWithinDistInMap(Player*p,float range,bool){return p->distance<=range;}bool IsWithinLOSInMap(Player*p){return p->los;}Corpse*GetCorpse(){return corpse;}
 uint32 GetCorpseReclaimDelay(bool){return 30;}char const*GetName(){return "Priest";}int GetGUIDLow(){return 1;}int GetTeam(){return 0;}int GetLevel(){return 24;}int GetObjectGuid(){return 1;}Session*GetSession(){return &session;}};
void Session::HandleReclaimCorpseOpcode(WorldPacket&){++reclaims;if(player->nativeAccept)player->alive=true;}
void Session::HandleSelfResOpcode(WorldPacket&){++selfres;if(player->nativeAccept)player->alive=true;}
struct Event{Player*owner=nullptr;WorldPacket packet;Player*getOwner(){return owner;}WorldPacket&getPacket(){return packet;}};
struct Facade{bool IsAlive(Unit*u){return u->alive;}bool UnitIsDead(Unit*u){return !u->alive;}bool IsInCombat(Player*p){return p->combat;}bool IsHostileTo(Unit*u,Player*){return u->hostile;}bool IsDistanceLessThan(float a,float b){return a<b;}}sServerFacade;
struct Config{float spellDistance=30,sightDistance=60,farDistance=100;template<class T>void logEvent(T*,const char*){}}sPlayerbotAIConfig;
struct Log{template<class...T>void outDetail(const char*,T...){}}sLog;
enum class PlayerbotSecurityLevel{PLAYERBOT_SECURITY_ALLOW_ALL};enum class BotState{BOT_STATE_NON_COMBAT};
struct PlayerbotAI{Player*bot,*master;bool active=true,real=false,manual=false;int stopped=0;std::map<int,Unit*>units;
 bool HasActivePlayerMaster(){return active&&master;}bool IsRealPlayer(){return real;}Player*GetMaster(){return master;}Player*GetGroupMaster(){return master;}bool IsSafe(Unit*u){return u&&u->safe;}Unit*GetUnit(int g){return units[g];}
 float GetRange(const char*){return 5;}void StopMoving(){++stopped;bot->stopped=true;}std::string GetDefaultMovementStrategy(){return "follow";}bool HasStrategy(std::string,BotState){return true;}void ChangeStrategy(std::string,BotState){}template<class...T>void TellPlayerNoFacing(T...){}};
bool GetBotAI(Player*){return false;}
int scans=0;std::string qualifier;
struct PossibleTargetsValue{PlayerbotAI*ai;PossibleTargetsValue(PlayerbotAI*a,const char*,float,bool):ai(a){}void Qualify(std::string s){qualifier=s;}std::list<int>Calculate(){++scans;std::list<int>r;for(auto const&e:ai->units)r.push_back(e.first);return r;}};
PlayerbotAI*currentAI=nullptr;
struct WorldPosition{WorldPosition(Player*){}uint32 getUnitsAggro(std::list<int>const&units,Player*){uint32 n=0;for(int g:units){auto*u=currentAI->GetUnit(g);if(u->distance<=u->aggro)++n;}return n;}};
#define AI_VALUE2(type,name,key) (ai->master->distance)
#define AI_VALUE(type,name) (ai->manual)
#define SET_AI_VALUE(type,name,value) (ai->manual=value)
struct MovementAction{PlayerbotAI*ai;Player*bot;int moves=0,legacyCalls=0;Player*destination=nullptr;explicit MovementAction(PlayerbotAI*a):ai(a),bot(a->bot){}Player*GetMaster(){return ai->master;}bool MoveNear(Player*p,float){++moves;destination=p;return true;}};
struct ReviveFromCorpseAction:MovementAction{using MovementAction::MovementAction;static bool IsSafeToAutoResurrect(PlayerbotAI*,Player*);bool Execute(Event&);};
struct FindCorpseAction:MovementAction{using MovementAction::MovementAction;bool Execute(Event&);};
#include "NativeReclaim.inc"
#include "NativeRegroup.inc"
struct SelfResurrectAction:MovementAction{using MovementAction::MovementAction;
#include "NativeSelfRes.inc"
};
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{Player bot,leader;Corpse corpse;bot.alive=false;bot.corpse=&corpse;leader.distance=5;PlayerbotAI ai{&bot,&leader};currentAI=&ai;
 check(ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"safe near living leader");check(qualifier=="61:1","fresh scan bypasses ghost attack filters");
 for(int i=0;i<8;++i){leader.alive=leader.world=leader.grouped=leader.los=leader.safe=true;leader.teleport=leader.combat=false;leader.distance=5;leader.map=bot.map;bot.grouped=true;
 if(i==0)leader.alive=false;if(i==1)leader.world=false;if(i==2)leader.teleport=true;if(i==3)leader.combat=true;if(i==4)leader.distance=31;if(i==5)leader.map=nullptr;if(i==6)bot.grouped=false;if(i==7)leader.los=false;
 int before=scans;check(!ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"unsafe master blocks reclaim");check(scans==before,"master gates precede hostile discovery");}
 leader.los=true;leader.map=bot.map;bot.grouped=true;Unit enemy;ai.units[7]=&enemy;
 check(!ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"hostile inside aggro radius blocks reclaim");enemy.distance=21;check(ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"outside native aggro radius");enemy.distance=10;enemy.alive=false;check(ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"dead enemy ignored");enemy.alive=true;enemy.hostile=false;check(ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"friendly/neutral ignored");ai.units.clear();
 Event event;ReviveFromCorpseAction revive(&ai);leader.distance=50;check(!revive.Execute(event)&&bot.session.reclaims==0,"no reclaim before leader arrives");
 leader.distance=5;corpse.ghost=std::time(nullptr);check(!revive.Execute(event)&&bot.session.reclaims==0,"native reclaim timer retained");corpse.ghost=std::time(nullptr)-120;
 ai.manual=true;bot.nativeAccept=false;check(!revive.Execute(event)&&ai.manual,"native rejection retains corpse-run state");bot.nativeAccept=true;check(revive.Execute(event)&&bot.alive&&!ai.manual,"successful native reclaim clears manual flag");bot.alive=false;
 FindCorpseAction find(&ai);leader.distance=80;check(find.Execute(event)&&find.destination==&leader&&find.legacyCalls==0,"ghost regroups instead of solo corpse run");leader.distance=4;bot.stopped=false;check(!find.Execute(event)&&bot.stopped,"ghost waits near leader and cancels old path");
 ai.manual=true;check(find.Execute(event)&&find.legacyCalls==1,"explicit corpse-run movement preserved");leader.distance=80;check(!revive.Execute(event),"corpse run does not bypass revival safety");ai.manual=false;
 SelfResurrectAction self(&ai);check(!self.Execute(event)&&bot.session.selfres==0,"automatic self-resurrection waits for leader");event.owner=&leader;check(self.Execute(event)&&bot.session.selfres==1,"explicit owner self-resurrection preserved");event.owner=nullptr;
 ai.active=false;check(ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"free bot policy unchanged");ai.active=true;bot.bg=true;check(ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"battleground policy unchanged");bot.bg=false;ai.real=true;check(ReviveFromCorpseAction::IsSafeToAutoResurrect(&ai,&bot),"real-player policy unchanged");
 std::cout<<"Native safe party reclaim, regroup and self-resurrection cases passed\n";
 }catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
