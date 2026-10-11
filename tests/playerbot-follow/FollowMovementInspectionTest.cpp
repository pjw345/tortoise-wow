// Real targeted-generator state/accessors, facade inspection and follow-dispatch
// boundary; deterministic motion/follower services, no navmesh simulation.
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
using uint32 = uint32_t;
constexpr float M_PI_F = 3.14159265358979323846f;
enum MovementGeneratorType { IDLE_MOTION_TYPE, CHASE_MOTION_TYPE, FOLLOW_MOTION_TYPE, POINT_MOTION_TYPE };
struct Unit;
struct Player;
struct Creature;
struct MovementGenerator
{
    virtual ~MovementGenerator() = default;
    virtual Unit* GetCurrentTarget() const { return nullptr; }
    virtual MovementGeneratorType GetMovementGeneratorType() const = 0;
};
template<class T, class D> struct MovementGeneratorMedium : MovementGenerator {};
struct FollowerReference
{
    Unit* target = nullptr;
    void link(Unit* value, void*) { target = value; }
    Unit* getTarget() const { return target; }
    void invalidate() { target = nullptr; }
};
struct ShortTimeTracker { explicit ShortTimeTracker(int) {} };
#include "NativeTargetedState.inc"
template<class T> struct FollowMovementGenerator : TargetedMovementGeneratorMedium<T, FollowMovementGenerator<T>>
{
    FollowMovementGenerator(Unit& target, float offset, float angle)
        : TargetedMovementGeneratorMedium<T, FollowMovementGenerator<T>>(target, offset, angle) {}
    MovementGeneratorType GetMovementGeneratorType() const override { return FOLLOW_MOTION_TYPE; }
    void InvalidateTarget() { this->i_target.invalidate(); }
};
template<class T> struct ChaseMovementGenerator : TargetedMovementGeneratorMedium<T, ChaseMovementGenerator<T>>
{
    ChaseMovementGenerator(Unit& target, float offset, float angle)
        : TargetedMovementGeneratorMedium<T, ChaseMovementGenerator<T>>(target, offset, angle) {}
    MovementGeneratorType GetMovementGeneratorType() const override { return CHASE_MOTION_TYPE; }
};
struct OtherMovement : MovementGenerator
{
    MovementGeneratorType GetMovementGeneratorType() const override { return POINT_MOTION_TYPE; }
};
struct MotionMaster
{
    Unit* owner;
    std::unique_ptr<MovementGenerator> current;
    unsigned launches = 0;
    MovementGenerator const* GetCurrent() const { return current.get(); }
    MovementGeneratorType GetCurrentMovementGeneratorType() const
    { return current ? current->GetMovementGeneratorType() : IDLE_MOTION_TYPE; }
    void MoveFollow(Unit* target, float offset, float angle, bool, bool);
};
struct Unit
{
    bool player;
    Unit* victim = nullptr;
    float radius = 0.5f;
    MotionMaster mm{this};
    explicit Unit(bool isPlayer) : player(isPlayer) {}
    bool IsPlayer() const { return player; }
    bool IsCreature() const { return !player; }
    Unit* GetVictim() const { return victim; }
    float GetObjectBoundingRadius() const { return radius; }
    MotionMaster* GetMotionMaster() { return &mm; }
};
struct Player : Unit { Player() : Unit(true) {} };
struct Creature : Unit { Creature() : Unit(false) {} };
void MotionMaster::MoveFollow(Unit* target, float offset, float angle, bool, bool)
{
    ++launches;
    if (owner->IsPlayer()) current = std::make_unique<FollowMovementGenerator<Player>>(*target, offset, angle);
    else current = std::make_unique<FollowMovementGenerator<Creature>>(*target, offset, angle);
}
struct ServerFacade
{
    Unit* GetChaseTarget(Unit*);
    float GetChaseAngle(Unit*);
    float GetChaseOffset(Unit*);
    bool IsFollowing(Unit*, Unit*, float, float);
} sServerFacade;
#include "NativeMovementInspection.inc"
struct Config { bool boostFollow = false; } sPlayerbotAIConfig;
bool RequestFollow(Unit* bot, Unit* target, float formationDistance, float angle)
{
    auto& mm = *bot->GetMotionMaster();
    float distance = formationDistance <= target->GetObjectBoundingRadius() ? 0.0f : formationDistance - target->GetObjectBoundingRadius();
#include "NativeFollowDispatch.inc"

unsigned checks = 0;
void Check(bool condition, const char* message)
{
    ++checks;
    if (!condition) throw std::runtime_error(message);
}
void CheckOwner(Unit& owner)
{
    Player master, replacement;
    Creature enemy;
    owner.victim = &enemy;
    Check(sServerFacade.GetChaseTarget(&owner) == nullptr, "empty movement must not use combat victim");
    Check(sServerFacade.GetChaseAngle(&owner) == 0 && sServerFacade.GetChaseOffset(&owner) == 0, "empty geometry");
    Check(RequestFollow(&owner, &master, 1.5f, M_PI_F), "first follow launches");
    Check(sServerFacade.GetChaseTarget(&owner) == &master, "follow target differs from victim");
    Check(sServerFacade.GetChaseAngle(&owner) == M_PI_F, "native angle");
    Check(sServerFacade.GetChaseOffset(&owner) == 1.0f, "native offset excludes radius");
    for (int i=0; i<30; ++i)
        Check(!RequestFollow(&owner, &master, 1.5f + (i%2 ? 0.001f : 0.0f), M_PI_F + 0.001f), "repeated ticks must retain route");
    Check(owner.mm.launches == 1, "one spline launch across repeated ticks");
    Check(!RequestFollow(&owner, &master, 1.5f, -M_PI_F), "angle wrap retains route");
    Check(!sServerFacade.IsFollowing(&owner, &replacement, 1.5f, M_PI_F), "different master not matching");
    Check(RequestFollow(&owner, &replacement, 1.5f, M_PI_F), "changed master replaces route");
    Check(RequestFollow(&owner, &replacement, 2.0f, M_PI_F), "changed formation distance replaces route");
    Check(RequestFollow(&owner, &replacement, 2.0f, 0.5f), "changed formation angle replaces route");
    owner.mm.current = std::make_unique<OtherMovement>();
    Check(sServerFacade.GetChaseTarget(&owner) == nullptr, "point movement must not expose victim or suspended follow");
    Check(sServerFacade.GetChaseAngle(&owner) == 0 && sServerFacade.GetChaseOffset(&owner) == 0, "point geometry safe defaults");
    Check(RequestFollow(&owner, &master, 0.25f, 0.5f), "point movement switches to follow");
    Check(sServerFacade.GetChaseOffset(&owner) == 0, "inside target radius clamps to zero");
    Check(!RequestFollow(&owner, &master, 0.25f, 0.5f), "clamped route retained");
    if (owner.IsPlayer()) static_cast<FollowMovementGenerator<Player>*>(owner.mm.current.get())->InvalidateTarget();
    else static_cast<FollowMovementGenerator<Creature>*>(owner.mm.current.get())->InvalidateTarget();
    Check(sServerFacade.GetChaseTarget(&owner) == nullptr, "invalidated follower reference yields no target");
    Check(!sServerFacade.IsFollowing(&owner, &master, 0.25f, 0.5f), "invalidated target cannot match");
    if (owner.IsPlayer()) owner.mm.current = std::make_unique<ChaseMovementGenerator<Player>>(enemy, 3.0f, 0.25f);
    else owner.mm.current = std::make_unique<ChaseMovementGenerator<Creature>>(enemy, 3.0f, 0.25f);
    owner.victim = &master;
    Check(sServerFacade.GetChaseTarget(&owner) == &enemy, "chase target comes from generator");
    Check(sServerFacade.GetChaseOffset(&owner) == 3.0f && sServerFacade.GetChaseAngle(&owner) == 0.25f, "chase geometry preserved");
    Check(!sServerFacade.IsFollowing(&owner, &enemy, 3.5f, 0.25f), "chase is not follow even with matching geometry");
}
int main()
{
    try
    {
        Player player; Creature creature;
        CheckOwner(player); CheckOwner(creature);
        Check(sServerFacade.GetChaseTarget(nullptr) == nullptr, "null unit target");
        Check(sServerFacade.GetChaseAngle(nullptr) == 0 && sServerFacade.GetChaseOffset(nullptr) == 0, "null unit geometry");
        Check(!sServerFacade.IsFollowing(nullptr, &player, 1, 1), "null follower");
        Check(!sServerFacade.IsFollowing(&player, nullptr, 1, 1), "null follow target");
        std::cout << checks << " native follow boundary checks passed\n";
    }
    catch (std::exception const& error) { std::cerr << error.what() << '\n'; return 1; }
}
