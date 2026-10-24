from pathlib import Path
import sys,subprocess
root=Path(__file__).resolve().parents[2]; out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def fn(path,name):
 s=(root/path).read_text();a=s.index(name);b=s.index('{',a);i=b+1;depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
follow=fn('modules/mod-playerbots/src/playerbot/strategy/actions/MovementActions.cpp','bool MovementAction::FollowOnTransport(')
read=fn('src/game/Objects/Object.cpp','void MovementInfo::Read(')
fixture=r'''
#include <iostream>
#include <stdexcept>
#include <vector>
using uint32=unsigned;
enum{MOVEFLAG_ONTRANSPORT=1,MOVEFLAG_SWIMMING=2,MOVEFLAG_JUMPING=4,MOVEFLAG_SPLINE_ELEVATION=8};
struct Pos{float x=0,y=0,z=0,o=0;};
struct ByteBuffer{std::vector<double> data;unsigned i=0;template<class T>ByteBuffer&operator>>(T&v){v=T(data.at(i++));return *this;}};
struct WorldTimer{static unsigned getMSTime(){return 42;}};
struct MovementInfo{unsigned stime=0,moveFlags=0,ctime=0,t_guid=0,t_time=0,fallTime=0;Pos pos,t_pos;float s_pitch=0,splineElevation=0;struct{float zspeed=0,cosAngle=0,sinAngle=0,xyspeed=0;unsigned startClientTime=0;Pos start;}jump;
 bool HasMovementFlag(unsigned f){return moveFlags&f;}void ClearTransportData(){t_guid=t_time=0;t_pos=Pos{};}void Read(ByteBuffer&);};
struct Unit;
struct GenericTransport{int adds=0,removes=0;void RemovePassenger(Unit*);void AddPassenger(Unit*);};
struct Unit{MovementInfo m_movementInfo;GenericTransport*transport=nullptr;int relocations=0,heartbeats=0;GenericTransport*GetTransport(){return transport;}float GetPositionX(){return 0;}float GetPositionY(){return 0;}float GetPositionZ(){return 0;}void Relocate(float,float,float){++relocations;}void SendHeartBeat(){++heartbeats;}};
void GenericTransport::RemovePassenger(Unit*u){++removes;u->transport=nullptr;}void GenericTransport::AddPassenger(Unit*u){++adds;u->transport=this;}
struct AI{int stops=0;void StopMoving(){++stops;}};
struct Facade{float distance=2;float GetDistance2d(Unit*,Unit*){return distance;}bool IsDistanceLessOrEqualThan(float a,float b){return a<=b;}}sServerFacade;
struct Config{float sightDistance=60;}sPlayerbotAIConfig;
struct MovementAction{Unit*bot;AI*ai;bool FollowOnTransport(Unit*);};
FOLLOW
READ
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){
 for(int mode=0;mode<7;++mode){Unit b,t;AI ai;GenericTransport a,z;MovementAction action{&b,&ai};b.m_movementInfo.t_guid=111;t.m_movementInfo.t_guid=222;sServerFacade.distance=2;
  if(mode==1)b.transport=t.transport=&a;
  if(mode==2)t.transport=&a;
  if(mode==3)b.transport=&a;
  if(mode==4){b.transport=&a;t.transport=&z;}
  if(mode==5){t.transport=&a;sServerFacade.distance=100;}
  if(mode==6){b.transport=&a;t.transport=&z;b.m_movementInfo.t_guid=t.m_movementInfo.t_guid;}
  bool handled=action.FollowOnTransport(&t);bool expected=mode==2||mode==3||mode==4||mode==6;
  check(handled==expected,"native attachment determines transfer");check(ai.stops==int(expected)&&b.heartbeats==int(expected),"no false stop or heartbeat");
  check(b.transport==(expected?t.transport:(mode==1?&a:nullptr)),"native passenger destination preserved");
  check(a.removes==int(mode==3||mode==4||mode==6),"native removal once");
 }
 MovementInfo m;m.t_guid=55;m.t_time=123;m.t_pos={1,2,3,4};
 ByteBuffer off{{0,100,1,2,3,4,0}};m.Read(off);check(!m.t_guid&&!m.t_time&&!m.t_pos.z&&off.i==off.data.size(),"omitted transport data cleared without consuming bytes");
 ByteBuffer on{{MOVEFLAG_ONTRANSPORT|MOVEFLAG_SWIMMING,101,1,2,3,4,99,5,6,7,8,.5,10}};m.Read(on);check(m.t_guid==99&&m.t_pos.z==7&&m.s_pitch==.5&&m.fallTime==10&&on.i==on.data.size(),"transport and following optional fields retained");
 ByteBuffer jumped{{MOVEFLAG_JUMPING|MOVEFLAG_SPLINE_ELEVATION,102,1,2,3,4,10,5,6,7,8,9}};m.Read(jumped);check(!m.t_guid&&m.jump.startClientTime==102&&m.splineElevation==9&&jumped.i==jumped.data.size(),"leave preserves jump and spline parsing");
 std::cout<<"PASS transport follow and repeated movement packet lifecycle\n";
}
'''
for label,f,r in [('before',follow.replace('bool const onDifferentTransports = myTransport != hisTransport;','bool const onDifferentTransports = bot->m_movementInfo.t_guid != target->m_movementInfo.t_guid;'),read.replace('    else\n        ClearTransportData();\n','')),('after',follow,read)]:
 p=out/(label+'.cpp');p.write_text(fixture.replace('FOLLOW',f).replace('READ',r));exe=out/label
 subprocess.run(['c++','-std=c++17',str(p),'-o',str(exe)],check=True)
 v=subprocess.run([str(exe)],capture_output=True,text=True);print(label,v.returncode,v.stdout.strip(),v.stderr.strip());assert (v.returncode!=0) if label=='before' else (v.returncode==0)
