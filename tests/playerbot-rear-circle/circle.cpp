#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
constexpr float M_PI_F=3.14159265358979323846f,CONTACT_DISTANCE=0.5f,INVALID_HEIGHT=-100000;
struct WorldLocation {int mapid=-1;float coord_x=0,coord_y=0,coord_z=0;WorldLocation()=default;WorldLocation(int m,float x,float y,float z):mapid(m),coord_x(x),coord_y(y),coord_z(z){}};
struct Map {float height=20;float GetHeight(float,float,float){return height;}float GetHeight(unsigned,float,float,float){return height;}};
struct PlayerbotAI;struct Group;
struct Unit {
 float x=100,y=-40,z=20,o=0;int mapid=0;bool alive=true,fly=false,freeFly=false;Map map;
 float GetPositionX(){return x;}float GetPositionY(){return y;}float GetPositionZ(){return z;}float GetOrientation(){return o;}
 int GetMapId(){return mapid;}Map*GetMap(){return &map;}unsigned GetPhaseMask(){return 1;}
 bool IsAlive(){return alive;}bool IsFlying(){return fly;}bool IsFreeFlying(){return freeFly;}
 void UpdateAllowedPositionZ(float,float,float&){}
};
struct Player:Unit {PlayerbotAI*ai=nullptr;Group*group=nullptr;int role=0;Group*GetGroup(){return group;}};
struct GroupReference {Player*member=nullptr;GroupReference*nextRef=nullptr;Player*getSource(){return member;}GroupReference*next(){return nextRef;}};
struct Group {std::vector<GroupReference> refs;GroupReference*GetFirstMember(){return refs.empty()?nullptr:&refs[0];}};
struct BotState {static constexpr int BOT_STATE_COMBAT=1;};
struct PlayerbotAI {
 Player*bot=nullptr;bool active=true,combat=false;float range=2;bool safe=true;
 bool HasActivePlayerMaster(){return active;}bool IsStateActive(int){return combat;}float GetRange(const char*){return range;}
 bool IsSafe(Unit*u){return safe&&u;}bool IsTank(Player*p){return p->role==2;}bool IsHeal(Player*p){return p->role==1;}
};
PlayerbotAI*GetBotAI(Player*p){return p->ai;}
struct BotMgr {template<class F>void ForEachPlayerbot(F){}};
BotMgr*GetBotMgr(Player*){return nullptr;}
struct Facade {bool IsAlive(Unit*u){return u->alive;}}sServerFacade;
Unit*followTarget=nullptr;Unit*currentTarget=nullptr;
Unit*value(const char*n){return std::string(n)=="follow target"?followTarget:currentTarget;}
#define AI_VALUE(type,name) value(name)
struct Formation {PlayerbotAI*ai;Player*bot;static WorldLocation NullLocation;Formation(PlayerbotAI*a,const char*):ai(a),bot(a->bot){}virtual WorldLocation GetLocation()=0;float GetFollowAngle();};
WorldLocation Formation::NullLocation;
struct MoveFormation:Formation {using Formation::Formation;};
#include "native-angle.inc"
#include "native-circle.inc"
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);}
bool close(float a,float b){return std::abs(a-b)<0.0002f;}
void checkNativePoint(WorldLocation p,Unit*t,float angle,float range){check(close(p.coord_x,t->x+std::cos(angle)*range)&&close(p.coord_y,t->y+std::sin(angle)*range),"native circle geometry changed outside rear-follow scope");}
int main(){try{
 Player master;Unit enemy;enemy.x=110;enemy.y=-30;followTarget=&master;currentTarget=&enemy;
 for(int count:{1,2,3,4,5,9,10}) {
  Group group;std::vector<Player> bots(count);std::vector<PlayerbotAI> ais(count);group.refs.resize(count+1);
  group.refs[0].member=&master;master.group=&group;
  for(int i=0;i<count;++i){bots[i].group=&group;bots[i].role=i%3;bots[i].ai=&ais[i];ais[i].bot=&bots[i];group.refs[i+1].member=&bots[i];}
  for(int i=0;i<count;++i)group.refs[i].nextRef=&group.refs[i+1];
  for(float heading:{0.0f,0.6f,3.0f,6.2f,-1.0f,8.0f}) {
   master.o=heading;
   std::vector<float> angles;
   for(int i=0;i<count;++i) {
    CircleFormation circle(&ais[i]);float original=circle.GetFollowAngle();auto p=circle.GetLocation();
    float dx=p.coord_x-master.x,dy=p.coord_y-master.y;
    check(dx*std::cos(heading)+dy*std::sin(heading)<-0.05f,"idle human-led circle must put every slot behind its follow target");
    check(close(std::hypot(dx,dy),2),"saved radial follow distance preserved");
    float relative=original-heading;angles.push_back(relative);
    checkNativePoint(p,&master,heading+M_PI_F+(relative-M_PI_F)*0.5f,2);
    check(close(p.coord_z,20.5f),"native ground correction preserved");
    ais[i].active=false;checkNativePoint(circle.GetLocation(),&master,original,2);ais[i].active=true;
    ais[i].combat=true;checkNativePoint(circle.GetLocation(),&enemy,original,2);
    enemy.alive=false;checkNativePoint(circle.GetLocation(),&master,original,2);enemy.alive=true;ais[i].combat=false;
    ais[i].range=1.5f;auto near=circle.GetLocation();check(close(std::hypot(near.coord_x-master.x,near.coord_y-master.y),1.5f),"range changes retained");ais[i].range=2;
   }
   for(int i=0;i<count;++i)for(int j=i+1;j<count;++j)check(std::abs(angles[i]-angles[j])>0.01f,"native roster keeps distinct slots");
  }
 }
 Player bot;PlayerbotAI ai;bot.ai=&ai;ai.bot=&bot;CircleFormation circle(&ai);master.group=nullptr;bot.group=nullptr;
 master.o=0;followTarget=&master;master.map.height=INVALID_HEIGHT;check(circle.GetLocation().mapid==-1,"invalid map height still rejected");master.map.height=20;
 ai.safe=false;check(circle.GetLocation().mapid==-1,"unsafe follow target still rejected");ai.safe=true;
 followTarget=nullptr;check(circle.GetLocation().mapid==-1,"missing follow target still rejected");
 std::cout<<"PASS rear circle and unchanged combat/free-bot geometry\n";
}catch(std::exception const&e){std::cerr<<e.what()<<'\n';return 1;}}
