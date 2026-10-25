#include "ResourceMarkers.h"
#include <iostream>
#include <thread>
#include <stdexcept>
using namespace ai::resources;
void check(bool value) { if (!value) throw std::runtime_error("resource protocol regression"); }
int main() {
 Request r;
 check(ParseRequest("R:1:4294967295:both",r));
 for (auto s : {"R:1:1:ore","R:1:2:herbs"}) check(ParseRequest(s,r));
 for (auto s : {"R:1:0:both","R:2:1:both","R:1:4294967296:both","R:1:1:all",
  "R:1:1:both:extra","R|1|1|both","R:1:-1:ore","R:1:1:both\n"}) check(!ParseRequest(s,r));
 ParseRequest("R:1:4294967295:both",r);
 check(Pack(r,"Alon",38,{})[0]=="S:1:4294967295:Alon:38:1:1:");
 std::vector<std::string> rows(32,"17370383791809394007,4294967295,O,100.000,100.000");
 auto packets=Pack(r,std::string(48,'A'),4294967295,rows); check(packets.size()==16);
 for(auto const& p:packets) check(p.size()<=240);
 rows.push_back("extra"); check(Pack(r,"Alon",38,rows).empty());
 check(Pack(r,std::string(49,'A'),38,{}).empty());
 std::atomic<time_t> last{0}; check(ai::questsync::Admit(last,100));
 check(!ai::questsync::Admit(last,104)); check(ai::questsync::Admit(last,105));
 RequestMailbox box; check(!box.Queue("invalid",100));
 check(box.Queue("R:1:1:both",100));check(!box.Queue("R:1:2:both",106));
 check(box.Take()=="R:1:1:both" && box.Take().empty());check(!box.Queue("R:1:3:both",104));
 check(box.Queue("R:1:4:both",105));check(box.Take()=="R:1:4:both");
 RequestMailbox concurrent;std::atomic<int> admitted{0};
 auto enqueue=[&]{if(concurrent.Queue("R:1:5:both",200))++admitted;};
 std::thread a(enqueue),b(enqueue);a.join();b.join();
 check(admitted==1 && concurrent.Take()=="R:1:5:both");
 std::cout<<"PASS: bounded requests, chunking, empty snapshots and shared native rate gate\n";
}
