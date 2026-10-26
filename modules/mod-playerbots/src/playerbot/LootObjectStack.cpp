#include "LootObjectStack.h"
#include "playerbot.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/values/SharedValueContext.h"

using namespace ai;

#define MAX_LOOT_OBJECT_COUNT 10

bool ai::CanOpenActivatedQuestChest(Player* bot, GameObject* go)
{
    return bot && go && go->GetGoType() == GAMEOBJECT_TYPE_CHEST &&
        go->getLootState() == GO_ACTIVATED &&
        sObjectMgr.IsGameObjectForQuests(go->GetEntry()) &&
        go->ActivateToQuest(bot) &&
        go->loot.HasQuestLootForPlayer(bot);
}

namespace
{
    // Quest views are created per player, but ordinary non-FFA quest items
    // are shared: an incomplete quest does not imply remaining loot. The
    // underlying chest is still one shared game object. Choose one nearby
    // eligible bot using stable group order to avoid simultaneous OPEN_LOCK.
    // A shared take advances the turn even when another bot's quest remains
    // incomplete but that spawn no longer contains its objective item.
    Player* GetPersonalQuestChestTurn(Player* bot, GameObject* go)
    {
        if (!bot || !go || go->GetGoType() != GAMEOBJECT_TYPE_CHEST ||
            !sObjectMgr.IsGameObjectForQuests(go->GetEntry()) ||
            !go->ActivateToQuest(bot))
            return bot;

        Group* group = bot->GetGroup();
        if (!group)
            return bot;

        PlayerbotAI* ai = GetBotAI(bot);
        float maxDistance = ai->HasActivePlayerMaster() ?
            sPlayerbotAIConfig.groupMemberLootDistanceWithActiveMaster :
            sPlayerbotAIConfig.groupMemberLootDistance;

        Group::MemberSlotList const& groupSlots = group->GetMemberSlots();
        for (Group::member_citerator itr = groupSlots.begin(); itr != groupSlots.end(); ++itr)
        {
            Player* groupMember = sObjectMgr.GetPlayer(itr->guid);
            if (!groupMember || !GetBotAI(groupMember) || !groupMember->IsAlive() ||
                groupMember->GetMapId() != go->GetMapId() ||
                sServerFacade.GetDistance2d(groupMember, go) > maxDistance ||
                !go->ActivateToQuest(groupMember) ||
                (go->getLootState() == GO_ACTIVATED && !go->loot.HasQuestLootForPlayer(groupMember)))
                continue;

            return groupMember;
        }

        return bot;
    }
}

LootTarget::LootTarget(ObjectGuid guid) : guid(guid), asOfTime(time(0))
{
}
LootTarget::LootTarget(LootTarget const& other)
{
    guid = other.guid;
    asOfTime = other.asOfTime;
}

LootTarget& LootTarget::operator=(LootTarget const& other)
{
    if((void*)this == (void*)&other)
        return *this;

    guid = other.guid;
    asOfTime = other.asOfTime;

    return *this;
}

bool LootTarget::operator< (const LootTarget& other) const
{
    return guid < other.guid;
}

void LootTargetList::shrink(time_t fromTime)
{
    for (std::set<LootTarget>::iterator i = begin(); i != end(); )
    {
        if (i->asOfTime <= fromTime)
            erase(i++);
		else
			++i;
    }
}

LootObject::LootObject(Player* bot, ObjectGuid guid)
	: guid(), skillId(SKILL_NONE), reqSkillValue(0), reqItem(0)
{
    Refresh(bot, guid);
}

