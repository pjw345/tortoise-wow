
#include "playerbot/playerbot.h"
#include "WaitForAttackAction.h"
#include "playerbot/strategy/generic/CombatStrategy.h"

using namespace ai;

bool WaitForAttackKeepSafeDistanceAction::Execute(Event& event)
{
    Unit* target = AI_VALUE(Unit*, "current target");

    if (target && !target->IsStopped() && target->GetTarget() && target->GetTarget()->IsStopped())
        target = target->GetTarget();


    if (target && target->IsAlive())
    {
        const float safeDistance = WaitForAttackStrategy::GetSafeDistance();
        const float safeDistanceThreshold = WaitForAttackStrategy::GetSafeDistanceThreshold();

        // Waiting bots only need to create space when they are inside the
        // configured pull distance. Do not pull a bot that is already farther
        // away back toward the hostile target.
        if (WorldPosition(bot).fDist(WorldPosition(target)) > safeDistance)
            return false;

        // Find the best point around the target.
        const WorldPosition bestPoint = GetBestPoint(target, (safeDistance - safeDistanceThreshold), safeDistance);
        if (bestPoint)
        {
            // Move to the best point
            bool success = MoveTo(bestPoint.getMapId(), bestPoint.getX(), bestPoint.getY(), bestPoint.getZ(), false, false, false, true);
            if (success)
                WaitForReach(WorldPosition(bot).fDist(bestPoint));
            return success;
        }
    }

    return false;
}

const ai::WorldPosition WaitForAttackKeepSafeDistanceAction::GetBestPoint(Unit* target, float minDistance, float maxDistance) const
{
    const Map* map = target->GetMap();
    const WorldPosition botPosition(bot);
    const WorldPosition targetPosition(target);
    const int8 startDir = urand(0, 1) * 2 - 1;
    const float radiansIncrement = (5.0f / 180.0f) * (M_PI_F);
    const float startAngle = targetPosition.getAngleTo(botPosition) + urand(0.f,radiansIncrement) * startDir;
    const float distance = frand(minDistance, maxDistance);
    const std::list<ObjectGuid> enemies = AI_VALUE(std::list<ObjectGuid>, "possible targets no los");

    if (ai->HasStrategy("debug move", BotState::BOT_STATE_COMBAT))
    {
        for (uint32 dist = 0; dist < distance; dist++)
        {
            WorldPosition point = targetPosition + WorldPosition(0, dist * cos(startAngle), dist * sin(startAngle), 1.0f);
            Creature* wpCreature = bot->SummonCreature(1, point.getX(), point.getY(), point.getZ(), 0.0f, TEMPSPAWN_TIMED_DESPAWN, 1000.0f + dist * 100.0f);
        }
    }

    std::list<WorldPosition> points;

    for (float tryAngle = 0.0f; tryAngle < M_PI_F; tryAngle += radiansIncrement)
    {
        for (int8 tryDir = -1; tryAngle && tryDir < 1; tryDir += 2)
        {
            float pointAngle = startAngle;
            pointAngle += tryAngle * startDir * tryDir;

            WorldPosition point = targetPosition + WorldPosition(0, distance * cos(pointAngle), distance * sin(pointAngle), 1.0f);

            point.setZ(point.getHeight());

            if (ai->HasStrategy("debug move", BotState::BOT_STATE_COMBAT))
            {
                Creature* wpCreature = bot->SummonCreature(1, point.getX(), point.getY(), point.getZ(), 0.0f, TEMPSPAWN_TIMED_DESPAWN, 5000.0f + tryAngle * 1000.0f);
            }

            // Check if the target is visible from the point
            if (!target->IsWithinLOS(point.getX(), point.getY(), point.getZ() + bot->GetCollisionHeight()))
                continue;

            // Check if the point is not surrounded by other enemies
            if (IsEnemyClose(point, enemies))
                continue;

            // Check if the bot can move to this point.
            if (!botPosition.canPathTo(point,bot))
                continue;

            if (ai->HasStrategy("debug move", BotState::BOT_STATE_COMBAT))
            {
                Creature* wpCreature = bot->SummonCreature(15631, point.getX(), point.getY(), point.getZ(), 0.0f, TEMPSPAWN_TIMED_DESPAWN, 5000.0f + tryAngle * 1000.0f);
            }

            points.push_back(point);
        }
    }

    // Prefer the valid point requiring the least movement. This prevents the
    // angular scan order from making a bot cross the pull area unnecessarily.
    if (!points.empty())
    {
        auto point = std::min_element(points.begin(), points.end(),
            [botPosition](const WorldPosition& left, const WorldPosition& right)
            {
                return botPosition.fDist(left) < botPosition.fDist(right);
            });
        if (point != points.end())
            return *point;
    }

    return botPosition;
}

bool WaitForAttackKeepSafeDistanceAction::IsEnemyClose(const WorldPosition& point, const std::list<ObjectGuid>& enemies) const
{
    for (const ObjectGuid& enemyGUID : enemies)
    {
        Unit* enemy = ai->GetUnit(enemyGUID);
        if (enemy && enemy->CanAttackOnSight(bot))
        {
            // The candidate list deliberately includes targets without line
            // of sight. A wall hiding an enemy from the bot does not make the
            // destination on the enemy's side of that wall safe.
            const float enemyAttackRange = enemy->GetAttackDistance(bot) + ATTACK_DISTANCE;
            const float distanceToPoint = WorldPosition(enemy).sqDistance(point);
            if (distanceToPoint <= (enemyAttackRange * enemyAttackRange))
                return true;
        }
    }

    return false;
}

bool WaitForAttackKeepSafeDistanceAction::isUseful()
{
    return MovementAction::isUseful() &&
        (!ai->HasStrategy("guard", ai->GetState()) ||
         WaitForAttackStrategy::GetSafeDistance() <= ai->GetRange("guard"));
}
