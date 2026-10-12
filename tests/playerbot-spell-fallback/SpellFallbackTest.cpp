#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
constexpr float ATTACK_DISTANCE = 5.0f;
constexpr int DIST_CALC_COMBAT_REACH = 0, DIST_CALC_COMBAT_REACH_WITH_MELEE = 1;
constexpr int SPELL_RANGE_IDX_COMBAT = 1, SPELL_RANGE_IDX_SELF_ONLY = 2, SPELL_RANGE_IDX_ANYWHERE = 3;
struct SpellEntry { int rangeIndex = SPELL_RANGE_IDX_ANYWHERE; };
struct Unit {
    bool mounted = false, combat = false, world = true, melee = true;
    float distance = 10.0f;
    int map = 0;
    bool IsMounted() const { return mounted; }
    bool IsInCombat() const { return combat; }
    bool IsInWorld() const { return world; }
    int GetMapId() const { return map; }
    float GetDistance(Unit*, bool, int) const { return distance; }
    bool CanReachWithMeleeAttack(Unit*) const { return melee; }
};
struct Facade {
    SpellEntry entry;
    bool exists = true;
    SpellEntry const* LookupSpellInfo(unsigned id) const { return id && exists ? &entry : nullptr; }
} sServerFacade;
struct { float contactDistance = 0.5f; } sPlayerbotAIConfig;
struct AI {
    std::set<unsigned> learned;
    bool useful = true, vehicle = false, vehicleAllows = true, policy = true, cast = true;
    int capabilityCalls = 0, castCalls = 0;
    bool HasSpell(unsigned id) { ++capabilityCalls; return learned.count(id) != 0; }
    bool IsInVehicle() const { return vehicle; }
    bool IsInVehicle(bool, bool, bool) const { return vehicleAllows; }
    bool IsPlayerMasterSpellAllowed(SpellEntry const*,Unit*) const { return policy; }
    bool IsRanged(Unit*) const { return true; }
    bool GetSpellRange(std::string const&,float* max,float* min) const { *max = 40; *min = 0; return true; }
    bool CanCastSpell(std::string const&, Unit*, int, void*, bool) { ++castCalls; return cast; }
    void Unmount() {}
};
struct CastSpellAction {
    AI* ai;
    Unit* bot;
    Unit* target;
    std::string spellName = "greater heal";
    unsigned spellId = 2060;
    float range = 40;
    bool isUseful();
    bool isPossible();
    Unit* GetTarget() const { return target; }
    std::string const& GetSpellName() const { return spellName; }
};
#define AI_VALUE2(type, name, qualifier) (ai->useful)
#include "NativeSpellAction.inc"
void check(bool condition,char const* message) { if (!condition) throw std::runtime_error(message); }
int main() {
    AI ai;
    Unit bot,target;
    CastSpellAction action{&ai,&bot,&target};
    // Native Engine queues alternatives for IMPOSSIBLE but not USELESS.
    // A priest that knows Heal but not Greater Heal must reach that branch.
    check(action.isUseful(), "unlearned higher heal must not discard alternatives");
    check(!action.isPossible(), "unlearned spell must be impossible");
    check(ai.castCalls == 0, "capability miss cannot attempt a cast");
    action.spellId = 0;
    check(action.isUseful() && !action.isPossible(), "unresolved spell must preserve alternatives");
    action.spellId = 2060;
    sServerFacade.exists = false;
    check(action.isUseful() && !action.isPossible(), "missing spell data must preserve alternatives");
    sServerFacade.exists = true;
    ai.learned.insert(2060);
    check(action.isUseful() && action.isPossible(), "learned spell remains usable");
    ai.useful = false;
    check(!action.isUseful(), "native current-cast usefulness retained");
    ai.useful = true;
    action.target = nullptr;
    check(!action.isUseful() && !action.isPossible(), "missing target is still rejected");
    action.target = &target;
    target.world = false;
    check(!action.isUseful(), "out-of-world target is rejected");
    target.world = true; target.map = 1;
    check(!action.isUseful(), "off-map target is rejected");
    target.map = 0;
    ai.vehicle = true; ai.vehicleAllows = false;
    check(!action.isUseful(), "vehicle restriction retained");
    ai.vehicle = false; ai.policy = false;
    check(!action.isPossible(), "master target policy retained");
    ai.policy = true; bot.distance = 50;
    check(!action.isPossible(), "range restriction retained");
    bot.distance = 10;
    action.spellName = "mount"; action.spellId = 0; action.target = &bot;
    check(action.isUseful() && action.isPossible(), "dynamic mount has no fixed capability requirement");
    std::cout << "Native spell capability fallback tests passed\n";
}