void LootObject::Refresh(Player* bot, ObjectGuid guid, bool debug)
{
    skillId = SKILL_NONE;
    reqSkillValue = 0;
    reqItem = 0;
    this->guid = ObjectGuid();

    PlayerbotAI* ai = GetBotAI(bot);
    Creature* creature = ai->GetCreature(guid);
    if (creature && sServerFacade.GetDeathState(creature) == CORPSE)
    {
        // Turtle sets LOOTABLE from the template even when the body roll is
        // empty. Native IsAllowedToLoot then correctly refuses body looting,
        // but the remaining skinning phase must still be classified below.
        bool hasBodyLoot = creature->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE);
#ifdef MANGOSBOT_ZERO
        hasBodyLoot = hasBodyLoot && !creature->loot.isLooted();
#endif
        if (hasBodyLoot)
        {
            // The lootable flag is set group-wide on a tapped corpse, so it alone does not mean
            // this bot may loot it. Require tap rights, otherwise the bot walks to and kneels on
            // corpses whose loot belongs to someone else (e.g. the master's round-robin kill).
            if (creature->IsTappedBy(bot))
            {
                if (debug)
                    sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=accepted type=creature reason=lootable",
                        bot->GetName(), guid.GetRawValue());

                this->guid = guid;
            }
            else if (debug)
            {
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=rejected type=creature reason=not_tapped",
                    bot->GetName(), guid.GetRawValue());
            }
        }

        // Loot the normal loot first, skin afterwards. A corpse that is both
        // lootable and skinnable used to have skillId overwritten to SKINNING
        // below, so IsLootPossible rejected the whole corpse for "not enough
        // skill" and the bot never took the normal loot it was tapped for
        // (Prairie Wolf etc.). If we are tapped for normal loot, take it now
        // with skillId left at SKILL_NONE; the corpse keeps its SKINNABLE flag
        // and, once the loot is gone (LOOTABLE clears), a later Refresh falls
        // through to the skinning branch below - so a skinner loots, then skins.
        if (!this->guid.IsEmpty())
        {
            skillId = SKILL_NONE;
            return;
        }

        if (creature->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE))
        {
            skillId = creature->GetCreatureInfo()->GetRequiredLootSkill();
            uint32 targetLevel = creature->GetLevel();
            reqSkillValue = targetLevel < 10 ? 1 : targetLevel < 20 ? (targetLevel - 10) * 10 : targetLevel * 5;
            if (ai->HasSkill((SkillType)skillId) && bot->GetSkillValue(skillId) >= reqSkillValue)
            {
                if (debug)
                    sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=accepted type=skinning reason=has_skill",
                        bot->GetName(), guid.GetRawValue());
                this->guid = guid;
            }
            else if (debug)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=rejected type=skinning reason=insufficient_skill",
                    bot->GetName(), guid.GetRawValue());
            return;
        }

        if (debug)
            sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=rejected type=creature reason=no_loot_or_skin_flag",
                bot->GetName(), guid.GetRawValue());

        return;
    }

    GameObject* go = ai->GetGameObject(guid);
    if (go && sServerFacade.isSpawned(go) &&
        (!go->IsInUse() || CanOpenActivatedQuestChest(bot, go)))
    {
        bool isQuestItemOnly = false;

#ifdef MANGOSBOT_TWO
        for (int i = 0; i < QUEST_ITEM_OBJECTIVES_COUNT; i++)
        {
            int itemId = go->GetGOInfo()->questItems[i];

            if (ItemUsageValue::IsNeededForQuest(bot, itemId))
            {
                if (debug)
                    sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=accepted type=gameobject reason=quest_item",
                        bot->GetName(), guid.GetRawValue());
                this->guid = guid;
                return;
            }
            isQuestItemOnly |= itemId > 0;
        }
#else
        /*if (!guid.IsEmpty())
        {
            for (auto& entry : GAI_VALUE2(std::list<int32>, "item drop list", -1*uint(go->GetEntry())))
            {
                if (IsNeededForQuest(bot, entry))
                {
                    this->guid = guid;
                    return;
                }
                isQuestItemOnly |= entry > 0;
            }
        }*/
#endif

        if (isQuestItemOnly)
        {
            if (debug)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=rejected type=gameobject reason=unneeded_quest_items",
                    bot->GetName(), guid.GetRawValue());
            return;
        }

        uint32 goId = go->GetGOInfo()->id;
        std::set<uint32>& skipGoLootList = ai->GetAiObjectContext()->GetValue<std::set<uint32>&>("skip go loot list")->Get();
        if (skipGoLootList.find(goId) != skipGoLootList.end())
        {
            if (debug)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=rejected type=gameobject reason=skip_list",
                    bot->GetName(), guid.GetRawValue());
            return;
        }

        uint32 lockId = go->GetGOInfo()->GetLockId();
        LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);
        if (!lockInfo)
        {
            if (debug)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=rejected type=gameobject reason=no_lock",
                    bot->GetName(), guid.GetRawValue());
            return;
        }

        for (int i = 0; i < 8; ++i)
        {
            switch (lockInfo->Type[i])
            {
                case LOCK_KEY_ITEM:
                    if (lockInfo->Index[i] > 0)
                    {
                        if (debug)
                            sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=accepted type=gameobject reason=key_lock",
                                bot->GetName(), guid.GetRawValue());
                        reqItem = lockInfo->Index[i];
                        this->guid = guid;
                    }
                    break;
                case LOCK_KEY_SKILL:
                    if (goId == 13891 || goId == 19535) // Serpentbloom
                    {
                        if (debug)
                            sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=accepted type=gameobject reason=serpentbloom",
                                bot->GetName(), guid.GetRawValue());
                        this->guid = guid;
                    }
                    else if (SkillByLockType(LockType(lockInfo->Index[i])) > 0)
                    {
                        if (debug)
                            sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=accepted type=gameobject reason=skill_lock",
                                bot->GetName(), guid.GetRawValue());
                        skillId = SkillByLockType(LockType(lockInfo->Index[i]));
                        reqSkillValue = std::max((uint32)1, lockInfo->Skill[i]);
                        this->guid = guid;
                    }
                    break;
                case LOCK_KEY_NONE:
                    if (debug)
                        sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=accepted type=gameobject reason=open_lock",
                            bot->GetName(), guid.GetRawValue());
                    this->guid = guid;
                    break;
            }
        }
    }

    if (debug && guid && !this->guid)
        sLog.outLoot("bot=\"%s\" target_guid=%lu phase=discover result=rejected type=gameobject reason=bad_lock",
            bot->GetName(), guid.GetRawValue());
}

