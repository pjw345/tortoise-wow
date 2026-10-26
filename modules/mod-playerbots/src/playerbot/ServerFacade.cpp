
#include "playerbot/playerbot.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"

#include "Database/DatabaseEnv.h"
#include "PlayerbotAI.h"

#include "Movement/TargetedMovementGenerator.h"
#include <cmath>

ServerFacade::ServerFacade() {}
ServerFacade::~ServerFacade() {}

float ServerFacade::GetDistance2d(Unit *unit, WorldObject* wo)
{
    if (!unit || !wo)
        return false;

    float dist =
#ifdef MANGOS
    unit->GetDistance2d(wo);
#endif
#ifdef CMANGOS
    sqrt(unit->GetDistance2d(wo->GetPositionX(), wo->GetPositionY(), DIST_CALC_NONE));
#endif
    return round(dist * 10.0f) / 10.0f;
}

float ServerFacade::GetDistance2d(Unit *unit, float x, float y)
{
    float dist =
#ifdef MANGOS
    unit->GetDistance2d(x, y);
#endif
#ifdef CMANGOS
    sqrt(unit->GetDistance2d(x, y, DIST_CALC_NONE));
#endif
    return round(dist * 10.0f) / 10.0f;
}

bool ServerFacade::IsDistanceLessThan(float dist1, float dist2)
{
    return dist1 - dist2 < sPlayerbotAIConfig.targetPosRecalcDistance;
}

bool ServerFacade::IsDistanceGreaterThan(float dist1, float dist2)
{
    return dist1 - dist2 > sPlayerbotAIConfig.targetPosRecalcDistance;
}

bool ServerFacade::IsDistanceGreaterOrEqualThan(float dist1, float dist2)
{
    return !IsDistanceLessThan(dist1, dist2);
}

bool ServerFacade::IsDistanceLessOrEqualThan(float dist1, float dist2)
{
    return !IsDistanceGreaterThan(dist1, dist2);
}

void ServerFacade::SetFacingTo(Unit* unit, float angle, bool force)
{
    MotionMaster &mm = *unit->GetMotionMaster();
    if (!force && !unit->IsStopped()) unit->SetFacingTo(angle);
    else
    {
        unit->SetOrientation(angle);
        unit->SendHeartBeat();
    }
    //unit->m_movementInfo.RemoveMovementFlag(MovementFlags(MOVEFLAG_SPLINE_ENABLED | MOVEFLAG_FORWARD));
}

bool ServerFacade::IsFriendlyTo(Unit* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsFriendlyTo(to);
#endif
#ifdef CMANGOS
    return bot->IsFriend(to);
#endif
}

bool ServerFacade::IsHostileTo(Unit* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsHostileTo(to);
#endif
#ifdef CMANGOS
    return bot->IsEnemy(to);
#endif
}

bool ServerFacade::IsFriendlyTo(WorldObject* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsFriendlyTo(to);
#endif
#ifdef CMANGOS
    return bot->IsFriend(to);
#endif
}

bool ServerFacade::IsHostileTo(WorldObject* bot, Unit* to)
{
#ifdef MANGOS
    return bot->IsHostileTo(to);
#endif
#ifdef CMANGOS
    return bot->IsEnemy(to);
#endif
}


bool ServerFacade::IsSpellReady(Player* bot, uint32 spell, uint32 itemId)
{
#ifdef MANGOS
    return !bot->HasSpellCooldown(spell);
#endif
#ifdef CMANGOS
    if (itemId)
    {
        const ItemPrototype* proto = sObjectMgr.GetItemPrototype(itemId);
        return bot->IsSpellReady(spell, proto);
    }
    else
        return bot->IsSpellReady(spell);
#endif
}



bool ServerFacade::IsUnderwater(Unit *unit)
{
#ifdef MANGOS
    return unit->IsUnderWater();
#endif
#ifdef CMANGOS
    return unit->IsUnderwater();
#endif
}

FactionTemplateEntry const* ServerFacade::GetFactionTemplateEntry(Unit *unit)
{
#ifdef MANGOS
    return unit->getFactionTemplateEntry();
#endif
#ifdef CMANGOS
    return unit->GetFactionTemplateEntry();
#endif
}

// Inspect only the active native generator, never the combat victim or a
// suspended generator underneath an unrelated movement request.
template<typename Read>
static float ReadTargetedMovement(Unit* unit, Read read)
{
    if (!unit)
        return 0.0f;

    MovementGenerator const* generator = unit->GetMotionMaster()->GetCurrent();
    if (!generator)
        return 0.0f;

    if (generator->GetMovementGeneratorType() == FOLLOW_MOTION_TYPE)
    {
        if (unit->IsPlayer())
            return read(static_cast<FollowMovementGenerator<Player> const*>(generator));
        if (unit->IsCreature())
            return read(static_cast<FollowMovementGenerator<Creature> const*>(generator));
    }
    else if (generator->GetMovementGeneratorType() == CHASE_MOTION_TYPE)
    {
        if (unit->IsPlayer())
            return read(static_cast<ChaseMovementGenerator<Player> const*>(generator));
        if (unit->IsCreature())
            return read(static_cast<ChaseMovementGenerator<Creature> const*>(generator));
    }
    return 0.0f;
}

Unit* ServerFacade::GetChaseTarget(Unit* unit)
{
    if (!unit)
        return nullptr;
    MovementGenerator const* generator = unit->GetMotionMaster()->GetCurrent();
    if (!generator || (generator->GetMovementGeneratorType() != FOLLOW_MOTION_TYPE &&
        generator->GetMovementGeneratorType() != CHASE_MOTION_TYPE))
        return nullptr;
    return generator->GetCurrentTarget();
}

float ServerFacade::GetChaseAngle(Unit* unit)
{
    return ReadTargetedMovement(unit, [](auto generator) { return generator->GetAngle(); });
}

float ServerFacade::GetChaseOffset(Unit* unit)
{
    return ReadTargetedMovement(unit, [](auto generator) { return generator->GetOffset(); });
}

bool ServerFacade::IsFollowing(Unit* unit, Unit* target, float distance, float angle)
{
    if (!unit || !target || unit->GetMotionMaster()->GetCurrentMovementGeneratorType() != FOLLOW_MOTION_TYPE ||
        GetChaseTarget(unit) != target)
        return false;

    // Follow() converts the formation's centre distance to the native offset.
    // Coordinate-derived formations incur small float rounding errors; those
    // must not replace a route around a corner on every AI tick.
    float offset = distance <= target->GetObjectBoundingRadius() ? 0.0f : distance - target->GetObjectBoundingRadius();
    return std::fabs(GetChaseOffset(unit) - offset) <= 0.01f &&
        std::fabs(std::remainder(GetChaseAngle(unit) - angle, 2.0f * M_PI_F)) <= 0.01f;
}

bool ServerFacade::isMoving(Unit *unit)
{
#ifdef MANGOS
    return unit->m_movementInfo.HasMovementFlag(movementFlagsMask);
#endif
#ifdef CMANGOS
#ifdef MANGOSBOT_ZERO
    // Turtle's direct player splines can move with UNIT_STAT_MOVING unset.
    // Match the native movement flags used by spell validation, not the legacy
    // movement-generator state queried by IsStopped(). Keep falling explicit.
    return unit->IsMoving() || unit->IsFalling();
#elif defined(MANGOSBOT_ONE)
    return !unit->IsStopped() || unit->IsFalling() || unit->IsJumping();
#else
    return !unit->IsStopped() || unit->IsFalling();
#endif
#endif
}
