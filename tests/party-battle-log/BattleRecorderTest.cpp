#include "BattleSessionRecorder.h"
#include <fstream>
#include <iostream>
#include <thread>

using namespace BattleLog;
void Require(bool ok,char const* message) { if(!ok) throw std::runtime_error(message); }
Actor Member(Guid id,bool combat=false) {
    Actor a; a.guid=id; a.name="Actor "+std::to_string(id); a.player=true; a.machine=id!=1;
    a.alive=true; a.combat=combat; a.health=100; a.maxHealth=100; a.mana=800; a.maxMana=1000; return a;
}
size_t CountFiles(std::filesystem::path const& dir) { size_t n=0; for(auto const& f:std::filesystem::directory_iterator(dir)) if(Recorder::OwnedName(f.path().filename().string())) ++n; return n; }
std::string ReadAll(std::filesystem::path const& dir) { std::string out; for(auto const& f:std::filesystem::directory_iterator(dir)) if(f.path().extension()==".jsonl") { std::ifstream in(f.path()); out+=std::string(std::istreambuf_iterator<char>(in),{}); } return out; }
int main(int argc,char** argv) {
    try {
        Require(argc==2,"Output path required");
        auto base=std::filesystem::path(argv[1])/std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        std::filesystem::create_directories(base); Settings cfg; cfg.directory=base/"complete";
        Recorder r; r.Configure(cfg); Actor leader=Member(1),priest=Member(2,true),mage=Member(3,true); leader.name="Hero \"\\\n";
        Event e; e.kind="attack_start"; e.actor=leader;
        Require(r.Start(leader,{1,2,3},{leader,priest,mage},e,"test-revision",1000),"Session did not open");
        Require(r.Start(leader,{1,2,3},{leader,priest,mage},e,"test-revision",1001),"Repeated start should reuse session");
        Require(CountFiles(cfg.directory)==1,"Repeated attack split the battle");
        Actor enemy; enemy.guid=99; enemy.name="Enemy"; enemy.alive=true; enemy.health=1000; enemy.maxHealth=1000;
        for(int i=0;i<20;++i) { e.kind="damage"; e.actor=mage; e.target=enemy; e.amount=7; e.effective=7; e.spell=133; r.Emit(e,1010+i); }
        mage.mana=700;
        e.kind="resource_spent"; e.actor=mage; e.target=enemy; e.amount=100; e.before=800; e.after=700; e.maximum=1000; e.powerType=0; r.Emit(e,1100);
        e=Event{};e.kind="heal";e.actor=priest;e.target=leader;e.amount=100;e.effective=40;e.spell=2061;r.Emit(e,1200);
        e=Event{};e.kind="item_spent";e.actor=mage;e.item=2455;e.amount=1;e.before=3;e.after=2;r.Emit(e,1250);
        leader.alive=false;leader.combat=false;r.Observe(leader,{1,2,3},1300);
        Require(r.Contains(1),"Leader death ended a still-fighting party's battle");
        priest.combat=false;r.Observe(priest,{1,2,3},1400);
        Require(r.Contains(1),"One idle priest ended a still-fighting mage's battle");
        mage.combat=false;r.Observe(mage,{1,2,3},1500);
        Require(!r.Contains(1),"Battle did not close when all members left combat");
        auto complete=ReadAll(cfg.directory);Require(complete.find("\"complete\":true")!=std::string::npos,"Normal end was not complete");
        Require(complete.find("party_out_of_combat")!=std::string::npos,"Wrong end reason");

        cfg.directory=base/"instant";r.Configure(cfg);leader=Member(1);leader.attacking=false;e=Event{};
        e.actor=leader;e.target=enemy;e.kind="cast_start";e.cast=10;e.hostile=true;e.castMs=0;
        r.Start(leader,{1},{leader},e,"test",2000);
        r.Observe(leader,{1},2001);Require(r.Contains(1),"Instant opener closed before launch");
        e.kind="cast_finish";e.result=1;r.Emit(e,2002);r.Observe(leader,{1},2003);Require(!r.Contains(1),"Failed opener did not end");
        Require(ReadAll(cfg.directory).find("attack_did_not_enter_combat")!=std::string::npos,"Failed opener misreported");

        cfg.directory=base/"unknown";r.Configure(cfg);leader=Member(1,true);r.Start(leader,{1,2},{leader},Event{},"test",3000);
        leader.combat=false;r.Observe(leader,{1,2},3001);Require(r.Contains(1),"Unknown/off-map member was silently idle");
        priest=Member(2,false);r.Observe(priest,{1,2},3002);Require(!r.Contains(1),"Off-map member's observed idle state was ignored");
        Require(ReadAll(cfg.directory).find("\"complete\":false")!=std::string::npos,"Initial off-map observation gap claimed completeness");

        cfg.directory=base/"roster";r.Configure(cfg);r.Start(leader,{1,2},{leader,priest},Event{},"test",4000);r.Observe(leader,{1},4001);
        Require(ReadAll(cfg.directory).find("party_changed")!=std::string::npos,"Roster change was not incomplete");
        cfg.directory=base/"logout";r.Configure(cfg);r.Start(leader,{1},{leader},Event{},"test",5000);r.Depart(1,"logout",5001);
        Require(ReadAll(cfg.directory).find("\"complete\":false")!=std::string::npos,"Logout claimed completion");

        cfg.directory=base/"limit";cfg.maxEvents=3;r.Configure(cfg);r.Start(leader,{1},{leader},Event{},"test",6000);e=Event{};e.actor=leader;e.kind="damage";
        for(int i=0;i<10;++i) r.Emit(e,6001+i);
        Require(!r.Contains(1),"Event limit failed");Require(ReadAll(cfg.directory).find("capture_limit")!=std::string::npos,"Truncation not labelled");
        cfg.directory=base/"duration";cfg.maxEvents=50000;cfg.maxDurationMs=10;r.Configure(cfg);r.Start(leader,{1},{leader},Event{},"test",7000);r.Observe(leader,{1},7020);
        Require(ReadAll(cfg.directory).find("duration_limit")!=std::string::npos,"Duration limit failed");

        cfg.directory=base/"bytes";cfg.maxBytes=6000;cfg.maxDurationMs=1800000;r.Configure(cfg);
        r.Start(leader,{1},{leader},Event{},"test",7100);e=Event{};e.actor=leader;e.kind="damage";
        for(int i=0;i<20;++i)r.Emit(e,7101+i);
        Require(!r.Contains(1),"Byte cap failed");
        for(auto const& f:std::filesystem::directory_iterator(cfg.directory)) Require(f.file_size()<=cfg.maxBytes,"Byte cap overrun");
        cfg.maxBytes=8*1024*1024;

        cfg.directory=base/"scope";r.Configure(cfg);leader=Member(1,true);r.Start(leader,{1},{leader},Event{},"test",7200);
        e=Event{};e.kind="scope_gap";e.actor.guid=1;r.Emit(e,7201);r.Observe(leader,{1},7202);Require(r.Contains(1),"Unknown cross-map state became idle");
        leader.combat=false;r.Observe(leader,{1},7203);Require(ReadAll(cfg.directory).find("scope_gap")!=std::string::npos,"Scope gap not recorded");

        cfg.directory=base/"cross-party";r.Configure(cfg);Actor p21=Member(21,true),p22=Member(22,true);
        r.Start(p21,{21},{p21},Event{},"test",7300);r.Start(p22,{22},{p22},Event{},"test",7300);
        Require(r.NeedsContext(21),"New battle context missing");e=Event{};e.kind="bot_context";e.actor=p21;r.Emit(e,7301);Require(!r.NeedsContext(21),"Repeated context requested");
        e=Event{};e.kind="damage";e.actor=p21;e.target=p22;e.effective=9;r.Emit(e,7302);
        p21.combat=false;p22.combat=false;r.Observe(p21,{21},7303);r.Observe(p22,{22},7303);Require(CountFiles(cfg.directory)==2,"Cross-party event lost a capture");

        cfg.directory=base/"retention";cfg.maxFiles=2;cfg.maxDurationMs=1800000;std::filesystem::create_directories(cfg.directory);
        {std::ofstream unrelated(cfg.directory/"battle-user-notes.jsonl");unrelated << "preserve";}
        r.Configure(cfg);
        for(int i=0;i<4;++i) { r.Start(leader,{1},{leader},Event{},"test",8000+i);r.Depart(1,"test",8000+i); }
        Require(CountFiles(cfg.directory)==2,"Retention cap failed");Require(std::filesystem::exists(cfg.directory/"battle-user-notes.jsonl"),"Unowned file deleted");

        cfg.directory=base/"concurrent";cfg.maxFiles=100;cfg.maxSessions=8;r.Configure(cfg);
        std::vector<std::thread> workers;
        for(Guid id=10;id<18;++id) workers.emplace_back([&,id]{ Actor p=Member(id,true);r.Start(p,{id},{p},Event{},"test",9000);Event hit;hit.kind="damage";hit.actor=p;for(int i=0;i<100;++i)r.Emit(hit,9001+i);p.combat=false;r.Observe(p,{id},9200); });
        for(auto& worker:workers)worker.join();Require(CountFiles(cfg.directory)==8,"Concurrent sessions lost files");
        Require(ReadAll(cfg.directory).find("\"complete\":true")!=std::string::npos,"Concurrent completion failed");

        cfg.directory=base/"shutdown";r.Configure(cfg);r.Start(leader,{1},{leader},Event{},"test",10000);r.CloseAll("server_shutdown");
        Require(ReadAll(cfg.directory).find("server_shutdown")!=std::string::npos,"Shutdown trailer missing");
        auto blocker=base/"not-a-directory";{std::ofstream f(blocker);f<<"x";}cfg.directory=blocker/"logs";r.Configure(cfg);
        Require(!r.Start(leader,{1},{leader},Event{},"test",11000),"I/O failure should be nonfatal and rejected");
        cfg.enabled=false;cfg.directory=base/"disabled";r.Configure(cfg);
        Require(!r.Start(leader,{1},{leader},Event{},"test",12000),"Disabled recorder accepted late opening event");
        cfg.enabled=true;cfg.allowedLeaderLowGuid=99;r.Configure(cfg);
        Require(!r.Start(leader,{1},{leader},Event{},"test",12001),"Leader filter was not enforced under lock");
        std::cout << base.string() << '\n';
    } catch(std::exception const& e) { std::cerr << e.what() << '\n'; return 1; }
}