WorldObject* LootObject::GetWorldObject(Player* bot)
{
    Refresh(bot, guid);

    PlayerbotAI* ai = GetBotAI(bot);

    Creature *creature = ai->GetCreature(guid);
    if (creature && sServerFacade.GetDeathState(creature) == CORPSE)
        return creature;

    GameObject* go = ai->GetGameObject(guid);
    if (go && sServerFacade.isSpawned(go))
        return go;

    return NULL;
}

LootObject::LootObject(const LootObject& other)
{
    guid = other.guid;
    skillId = other.skillId;
    reqSkillValue = other.reqSkillValue;
    reqItem = other.reqItem;
}

bool LootObject::IsApproachSafe(Player* bot, Unit** blocker)
{
    if (blocker)
        *blocker = nullptr;
    PlayerbotAI* ai = GetBotAI(bot);
    // Preserve the existing follower-only policy, including its configuration
    // opt-out. An active human master does not make unattended loot safe.
    if (!bot->GetGroup() || ai->IsGroupLeader() || sPlayerbotAIConfig.lootHostileDistance <= 0.0f)
        return true;
    WorldObject* object = GetWorldObject(bot);
    if (!object)
        return false;
    // Search the native visible area, not just the configured combat radius:
    // higher-level creatures can initiate aggro from beyond that old cutoff.
    float const searchRadius = std::max(sPlayerbotAIConfig.lootHostileDistance,
        std::max(bot->GetVisibilityDistance(), object->GetVisibilityDistance()));
    std::list<Unit*> hostiles = ai->GetAllHostileNPCNonPetUnitsAroundWO(object, searchRadius);
    for (Unit* hostile : hostiles)
    {
        bool const engaged = hostile->IsInCombat();
        float safetyRadius = sPlayerbotAIConfig.lootHostileDistance;
        float aggroRadius = 0.0f;
        if (!engaged)
        {
            // Preserve native wall/floor separation for idle enemies. Use the
            // gathering bot's level/auras, not its human master's level.
            Creature* creature = hostile->ToCreature();
            if (!creature || !creature->CanInitiateAttack())
                continue;
            aggroRadius = creature->GetAttackDistance(bot);
            if (aggroRadius <= 0.0f)
                continue;

            // The bot can stand up to INTERACTION_DISTANCE from the object.
            // Protect that interaction area, not just the object's centre.
            safetyRadius = aggroRadius + INTERACTION_DISTANCE;
        }

        else if (Creature* creature = hostile->ToCreature())
        {
            // Fighting enemies remain conservative: never shrink the old
            // combat boundary, and still account for a larger native reach.
            aggroRadius = creature->GetAttackDistance(bot);
            if (aggroRadius > 0.0f)
                safetyRadius = std::max(safetyRadius, aggroRadius + INTERACTION_DISTANCE);
        }

        // Idle aggro uses the same centre-distance convention as native AI.
        // Engaged enemies retain size-aware combat exclusion as a minimum.
        if (!hostile->IsWithinDistInMap(object, safetyRadius, true,
            engaged ? SizeFactor::BoundingRadius : SizeFactor::None))
            continue;
        // Native AI also rejects cheap range checks before casting LOS rays.
        if (!engaged && !hostile->IsWithinLOSInMap(object, true))
            continue;
        if (ai->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT))
            sLog.outLoot("bot=%s event=loot-threat hostile=\"%s\" bot-level=%u hostile-level=%u aggro-radius=%.1f safety-radius=%.1f engaged=%d",
                bot->GetName(), hostile->GetName(), bot->GetLevel(), hostile->GetLevel(),
                aggroRadius, safetyRadius, engaged ? 1 : 0);
        if (blocker)
            *blocker = hostile;
        return false;
    }
    return true;
}

