#include <cstdint>
#include <stdexcept>
struct Guid {uint64_t GetRawValue(){return 1;}};
struct Player {bool world=true,near=false,far=false;Guid GetObjectGuid(){return {};}bool IsInWorld(){return world;}bool IsBeingTeleportedNear(){return near;}bool IsBeingTeleportedFar(){return far;}};
struct Recorder {int observations=0,departures=0;bool contains=true;bool Contains(uint64_t){return contains;}void Depart(uint64_t,char const*){++departures;}void Observe(int,int){++observations;}}recorder;
bool enabled=true;bool Enabled(){return enabled;}int Snapshot(Player*){return 0;}int Roster(Player*){return 0;}
#include "ObserveMethod.h"
void Check(bool ok){if(!ok)throw std::runtime_error("Native battle observation boundary");}
int main(){Player p;Observe(&p);Check(recorder.observations==1);
p.near=true;Observe(&p);Check(recorder.observations==1&&recorder.departures==0);
p.near=false;Observe(&p);Check(recorder.observations==2&&recorder.departures==0);
p.far=true;Observe(&p);Check(recorder.departures==1);
p.far=false;p.world=false;Observe(&p);Check(recorder.departures==2);
p.world=true;enabled=false;Observe(&p);Check(recorder.observations==2);}
