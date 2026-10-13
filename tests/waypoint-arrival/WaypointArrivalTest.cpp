#include <cstdint>
#include <map>
#include <stdexcept>
#include <iostream>
using uint32=uint32_t;
constexpr uint32 UNIT_STAT_CAN_NOT_MOVE=1, UNIT_STAT_DISTRACTED=2, UNIT_STAT_ROAMING_MOVE=4;
constexpr int STOP_TIME_FOR_PLAYER=30000;
struct Spline { bool finalized=true; bool Finalized() const { return finalized; } };
struct Creature {
 uint32 state=0; bool stopped=true; float x=0,y=0,z=0; Spline spline; Spline* movespline=&spline;
 bool HasUnitState(uint32 mask) const { return (state&mask)!=0; }
 void ClearUnitState(uint32 mask) { state&=~mask; }
 bool IsStopped() const { return stopped; }
 float GetPositionX() const { return x; } float GetPositionY() const { return y; } float GetPositionZ() const { return z; }
};
struct WaypointNode { float x=0,y=0,z=0; };
template<class T> struct WaypointMovementGenerator;
template<> struct WaypointMovementGenerator<Creature> {
 std::map<uint32,WaypointNode> path{{1,{0,0,0}},{2,{10,0,0}}};
 std::map<uint32,WaypointNode>* i_path=&path;
 uint32 i_currentNode=1; int wait=0,arrivals=0,starts=0;
 bool arrivalCanContinue=true;
 bool Stopped() const { return wait>0; }
 void Stop(int time) { wait=time; }
 bool CanMove(int diff) { wait-=diff; return wait<=0; }
 bool OnArrived(Creature&) { ++arrivals;return arrivalCanContinue; }
 void StartMove(Creature&) { ++starts; }
 bool Update(Creature&,const uint32&);
};
#include "NativeWaypointUpdate.inc"
void check(bool value,const char* label){if(!value)throw std::runtime_error(label);}
int main(){try{
 Creature c; WaypointMovementGenerator<Creature> g;
 g.Update(c,50);check(g.arrivals==1&&g.starts==1&&g.wait==0,"stationary first node advances");
 c.x=0.005f;g.arrivals=g.starts=0;g.Update(c,50);check(g.arrivals==1,"within spline displacement tolerance arrives");
 c.x=0.02f;g.arrivals=g.starts=0;g.Update(c,50);check(!g.arrivals&&g.wait==30000,"stopped away from node retains retry delay");
 g.Update(c,29999);check(!g.starts,"interrupted retry honours delay");g.Update(c,1);check(g.starts==1,"retry resumes after delay");
 g.wait=0;c.x=0;c.spline.finalized=false;g.arrivals=g.starts=0;g.Update(c,50);check(!g.arrivals&&g.wait==30000,"unfinished stopped spline does not arrive");
 for(uint32 state:{UNIT_STAT_CAN_NOT_MOVE,UNIT_STAT_DISTRACTED}){
  g.wait=0;c.state=state|UNIT_STAT_ROAMING_MOVE;c.spline.finalized=true;g.arrivals=g.starts=0;
  g.Update(c,50);check(!g.arrivals&&!g.starts&&!(c.state&UNIT_STAT_ROAMING_MOVE),"root or distraction prevents arrival");
 }
 c.state=0;g.wait=0;c.stopped=false;c.x=3;c.spline.finalized=true;g.arrivals=g.starts=0;
 g.Update(c,50);check(g.arrivals==1&&g.starts==1,"ordinary finalized spline retains arrival semantics");
 c.spline.finalized=false;g.arrivals=g.starts=0;g.Update(c,50);check(!g.arrivals&&!g.starts,"active spline stays active");
 c.stopped=true;c.x=0;c.spline.finalized=true;g.arrivalCanContinue=false;g.arrivals=g.starts=0;
 g.Update(c,50);check(g.arrivals==1&&!g.starts,"arrival hook can preserve waypoint wait");
 g.path.clear();g.arrivals=g.starts=0;g.Update(c,50);check(!g.arrivals&&!g.starts,"empty path safe");
 g.i_path=nullptr;g.Update(c,50);check(!g.arrivals&&!g.starts,"missing path safe");
 std::cout<<"Native waypoint update arrival and interruption checks passed\n";
}catch(std::exception const& e){std::cerr<<e.what()<<'\n';return 1;}}