bool LootObject::IsLootPossible(Player* bot, bool* suppressRediscovery)
{
    if (suppressRediscovery)
        *suppressRediscovery = false;

    if (IsEmpty() || !GetWorldObject(bot))
        return false;

    PlayerbotAI* ai = GetBotAI(bot);

    // An activated personal quest chest does not expose this bot's quest-item
    // slots until the bot opens it and Player::SendLoot calls
    // FillNotNormalLootFor(bot). The native read-only query checks the actual
    // remaining quest items before bypassing the unpopulated per-player view.
    // Template eligibility alone can repeatedly admit an empty shared chest.
    GameObject* questChest = guid.IsGameObject() ? ai->GetGameObject(guid) : nullptr;
    bool canOpenPersonalQuestLoot = CanOpenActivatedQuestChest(bot, questChest);

    if (reqItem && !bot->HasItemCount(reqItem, 1))
    {
        // Scripts and encounters can remove GO_FLAG_LOCKED without changing
        // the object's template lock. In that state the native game object is
        // authoritative: an unlocked chest no longer requires the cached key.
        GameObject* go = guid.IsGameObject() ? ai->GetGameObject(guid) : nullptr;
        bool unlockedChest = go && go->GetGOInfo()->type == GAMEOBJECT_TYPE_CHEST &&
            go->GetGOInfo()->GetLootId() && !go->HasFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
        if (!unlockedChest)
            return false;
    }

    if (guid.IsCreature())
    {
        Creature* creature = ai->GetCreature(guid);
        if (creature && sServerFacade.GetDeathState(creature) == CORPSE)
        {
            // Reuse Turtle's authoritative group, round-robin, quest-item and
            // allowed-looter checks. Do this even before the compatibility
            // m_loot pointer is populated: Player::IsAllowedToLoot reads the
            // creature's native loot object directly. Gating this call on
            // m_loot allowed bots to repeatedly approach corpses assigned to
            // another group member.
            if (skillId != SKILL_SKINNING && !bot->IsAllowedToLoot(creature))
            {
                if (ai->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT))
                    sLog.outLoot("bot=%s event=reject guid=%lu reason=native-loot-rights",
                        bot->GetName(), guid.GetRawValue());
                if (suppressRediscovery)
                    *suppressRediscovery = true;
                return false;
            }
        }
    }

    AiObjectContext* context = ai->GetAiObjectContext();

    if (!canOpenPersonalQuestLoot &&
        !AI_VALUE2_LAZY(bool, "should loot object", std::to_string(guid.GetRawValue())))
    {
        // A creature can remain server-lootable when it contains only items
        // excluded by the bot's loot policy (for example linen). Remember the
        // inspection briefly so the periodic corpse scanner does not make the
        // bot run back and open the same corpse a second time.
        if (guid.IsCreature() && suppressRediscovery)
            *suppressRediscovery = true;
        return false;
    }

    // Check if the game object has quest loot and bot has the quest for it
    if (guid.IsGameObject())
    {
        GameObject* go = ai->GetGameObject(guid);
        if (go)
        {
            // Ignore for mining nodes and herbs
            if (skillId != SKILL_MINING && skillId != SKILL_HERBALISM)
            {
                if (sObjectMgr.IsGameObjectForQuests(guid.GetEntry()))
                {
                    if (!go->ActivateToQuest(bot))
                    {
                        return false;
                    }

                    Player* questChestTurn = GetPersonalQuestChestTurn(bot, go);
                    if (questChestTurn != bot)
                    {
                        if (ai->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT))
                            sLog.outLoot("bot=%s event=quest-chest-wait guid=%lu turn=%s",
                                bot->GetName(), guid.GetRawValue(), questChestTurn->GetName());
                        return false;
                    }
                }
            }
            
            // herb-like quest objects
            if (skillId == SKILL_HERBALISM && reqSkillValue == 1)
            {
                if (sObjectMgr.IsGameObjectForQuests(guid.GetEntry()))
                {
                    if (go->ActivateToQuest(bot))
                    {
                        bool hasQuestItems = false;
                        for (auto& entry : GAI_VALUE2(std::list<uint32>, "entry loot list", -1*int(go->GetEntry())))
                        {
                            if (ItemUsageValue::IsNeededForQuest(bot, entry))
                            {
                                hasQuestItems = true;
                            }
                        }
                        return hasQuestItems || go->GetLootState() != GO_READY;
                    }
                }
            }

            // An activated quest chest is not globally exhausted: the core's
            // Player::SendLoot path calls FillNotNormalLootFor for each later
            // player. Permit that narrow case while retaining the normal
            // in-use guard for every other game object.
            if ((go->IsInUse() || go->GetGoState() == GO_STATE_ACTIVE) &&
                !canOpenPersonalQuestLoot)
            {
                if (ai->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT))
                    sLog.outLoot("bot=%s event=reject guid=%lu reason=gameobject-active go-state=%u loot-state=%u quest=%d",
                        bot->GetName(), guid.GetRawValue(), uint32(go->GetGoState()),
                        uint32(go->getLootState()), sObjectMgr.IsGameObjectForQuests(go->GetEntry()) ? 1 : 0);
                return false;
            }
        }
    }

    if (skillId == SKILL_NONE)
        return true;

    if (skillId == SKILL_FISHING)
        return false;

    if (!ai->HasSkill((SkillType)skillId))
        return false;

    if (!reqSkillValue)
        return true;

    uint32 skillValue = uint32(bot->GetSkillValue(skillId));
    if (reqSkillValue > skillValue)
        return false;

    if (skillId == SKILL_MINING && !bot->HasItemCount(2901, 1))
        return false;

    if (skillId == SKILL_SKINNING && !bot->HasItemCount(7005, 1))
        return false;

    return true;
}

