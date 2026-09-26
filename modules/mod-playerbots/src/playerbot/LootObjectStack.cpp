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
        go->ActivateToQuest(bot);
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
        if (creature->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE))
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

bool LootObject::IsLootPossible(Player* bot, bool* suppressRediscovery)
{
    if (suppressRediscovery)
        *suppressRediscovery = false;

    if (IsEmpty() || !GetWorldObject(bot))
        return false;

    PlayerbotAI* ai = GetBotAI(bot);

    // An activated personal quest chest does not expose this bot's quest-item
    // slots until the bot opens it and Player::SendLoot calls
    // FillNotNormalLootFor(bot). Do not ask "should loot object" first: that
    // value can only see the still-empty per-player view and would reject the
    // chest before the core has a chance to populate it.
    GameObject* questChest = guid.IsGameObject() ? ai->GetGameObject(guid) : nullptr;
    bool canOpenPersonalQuestLoot = CanOpenActivatedQuestChest(bot, questChest);

    if (reqItem && !bot->HasItemCount(reqItem, 1))
        return false;

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
        return false;

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
}

void LootObjectStack::Ignore(ObjectGuid guid, uint32 seconds)
{
    Remove(guid);
    ignoredLoot[guid] = time(0) + std::max<uint32>(1, seconds);
}

void LootObjectStack::PruneIgnored()
{
    time_t now = time(0);
    for (std::map<ObjectGuid, time_t>::iterator i = ignoredLoot.begin(); i != ignoredLoot.end();)
    {
        if (i->second <= now)
            ignoredLoot.erase(i++);
        else
            ++i;
    }
}

void LootObjectStack::Clear()
{
    availableLoot.clear();
    ignoredLoot.clear();
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
        LootObject lootObject(bot, guid);
        bool suppressRediscovery = false;
        if (!lootObject.IsLootPossible(bot, &suppressRediscovery))
        {
            if (guid.IsCreature() && suppressRediscovery)
                Ignore(guid, sPlayerbotAIConfig.lootTargetInspectDelay);
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
