#include <algorithm>
#include <cstdarg>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using uint32 = uint32_t;
enum CurrentSpellTypes { CURRENT_MELEE_SPELL, CURRENT_GENERIC_SPELL, CURRENT_AUTOREPEAT_SPELL, CURRENT_CHANNELED_SPELL, CURRENT_MAX_SPELL };
constexpr uint32 SPELLCATEGORY_RANGED_WEAPON = 351, SPELLCATEGORY_GLOBAL = 133;
constexpr uint32 SPELL_ATTR_DISABLED_WHILE_ACTIVE = 1;
enum SpellCastResult { SPELL_CAST_OK, SPELL_FAILED_MOVING, SPELL_FAILED_NOT_READY, SPELL_FAILED_DONT_REPORT };
constexpr int RANGED_ATTACK = 0, UNIT_STAND_STATE_STAND = 0;
struct WorldTimer {
    static uint32 now;
    static uint32 getMSTime() { return now; }
    static uint32 getMSTimeDiff(uint32 a, uint32 b) { return b-a; }
};
uint32 WorldTimer::now = 0;
struct SpellEntry {
    uint32 Id, Category = 0, StartRecoveryCategory = SPELLCATEGORY_GLOBAL, Attributes = 0;
    bool repeat = false;
    bool IsAutoRepeatRangedSpell() const { return repeat; }
};
struct GlobalCooldown {
    uint32 duration, cast_time;
    GlobalCooldown(uint32 d=0, uint32 t=0) : duration(d),cast_time(t) {}
};
using GlobalCooldownList = std::unordered_map<uint32, GlobalCooldown>;
struct GlobalCooldownMgr {
    GlobalCooldownList m_GlobalCooldowns;
    bool HasGlobalCooldown(SpellEntry const*) const;
    void AddGlobalCooldown(uint32, uint32);
};
class Spell;
struct Unit {
    Spell* m_currentSpells[CURRENT_MAX_SPELL]{};
    uint32 timer = 0, shots = 0, interrupts = 0;
    bool moving = false, ready = true;
    GlobalCooldownMgr gcd;
    Spell* GetCurrentSpell(CurrentSpellTypes t) { return m_currentSpells[t]; }
    bool IsSpellReady(uint32) const { return ready; }
    void InterruptSpell(CurrentSpellTypes t) { m_currentSpells[t] = nullptr; ++interrupts; }
    bool IsPlayer() const { return true; }
    bool IsMoving() const { return moving; }
    bool IsNonMeleeSpellCasted(bool, bool, bool) const { return m_currentSpells[CURRENT_GENERIC_SPELL]; }
    bool m_AutoRepeatFirstCast = false;
    uint32 GetAttackTimer(int) const { return timer; }
    void SetAttackTimer(int, uint32 v) { timer = v; }
    bool IsAttackReady(int) const { return !timer; }
    void ResetAttackTimer(int) { timer = 2000; }
    GlobalCooldownMgr& GetGlobalCooldownMgr() { return gcd; }
    void SetStandState(int) {}
    void _UpdateAutoRepeatSpell();
    void Advance(uint32 ms) { WorldTimer::now += ms; timer = timer > ms ? timer-ms : 0; }
};
struct Targets {};
class Spell {
public:
    Unit* caster;
    SpellEntry const* m_spellInfo;
    Targets m_targets;
    Spell(Unit* u, SpellEntry const* s, bool = false) : caster(u), m_spellInfo(s) {}
    bool HasGlobalCooldown() const { return caster->gcd.HasGlobalCooldown(m_spellInfo); }
    bool CanBeInterrupted() const { return true; }
    void UpdatePointers() {}
    SpellCastResult CheckCast(bool strict);
    void prepare(Targets const&) { ++caster->shots; delete this; }
};
struct PlayerbotAI {
    Unit* bot;
    bool known = true, allowed = true;
    uint32 interruptedId = 0;
    bool HasSpell(uint32 id) { return id && known; }
    bool IsPlayerMasterSpellAllowed(SpellEntry const*, Unit*) { return allowed; }
    void SpellInterrupted(uint32 id) { interruptedId = id; }
};
// Heal-qualified cancellation is exercised by playerbot-party-healing.
struct HealTargetFullHealthTrigger { explicit HealTargetFullHealthTrigger(PlayerbotAI*){} bool IsActive(){return false;} };
struct ServerFacade {
    std::unordered_map<uint32, SpellEntry*> spells;
    SpellEntry const* LookupSpellInfo(uint32 id) { auto i=spells.find(id); return i==spells.end() ? nullptr : i->second; }
} sServerFacade;
struct { uint32 globalCoolDown = 1500, reactDelay = 100; } sPlayerbotAIConfig;
struct Event {};
struct NextAction {
    std::string name;
    explicit NextAction(std::string n) : name(n) {}
    static NextAction** array(int, NextAction* one, void*) { return new NextAction*[2]{one,nullptr}; }
    static NextAction** merge(NextAction** a, NextAction** b) {
        std::vector<NextAction*> items;
        for (auto list : {a,b}) if(list) { for(int i=0;list[i];++i) items.push_back(list[i]); delete[] list; }
        auto result = new NextAction*[items.size()+1]{};
        std::copy(items.begin(),items.end(),result); return result;
    }
};
struct Qualified {
    std::string qualifier;
    std::string getQualifier() const { return qualifier; }
    static std::string MultiQualify(std::vector<std::string> const& parts, std::string const& sep) {
        std::string out="{"; for(auto const& p:parts) { if(out!="{") out+=sep; out+=p; } return out+"}";
    }
};
class Action {
public:
    PlayerbotAI* ai; Unit* bot; uint32 duration = 0;
    Action(PlayerbotAI* a, std::string = "") : ai(a),bot(a->bot) {}
    virtual ~Action() = default;
    virtual bool isUseful() { return true; }
    virtual bool Execute(Event&) { return true; }
    NextAction** getPrerequisites() { return nullptr; }
    void SetDuration(uint32 v) { duration=v; }
};
class CastSpellAction : public Action {
public:
    uint32 spellId;
    Unit* target;
    std::string reach="reach spell", targetName="current target", spellName="smite";
    CastSpellAction(PlayerbotAI* a, uint32 id, Unit* t) : Action(a),spellId(id),target(t) {}
    Unit* GetTarget() { return target; }
    std::string GetReachActionName() { return reach; }
    std::string GetTargetName() { return targetName; }
    std::string GetSpellName() { return spellName; }
    std::string GetTargetQualifier() { return ""; }
    NextAction** getPrerequisites();
};
#include "NativeInterruptClass.inc"
#include "NativeWand.inc"