bool LootObjectStack::Add(ObjectGuid guid)
{
    PruneIgnored();

    std::map<ObjectGuid, time_t>::const_iterator ignored = ignoredLoot.find(guid);
    if (ignored != ignoredLoot.end())
    {
#ifdef MANGOSBOT_ZERO
        // Recheck a rejected body when native ownership/item eligibility
        // changes. Successful inspections and real failures retain their
        // own suppression; gathering also admits the later skinning phase.
        if (!ignoredBodyLoot.count(guid))
            return false;
        LootObject current(bot, guid);
        Creature* corpse = GetBotAI(bot)->GetCreature(guid);
        bool bodyReady = unavailableBodyLoot.count(guid) &&
            current.skillId == SKILL_NONE && corpse &&
            !corpse->loot.isLooted() && current.IsLootPossible(bot);
        bool skinReady = !bodyReady &&
            GetBotAI(bot)->HasStrategy("gather", BotState::BOT_STATE_NON_COMBAT) &&
            current.skillId == SKILL_SKINNING && corpse &&
            corpse->IsSkinnableBy(bot) && !corpse->lootForSkin &&
            corpse->loot.isLooted() && current.IsLootPossible(bot);
        if (!bodyReady && !skinReady)
            return false;
        ignoredLoot.erase(guid);
        ignoredBodyLoot.erase(guid);
        unavailableBodyLoot.erase(guid);
        if (GetBotAI(bot)->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT))
            sLog.outLoot("bot=%s event=%s guid=%lu", bot->GetName(),
                bodyReady ? "body-loot-ready" : "body-to-skin-ready", guid.GetRawValue());
#else
        return false;
#endif
    }

    LootTargetList::iterator existing = availableLoot.find(guid);
    if (existing != availableLoot.end())
    {
        // A corpse can enter the queue from the XP event while combat is still
        // active. Refresh corpse entries when rediscovered after combat, but do
        // not keep game objects alive indefinitely as the periodic scanner sees
        // them repeatedly.
        if (guid.IsCreature())
        {
            availableLoot.erase(existing);
            availableLoot.insert(guid);
        }
        return false;
    }

    availableLoot.insert(guid);

    if (availableLoot.size() < MAX_LOOT_OBJECT_COUNT)
        return true;

    std::vector<LootObject> ordered = OrderByDistance();
    for (size_t i = MAX_LOOT_OBJECT_COUNT; i < ordered.size(); i++)
        Remove(ordered[i].guid);

    return true;
}

void LootObjectStack::Remove(ObjectGuid guid)
{
    LootTargetList::iterator i = availableLoot.find(guid);
    if (i != availableLoot.end())
        availableLoot.erase(i);

    deferredLoot.erase(guid);
}

