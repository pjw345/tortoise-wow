#pragma once

// Scalar-only recorder. No game objects survive a hook, and file/state access
// is serialized across map owners. The adapter supplies native observations.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <map>
#include <locale>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace BattleLog
{
using Guid = unsigned long long;
inline std::string Decimal(double value)
{
    if(!std::isfinite(value)) return "null";
    std::ostringstream out;out.imbue(std::locale::classic());out << value;return out.str();
}
inline std::string Quote(std::string const& text)
{
    std::ostringstream out; out << '"';
    for (unsigned char c : text)
    {
        switch (c) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\r': out << "\\r"; break;
            case '\t': out << "\\t"; break;
            default:
                if (c < 32) { const char* h="0123456789abcdef"; out << "\\u00" << h[c >> 4] << h[c & 15]; }
                else out << c;
        }
    }
    return out.str() + '"';
}
struct Actor
{
    Guid guid=0, owner=0, target=0, selected=0;
    std::string name;
    unsigned entry=0, characterClass=0, map=0, instance=0;
    unsigned health=0, maxHealth=0, mana=0, maxMana=0, rage=0, maxRage=0;
    unsigned power=0, maxPower=0, powerType=0;
    unsigned casting=0,shooting=0;
    unsigned unitState=0,unitFlags=0;
    bool targetFleeing=false,targetSampleKnown=false;
    float targetX=0,targetY=0,targetZ=0;
    bool player=false, machine=false, alive=false, combat=false, fleeing=false, assistanceRunning=false, attacking=false;
    float x=0,y=0,z=0;
    std::string Json() const
    {
        std::ostringstream o; o.imbue(std::locale::classic());
        o << "{\"guid\":" << Quote(std::to_string(guid)) << ",\"owner\":" << Quote(std::to_string(owner))
          << ",\"name\":" << Quote(name) << ",\"entry\":" << entry << ",\"class\":" << characterClass
          << ",\"player\":" << player << ",\"machine\":" << machine << ",\"map\":" << map << ",\"instance\":" << instance
          << ",\"health\":" << health << ",\"max_health\":" << maxHealth << ",\"mana\":" << mana << ",\"max_mana\":" << maxMana
          << ",\"rage\":" << rage << ",\"max_rage\":" << maxRage << ",\"power\":" << power << ",\"max_power\":" << maxPower
          << ",\"mana_pct\":" << (maxMana?Decimal(100.0*mana/maxMana):"null")
          << ",\"rage_pct\":" << (maxRage?Decimal(100.0*rage/maxRage):"null")
          << ",\"power_pct\":" << (maxPower?Decimal(100.0*power/maxPower):"null")
          << ",\"power_type\":" << powerType << ",\"alive\":" << alive << ",\"combat\":" << combat << ",\"fleeing\":" << fleeing
          << ",\"assistance_running\":" << assistanceRunning << ",\"attacking\":" << attacking << ",\"casting\":" << casting << ",\"shooting\":" << shooting << ",\"target\":" << Quote(std::to_string(target)) << ",\"selected\":" << Quote(std::to_string(selected))
          << ",\"unit_state\":" << unitState << ",\"unit_flags\":" << unitFlags << ",\"target_fleeing\":" << targetFleeing << ",\"target_sample_known\":" << targetSampleKnown
          << ",\"target_x\":" << Decimal(targetX) << ",\"target_y\":" << Decimal(targetY) << ",\"target_z\":" << Decimal(targetZ)
          << ",\"x\":" << Decimal(x) << ",\"y\":" << Decimal(y) << ",\"z\":" << Decimal(z) << '}';
        return o.str();
    }
};
struct Event
{
    std::string kind, spellName, itemName, detail;
    Actor actor, target;
    Guid cast=0;
    unsigned spell=0, item=0;
    long long amount=0, effective=0, before=0, after=0, maximum=0;
    int powerType=-1, result=0;
    unsigned castMs=0, absorbed=0, outcome=0;
    long long resisted=0;
    bool periodic=false, critical=false, triggered=false, healing=false, hostile=false, wand=false, targetWasFleeing=false;
    double threat=0,threatTotal=0;
    std::string Json() const
    {
        std::ostringstream o; o.imbue(std::locale::classic());
        o << "\"kind\":" << Quote(kind) << ",\"actor\":" << actor.Json() << ",\"target\":" << target.Json()
          << ",\"spell\":" << spell << ",\"spell_name\":" << Quote(spellName) << ",\"item\":" << item << ",\"item_name\":" << Quote(itemName)
          << ",\"cast\":" << Quote(std::to_string(cast)) << ",\"cast_ms\":" << castMs << ",\"amount\":" << amount << ",\"effective\":" << effective
          << ",\"before\":" << before << ",\"after\":" << after << ",\"maximum\":" << maximum << ",\"power_type\":" << powerType
          << ",\"pool_before_pct\":" << (maximum?Decimal(100.0*before/maximum):"null")
          << ",\"pool_after_pct\":" << (maximum?Decimal(100.0*after/maximum):"null")
          << ",\"result\":" << result << ",\"periodic\":" << periodic << ",\"critical\":" << critical << ",\"triggered\":" << triggered
          << ",\"healing\":" << healing << ",\"absorbed\":" << absorbed << ",\"resisted\":" << resisted << ",\"outcome\":" << outcome
          << ",\"hostile\":" << hostile << ",\"wand\":" << wand << ",\"target_was_fleeing\":" << targetWasFleeing
          << ",\"threat\":" << Decimal(threat) << ",\"threat_total\":" << Decimal(threatTotal) << ",\"detail\":" << Quote(detail);
        return o.str();
    }
};
struct Settings
{
    std::filesystem::path directory;
    bool enabled=true;
    unsigned allowedLeaderLowGuid=0;
    size_t maxBytes=8*1024*1024, maxEvents=50000, maxFiles=100, maxSessions=8;
    unsigned long long maxDurationMs=30*60*1000;
};
class Recorder
{
    struct FileCloser { void operator()(std::FILE* file) const { if(file) std::fclose(file); } };
    struct Session
    {
        Guid leader=0;
        std::unique_ptr<std::FILE,FileCloser> file;
        std::filesystem::path path;
        unsigned long long start=0, lastFlush=0, seq=0;
        size_t bytes=0, events=0;
        bool sawCombat=false, ioFailed=false, scopeGap=false;
        std::set<Guid> roster;
        std::map<Guid,Actor> actors;
        std::map<Guid,unsigned long long> observedAt;
        std::map<Guid,unsigned long long> samples;
        std::set<Guid> pendingCasts;
        std::set<Guid> context;
    };
    Settings settings;
    mutable std::mutex mutex;
    std::map<Guid,Session> sessions;
    std::map<Guid,Guid> members;
    unsigned long long serial=0;
    bool Write(Session& s, std::string const& fields, unsigned long long now)
    {
        std::string line="{\"schema\":1,\"seq\":"+std::to_string(++s.seq)+",\"time_ms\":"+std::to_string(now)+",\"elapsed_ms\":"+std::to_string(now-s.start)+","+fields+"}\n";
        bool ok=std::fwrite(line.data(),1,line.size(),s.file.get())==line.size();
        s.bytes+=line.size(); s.ioFailed |= !ok;
        if (now-s.lastFlush>=1000) { s.ioFailed |= std::fflush(s.file.get())!=0; s.lastFlush=now; }
        return ok;
    }
    std::set<Guid> Find(Event const& e) const
    {
        std::set<Guid> result;
        for(Guid g : {e.actor.guid,e.actor.owner,e.target.guid,e.target.owner})
        { auto i=members.find(g); if(i!=members.end()) result.insert(i->second); }
        return result;
    }
    void Finish(Guid key, char const* reason, bool complete, unsigned long long now)
    {
        auto i=sessions.find(key); if(i==sessions.end()) return;
        Session& s=i->second;
        Write(s,"\"kind\":\"battle_end\",\"complete\":"+std::string(complete&&!s.ioFailed&&!s.scopeGap?"true":"false")+",\"reason\":"+Quote(complete&&s.scopeGap?"scope_gap":reason)+",\"events\":"+std::to_string(s.events),now);
        std::fflush(s.file.get()); s.file.reset();
        for(auto m=members.begin();m!=members.end();) { if(m->second==key) m=members.erase(m); else ++m; }
        sessions.erase(i);
    }
    void Prune()
    {
        // Only this facility's regular battle-*.jsonl files; never symlinks or
        // open files. Failure to prune prevents new files rather than growing.
        std::vector<std::filesystem::directory_entry> files;
        for(auto const& f:std::filesystem::directory_iterator(settings.directory))
            if(f.is_regular_file()&&!f.is_symlink()&&OwnedName(f.path().filename().string())) files.push_back(f);
        std::sort(files.begin(),files.end(),[](auto const& a,auto const& b){return a.path().filename()<b.path().filename();});
        while(files.size()>=settings.maxFiles)
        {
            auto i=std::find_if(files.begin(),files.end(),[&](auto const& f){return std::none_of(sessions.begin(),sessions.end(),[&](auto const& s){return s.second.path==f.path();});});
            if(i==files.end()) throw std::runtime_error("Battle log retention limit has no closed file");
            std::filesystem::remove(i->path()); files.erase(i);
        }
    }
public:
    static bool OwnedName(std::string const& name)
    {
        if(name.rfind("battle-",0)!=0||name.size()<18||name.substr(name.size()-6)!=".jsonl") return false;
        std::string middle=name.substr(7,name.size()-13); unsigned separators=0; bool digit=false;
        for(char c:middle) { if(c=='-') { if(!digit) return false; digit=false; ++separators; } else if(c>='0'&&c<='9') digit=true; else return false; }
        return digit && separators==2;
    }
    static unsigned long long Now()
    { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
    void Configure(Settings const& value)
    {
        std::lock_guard<std::mutex> lock(mutex);
        while(!sessions.empty()) Finish(sessions.begin()->first,"configuration_changed",false,Now());
        settings=value;
    }
    bool Start(Actor const& leader, std::set<Guid> roster, std::vector<Actor> const& initial, Event const& event, std::string const& revision, unsigned long long now=Now())
    {
        std::lock_guard<std::mutex> lock(mutex);
        if(!settings.enabled || (settings.allowedLeaderLowGuid && settings.allowedLeaderLowGuid!=(leader.guid&0xffffffffull))) return false;
        if(members.count(leader.guid)) { EmitLocked(event,now); return true; }
        for(Guid g:roster) if(members.count(g)) return false; // never duplicate a live party's battle
        if(sessions.size()>=settings.maxSessions) return false;
        try {
            std::filesystem::create_directories(settings.directory); Prune();
            auto wall=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            Session s; s.leader=leader.guid; s.start=now; s.lastFlush=now; s.roster=std::move(roster);
            s.path=settings.directory/("battle-"+std::to_string(wall)+"-"+std::to_string(leader.guid)+"-"+std::to_string(++serial)+".jsonl");
            s.file.reset(std::fopen(s.path.string().c_str(),"wb")); if(!s.file) return false;
            for(auto const& a:initial) { s.actors[a.guid]=a; s.observedAt[a.guid]=now; s.sawCombat |= a.combat; }
            std::string rosterJson="["; for(Guid g:s.roster) { if(rosterJson!="[") rosterJson+=','; rosterJson+=Quote(std::to_string(g)); } rosterJson+=']';
            std::string partyJson="["; for(auto const& a:initial) { if(partyJson!="[") partyJson+=','; partyJson+=a.Json(); } partyJson+=']';
            if(!Write(s,"\"kind\":\"battle_start\",\"leader\":"+leader.Json()+",\"roster\":"+rosterJson+",\"party\":"+partyJson+",\"revision\":"+Quote(revision)+",\"wall_time_us\":"+std::to_string(wall),now)) return false;
            auto key=s.leader; sessions.emplace(key,std::move(s));
            for(Guid g:sessions.at(key).roster) members[g]=key;
            EmitLocked(event,now); // opening intent/pending cast and file creation are atomic
        } catch(std::exception const&) { return false; }
        return true;
    }
    bool Contains(Guid guid) const { std::lock_guard<std::mutex> lock(mutex); return members.count(guid)!=0; }
    bool NeedsContext(Guid guid) const
    {
        std::lock_guard<std::mutex> lock(mutex);auto i=members.find(guid);
        return i!=members.end()&&!sessions.at(i->second).context.count(guid);
    }
private:
    void EmitLocked(Event const& e, unsigned long long now)
    {
        auto keys=Find(e);
        std::string data=e.Json();
        for(Guid key:keys)
        {
        auto i=sessions.find(key); if(i==sessions.end()) continue;
        Session& s=i->second;
        if(s.bytes+data.size()+2048>=settings.maxBytes||s.events>=settings.maxEvents||now-s.start>=settings.maxDurationMs||s.ioFailed)
        { Finish(i->first,s.ioFailed?"io_error":"capture_limit",false,now); continue; }
        s.scopeGap |= e.kind=="scope_gap";
        if(e.kind!="scope_gap" && e.actor.guid && (s.roster.count(e.actor.guid)||s.roster.count(e.actor.owner)))
        { s.actors[e.actor.guid]=e.actor; s.observedAt[e.actor.guid]=now; s.sawCombat|=e.actor.combat; if(e.actor.owner) members[e.actor.guid]=i->first; }
        if(e.kind!="scope_gap" && e.target.guid && (s.roster.count(e.target.guid)||s.roster.count(e.target.owner)))
        { s.actors[e.target.guid]=e.target; s.observedAt[e.target.guid]=now; s.sawCombat|=e.target.combat; if(e.target.owner) members[e.target.guid]=i->first; }
        if(e.kind=="cast_start"&&!e.triggered&&!e.actor.machine&&e.hostile) s.pendingCasts.insert(e.cast);
        if(e.kind=="cast_finish") s.pendingCasts.erase(e.cast);
        if(e.kind=="bot_context") s.context.insert(e.actor.guid);
        ++s.events;
        std::string leader=s.actors.count(s.leader)?s.actors.at(s.leader).Json():"null";
        Write(s,data+",\"leader\":"+leader+",\"leader_sample_age_ms\":"+std::to_string(now-s.observedAt[s.leader]),now);
        }
    }
public:
    void Emit(Event const& e, unsigned long long now=Now())
    { std::lock_guard<std::mutex> lock(mutex); EmitLocked(e,now); }
    void Observe(Actor const& actor, std::set<Guid> const& roster, unsigned long long now=Now())
    {
        std::lock_guard<std::mutex> lock(mutex);
        auto m=members.find(actor.guid); if(m==members.end()) return;
        Guid key=m->second; Session& s=sessions.at(key);
        if(roster!=s.roster) { Finish(key,"party_changed",false,now); return; }
        s.actors[actor.guid]=actor; s.observedAt[actor.guid]=now; s.sawCombat|=actor.combat;
        if(now-s.start>=settings.maxDurationMs) { Finish(key,"duration_limit",false,now); return; }
        if(s.bytes+4096>=settings.maxBytes||s.events>=settings.maxEvents||s.ioFailed) { Finish(key,"capture_limit",false,now); return; }
        if(now-s.samples[actor.guid]>=500)
        {
            Event e; e.kind="state"; e.actor=actor; ++s.events; Write(s,e.Json(),now); s.samples[actor.guid]=now;
        }
        for(Guid g:s.roster) if(!s.actors.count(g)) return; // off-map/offline is unknown, never quietly idle
        if(!s.sawCombat&&!s.pendingCasts.empty()) return;
        for(auto const& a:s.actors) if(a.second.combat||(!s.sawCombat&&a.second.attacking)) return;
        Finish(key,s.sawCombat?"party_out_of_combat":"attack_did_not_enter_combat",s.sawCombat,now);
    }
    void Depart(Guid guid, char const* reason, unsigned long long now=Now())
    { std::lock_guard<std::mutex> lock(mutex); auto i=members.find(guid); if(i!=members.end()) Finish(i->second,reason,false,now); }
    void CloseAll(char const* reason)
    { std::lock_guard<std::mutex> lock(mutex); while(!sessions.empty()) Finish(sessions.begin()->first,reason,false,Now()); }
    ~Recorder() { try { CloseAll("process_exit"); } catch(...) {} }
};
}