void Require(bool b, char const* message) { if(!b) throw std::runtime_error(message); }
bool HasWandPrerequisite(CastSpellAction& cast, bool requireReach=true) {
    NextAction** list=cast.getPrerequisites(); bool wand=false,reach=false;
    if(list) { for(int i=0;list[i];++i) { wand |= list[i]->name=="interrupt current spell::wand"; reach |= list[i]->name.find("reach spell::")==0; delete list[i]; } delete[] list; }
    if(requireReach) Require(reach,"Native reach prerequisite lost");
    return wand;
}
int main() {
    try {
        SpellEntry wand{5019,SPELLCATEGORY_RANGED_WEAPON,0,0,true};
        SpellEntry heal{2061}, damage{984}, hunter{75,0,0,0,true};
        sServerFacade.spells={{5019,&wand},{2061,&heal},{984,&damage},{75,&hunter}};
        Unit bot, patient; PlayerbotAI ai{&bot}; Spell active(&bot,&wand), healing(&bot,&heal);
        bot.m_currentSpells[CURRENT_AUTOREPEAT_SPELL]=&active;
        // Actual native wand update renews spell GCD before each eligibility
        // probe. Priority alone never makes the spell possible in this loop.
        for(int i=0;i<6;++i) {
            bot._UpdateAutoRepeatSpell();
            Require(healing.CheckCast(true)==SPELL_FAILED_NOT_READY,"Continuous wand must block native strict cast check");
            bot.Advance(2000);
        }
        Require(bot.shots==6,"Native wand update did not run");
        bot._UpdateAutoRepeatSpell();
        CastSpellAction cast(&ai,2061,&patient);
        Require(HasWandPrerequisite(cast),"Heal must interrupt wand before cast checks");
        cast.spellId=984;
        Require(HasWandPrerequisite(cast),"Allowed DPS spell must also preempt wand");
        ai.allowed=false;
        Require(!HasWandPrerequisite(cast),"Mana/target-policy rejection must not stop shooting"); ai.allowed=true;
        ai.known=false;
        Require(!HasWandPrerequisite(cast),"Unknown spell must retain alternatives"); ai.known=true;
        bot.ready=false;
        Require(!HasWandPrerequisite(cast),"Spell on cooldown must not stop shooting"); bot.ready=true;
        cast.spellId=0;
        Require(!HasWandPrerequisite(cast),"Unresolved spell must retain alternatives");
        cast.spellId=5019;
        Require(!HasWandPrerequisite(cast),"Wand must not interrupt itself");
        cast.spellId=2061; cast.target=nullptr;
        Require(!HasWandPrerequisite(cast),"Missing target must not stop shooting"); cast.target=&patient;
        cast.targetName="self target";
        Require(HasWandPrerequisite(cast,false),"Self heal must stop shooting too");
        Event event; InterruptCurrentSpellAction stop(&ai); stop.qualifier="wand";
        Require(stop.isUseful() && stop.Execute(event),"Wand prerequisite failed");
        Require(ai.interruptedId==5019 && !bot.GetCurrentSpell(CURRENT_AUTOREPEAT_SPELL),"Wrong spell interrupted");
        Require(healing.CheckCast(true)==SPELL_FAILED_NOT_READY,"Interrupt must not bypass existing GCD");
        bot.Advance(stop.duration);
        Require(healing.CheckCast(true)==SPELL_CAST_OK,"Heal must become eligible after wand stops and GCD expires");
        Require(!stop.isUseful() && !stop.Execute(event),"Repeated interrupt must be harmless");
        Spell bow(&bot,&hunter); bot.m_currentSpells[CURRENT_AUTOREPEAT_SPELL]=&bow;
        Require(!HasWandPrerequisite(cast,false) && !stop.isUseful() && !stop.Execute(event),"Hunter auto shot must be unchanged");
        bot.m_currentSpells[CURRENT_AUTOREPEAT_SPELL]=nullptr;
        bot.m_currentSpells[CURRENT_GENERIC_SPELL]=&healing;
        Require(!stop.Execute(event) && bot.GetCurrentSpell(CURRENT_GENERIC_SPELL)==&healing,"Wand-only interrupt must preserve active heal");
        InterruptCurrentSpellAction ordinary(&ai);
        Require(ordinary.isUseful() && ordinary.Execute(event),"Existing unqualified interrupt changed");
        std::cout << "Native wand GCD starvation and handoff checks passed\n";
    } catch(std::exception const& e) { std::cerr << e.what() << '\n'; return 1; }
}