void LootObjectStack::Defer(ObjectGuid guid, uint32 seconds)
{
    // Keep the target queued while making it temporarily unavailable. This is
    // required for asynchronous gathering casts: a normal orange-skill
    // failure produces no loot response, but a manually queued `u go` target
    // still has to become selectable again without relying on rediscovery.
    LootTargetList::iterator existing = availableLoot.find(guid);
    if (existing != availableLoot.end())
        availableLoot.erase(existing);
    availableLoot.insert(guid);
    deferredLoot[guid] = time(0) + std::max<uint32>(1, seconds);
}

void LootObjectStack::Ignore(ObjectGuid guid, uint32 seconds, bool bodyLoot, bool unavailableBody)
{
    Remove(guid);
    ignoredLoot[guid] = time(0) + std::max<uint32>(1, seconds);
    ignoredBodyLoot.erase(guid);
    unavailableBodyLoot.erase(guid);
    if (bodyLoot && guid.IsCreature())
    {
        ignoredBodyLoot.insert(guid);
        if (unavailableBody)
            unavailableBodyLoot.insert(guid);
    }
}

void LootObjectStack::PruneIgnored()
{
    time_t now = time(0);
    for (std::map<ObjectGuid, time_t>::iterator i = ignoredLoot.begin(); i != ignoredLoot.end();)
    {
        if (i->second <= now)
        {
            ignoredBodyLoot.erase(i->first);
            unavailableBodyLoot.erase(i->first);
            ignoredLoot.erase(i++);
        }
        else
            ++i;
    }
}

void LootObjectStack::PruneDeferred()
{
    time_t now = time(0);
    for (std::map<ObjectGuid, time_t>::iterator i = deferredLoot.begin(); i != deferredLoot.end();)
    {
        if (i->second <= now)
            deferredLoot.erase(i++);
        else
            ++i;
    }
}

void LootObjectStack::Clear()
{
    availableLoot.clear();
    deferredLoot.clear();
    ignoredLoot.clear();
    ignoredBodyLoot.clear();
    unavailableBodyLoot.clear();
}

bool LootObjectStack::CanLoot(float maxDistance)
{
    std::vector<LootObject> ordered = OrderByDistance(maxDistance);
    return !ordered.empty();
}

LootObject LootObjectStack::GetLoot(float maxDistance)
{
    std::vector<LootObject> ordered = OrderByDistance(maxDistance);
    return ordered.empty() ? LootObject() : *ordered.begin();
}

std::vector<LootObject> LootObjectStack::OrderByDistance(float maxDistance)
{
    PruneDeferred();
    size_t beforeShrink = availableLoot.size();
    availableLoot.shrink(time(0) - 30);
    if (availableLoot.size() < beforeShrink &&
        GetBotAI(bot)->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT))
        sLog.outLoot("bot=%s event=queue-expire count=%zu age-seconds=30",
            bot->GetName(), beforeShrink - availableLoot.size());

    // Creature corpses always precede chests and quest game objects. A
    // multimap also preserves multiple targets at the same measured distance;
    // the old map<float, LootObject> silently discarded one of them.
    typedef std::pair<uint8, float> LootOrder;
    std::multimap<LootOrder, LootObject> sortedMap;
    LootTargetList safeCopy(availableLoot);
    for (LootTargetList::iterator i = safeCopy.begin(); i != safeCopy.end(); i++)
    {
        ObjectGuid guid = i->guid;
        if (deferredLoot.find(guid) != deferredLoot.end())
            continue;

        LootObject lootObject(bot, guid);
        bool suppressRediscovery = false;
        if (!lootObject.IsLootPossible(bot, &suppressRediscovery))
        {
            if (guid.IsCreature() && suppressRediscovery)
                Ignore(guid, sPlayerbotAIConfig.lootTargetInspectDelay, lootObject.skillId == SKILL_NONE, lootObject.skillId == SKILL_NONE);
            else
                Remove(guid);
            continue;
        }

        WorldObject* worldObject = lootObject.GetWorldObject(bot);
        if (!worldObject)
        {
            Remove(guid);
            continue;
        }

        float distance = bot->GetDistance(worldObject);
        if (!maxDistance || distance <= maxDistance)
            sortedMap.insert(std::make_pair(LootOrder(guid.IsCreature() ? 0 : 1, distance), lootObject));
    }

    std::vector<LootObject> result;
    for (std::multimap<LootOrder, LootObject>::iterator i = sortedMap.begin(); i != sortedMap.end(); i++)
        result.push_back(i->second);
    return result;
}
