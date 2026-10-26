
#include "playerbot/playerbot.h"
#include "LootTriggers.h"
#include "playerbot/LootObjectStack.h"
#include "playerbot/PlayerbotAIConfig.h"

#include "playerbot/ServerFacade.h"
using namespace ai;

namespace
{
    bool IsCorpseOutsideMasterLootRange(PlayerbotAI* ai, Player* bot, LootObject const& loot)
    {
        Player* master = ai->GetMaster();
        Creature* corpse = ai->GetCreature(loot.guid);
        if (!master || master == bot || !corpse || sServerFacade.GetDeathState(corpse) != CORPSE)
            return false;

        // Match LootAction::Execute's existing companion corpse admission.
        float range = sPlayerbotAIConfig.lootDistance;
        if (bot->GetGroup() && !ai->IsGroupLeader())
            range = ai->HasActivePlayerMaster() ?
                sPlayerbotAIConfig.groupMemberLootDistanceWithActiveMaster :
                sPlayerbotAIConfig.groupMemberLootDistance;
        return sServerFacade.GetDistance2d(master, corpse) > range;
    }
}

bool LootAvailableTrigger::IsActive()
{
    if (!AI_VALUE(bool, "has available loot") ||
        AI_VALUE2(bool, "combat", "self target") ||
        AI_VALUE2(bool, "mounted", "self target"))
        return false;

    LootObject current = AI_VALUE(LootObject, "loot target");
    // A selected target that cannot proceed must not strand another queued
    // candidate. Native selection still rechecks eligibility and approach safety.
    return !current.IsLootPossible(bot) || IsCorpseOutsideMasterLootRange(ai, bot, current) ||
        sServerFacade.IsDistanceLessOrEqualThan(AI_VALUE2(float, "distance", "loot target"), INTERACTION_DISTANCE) ||
        AI_VALUE(std::list<ObjectGuid>, "all targets").empty();
}

bool FarFromCurrentLootTrigger::IsActive()
{
    LootObject loot = AI_VALUE(LootObject, "loot target");

    if (!loot.IsLootPossible(bot))
        return false;

    // The movement trigger must accept the same configured corpse range as
    // selection; a follow-distance gate can select a target but never approach it.
    if (IsCorpseOutsideMasterLootRange(ai, bot, loot))
        return false;

    // Must agree with OpenLootAction::DoLoot's loot-range rule, or the bot deadlocks: for a
    // creature corpse the server validates loot with a 3D distance check (Player::SendLoot),
    // but the plain "distance" value here is 2D and ignores Z. On sloped ground the bot read
    // "close enough" (2D <= 5) and so never fired "move to loot", yet "open loot" failed the
    // 3D gate -> it sat a few yards off a corpse it could not loot. Use the same 3D rule so
    // the bot keeps approaching until it is genuinely on the corpse, then loots.
    if (Creature* creature = GetBotAI(bot)->GetCreature(loot.guid))
    {
        if (sServerFacade.GetDeathState(creature) == CORPSE)
            return !creature->IsWithinDistInMap(bot, bot->GetMaxLootDistance(creature), true, SizeFactor::None);
    }

    return AI_VALUE2(float, "distance", "loot target") > INTERACTION_DISTANCE;
}

bool CanLootTrigger::IsActive()
{
    return AI_VALUE(bool, "can loot");
}
