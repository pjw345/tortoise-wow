#include <atomic>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

using uint32=uint32_t;
struct Unit { bool relevant=false; Unit* ToUnit() { return this; } };
struct Item { uint32 entry; uint32 GetEntry() { return entry; } };
struct Spell
{
    Unit* caster=nullptr;
    struct Targets { Unit* target=nullptr; Unit* getUnitTarget() { return target; } } m_targets;
    uint64_t m_battleLogCastId=0;
    uint32 m_battleLogItemId=0;
    Item* item=nullptr;
    Unit* GetCaster() { return caster; }
    Item* GetCastItem() { return item; }
};
struct Event
{
    std::string kind,detail;
    uint64_t cast=0;
    int powerType=0;
    uint32 item=0,before=0,after=0,maximum=0;
    long long amount=0;
};
struct Recorder { std::vector<Event> events; void Emit(Event const& e) { events.push_back(e); } } recorder;
std::atomic<uint64_t> nextCast{0};
bool enabled=true;
bool Enabled() { return enabled; }
bool Relevant(Unit* u) { return u&&u->relevant; }
Event SpellEvent(Spell* s,char const* kind)
{
    Event e;e.kind=kind;e.cast=s->m_battleLogCastId;e.item=s->m_battleLogItemId;return e;
}
#include "CastAttachmentMethods.h"
void Require(bool ok) { if(!ok) throw std::runtime_error("Native cast attachment regression"); }
int main()
{
    Unit party{true},outside{false};
    Spell cast;cast.caster=&party;
    // Preparation happened before capture: retain the actual later power charge.
    Resource(&cast,0,1368,1248,1500);
    Require(recorder.events.size()==2);
    Require(recorder.events[0].kind=="cast_attached"&&recorder.events[0].cast!=0);
    auto paid=recorder.events[1];
    Require(paid.kind=="resource_spent"&&paid.amount==120&&paid.before==1368&&paid.after==1248&&paid.maximum==1500);
    Require(paid.cast==recorder.events[0].cast);
    ItemSpent(&cast,2455,32,31);
    Require(recorder.events.size()==3&&recorder.events.back().cast==paid.cast&&recorder.events.back().amount==1);
    // Native item destruction cleared the pointer before this hook.
    Spell potion;potion.caster=&party;
    ItemSpent(&potion,2455,2,1);
    Require(recorder.events[3].kind=="cast_attached"&&recorder.events[3].item==2455);
    Require(recorder.events.back().item==2455&&recorder.events.back().amount==1);
    Spell begun;begun.caster=&party;begun.m_battleLogCastId=500;
    size_t count=recorder.events.size();
    Resource(&begun,1,100,50,1000);
    Require(recorder.events.size()==count+1&&recorder.events.back().cast==500&&recorder.events.back().amount==50);
    Spell irrelevant;irrelevant.caster=&outside;
    count=recorder.events.size();Resource(&irrelevant,0,100,0,100);
    Require(recorder.events.size()==count&&!irrelevant.m_battleLogCastId);
    irrelevant.m_targets.target=&party;
    Resource(&irrelevant,0,100,0,100);
    Require(irrelevant.m_battleLogCastId&&recorder.events.size()==count+2);
    Spell disabled;disabled.caster=&party;enabled=false;count=recorder.events.size();
    Resource(&disabled,0,100,0,100);ItemSpent(&disabled,2455,2,1);
    Require(!disabled.m_battleLogCastId&&recorder.events.size()==count);
    enabled=true;Spell absent;
    Resource(&absent,0,100,0,100);
    Require(!absent.m_battleLogCastId&&recorder.events.size()==count);
}
