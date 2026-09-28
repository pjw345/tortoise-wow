
#include "playerbot/playerbot.h"
#include "LootAction.h"

#include "playerbot/LootObjectStack.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "playerbot/RandomPlayerbotMgr.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/values/LootStrategyValue.h"
#include "playerbot/strategy/values/ItemUsageValue.h"
#include "playerbot/ServerFacade.h"
#include "playerbot/strategy/values/SharedValueContext.h"


using namespace ai;

bool LootAction::Execute(Event& event)
{
    if (!AI_VALUE(bool, "has available loot"))
        return false;

    LootObject prevLoot = AI_VALUE(LootObject, "loot target");

    LootObject lootObject;
    Player* master = ai->GetMaster();
    std::vector<LootObject> candidates = AI_VALUE(LootObjectStack*, "available loot")->OrderByDistance(sPlayerbotAIConfig.lootDistance);
    for (LootObject& candidate : candidates)
    {
        if (master && master != bot)
        {
            Creature* c = ai->GetCreature(candidate.guid);
            if (c && sServerFacade.GetDeathState(c) == CORPSE)
            {
                // Use the same master-to-corpse radius that admitted the target
                // to the loot queue. The former follow-distance-plus-interact
                // check reduced the effective active-master range to roughly
                // six yards even when GroupMemberLootDistanceWithActiveMaster
                // was configured as 25 yards.
                float safeRange = sPlayerbotAIConfig.lootDistance;
                if (bot->GetGroup() && !ai->IsGroupLeader())
                {
                    safeRange = ai->HasActivePlayerMaster() ?
                        sPlayerbotAIConfig.groupMemberLootDistanceWithActiveMaster :
                        sPlayerbotAIConfig.groupMemberLootDistance;
                }
                if (sServerFacade.GetDistance2d(master, c) > safeRange)
                    continue;
            }
        }
        lootObject = candidate;
        break;
    }

    if (lootObject.IsEmpty())
        return false;

    bool released = false;
    if (!prevLoot.IsEmpty() && prevLoot.guid != lootObject.guid)
    {
        WorldPacket packet(CMSG_LOOT_RELEASE, 8);
        packet << prevLoot.guid;
        bot->GetSession()->HandleLootReleaseOpcode(packet);
        released = true;
    }

    if (ai->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT))
        sLog.outLoot("bot=%s event=select guid=%lu previous=%lu released=%d",
            bot->GetName(), lootObject.guid.GetRawValue(), prevLoot.guid.GetRawValue(), released ? 1 : 0);

    context->GetValue<LootObject>("loot target")->Set(lootObject);
    return true;
}

enum ProfessionSpells
{
    ALCHEMY                      = 2259,
    BLACKSMITHING                = 2018,
    COOKING                      = 2550,
    ENCHANTING                   = 7411,
    ENGINEERING                  = 49383,
    FIRST_AID                    = 3273,
    FISHING                      = 7620,
    HERB_GATHERING               = 2366,
    INSCRIPTION                  = 45357,
    JEWELCRAFTING                = 25229,
    MINING                       = 2575,
    SKINNING                     = 8613,
    TAILORING                    = 3908
};

bool OpenLootAction::Execute(Event& event)
{
    LootObject lootObject = AI_VALUE(LootObject, "loot target");
    bool result = DoLoot(lootObject);
    if (result)
    {
        AI_VALUE(LootObjectStack*, "available loot")->Remove(lootObject.guid);
        context->GetValue<LootObject>("loot target")->Set(LootObject());
    }
    return result;
}

bool OpenLootAction::DoLoot(LootObject& lootObject)
{
    if (lootObject.IsEmpty())
        return false;

    bool debugLoot = ai->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT);
    if (debugLoot)
        sLog.outLoot("bot=%s event=open-attempt guid=%lu", bot->GetName(), lootObject.guid.GetRawValue());

    Creature* creature = ai->GetCreature(lootObject.guid);
    // Gate the loot send on the SERVER's exact loot-range rule: a 3D distance check against
    // GetMaxLootDistance with no bounding-radius slack (Player::SendLoot, Player.cpp:9382 ->
    // Object _IsWithinDist with SizeFactor::None). The old gate only measured 2D distance and
    // ignored Z, so while the bot was being dragged along by follow/chase it fired CMSG_LOOT
    // from a few yards above/below a corpse it had not actually reached (2D=2y but 3D>5y),
    // and the server replied TOO_FAR. Returning false here keeps MoveToLoot approaching until
    // the bot is truly standing on the corpse, then it loots.
    if (creature && !creature->IsWithinDistInMap(bot, bot->GetMaxLootDistance(creature), true, SizeFactor::None))
    {
        if (debugLoot)
            sLog.outLoot("bot=%s event=open-wait guid=%lu reason=distance distance2d=%.1f cap=%.1f",
                bot->GetName(), lootObject.guid.GetRawValue(), sServerFacade.GetDistance2d(bot, creature), bot->GetMaxLootDistance(creature));
        return false;
    }

    // Re-confirm the creature is still a fresh, lootable corpse before sending CMSG_LOOT.
    // The cached UNIT_DYNFLAG_LOOTABLE can lag behind a corpse that has despawned or
    // respawned while the bot was busy chain-killing (corpses age up to 30s in the loot
    // stack). A stale entry makes the server reply with a loot error (DIDNT_KILL) and the
    // bot kneel/abort on a non-corpse. Same predicate LootObjectStack::Refresh uses.
    if (creature && sServerFacade.GetDeathState(creature) != CORPSE)
    {
        if (debugLoot)
            sLog.outLoot("bot=%s event=reject guid=%lu reason=not-corpse death-state=%d",
                bot->GetName(), lootObject.guid.GetRawValue(), (int)sServerFacade.GetDeathState(creature));
        AI_VALUE(LootObjectStack*, "available loot")->Remove(lootObject.guid);
        RESET_AI_VALUE(LootObject, "loot target");
        return false;
    }

    // A corpse can expose ordinary loot and skinning at the same time. Refresh()
    // leaves skillId at SKILL_NONE while the ordinary-loot phase is pending, so
    // consume that loot before attempting the creature's gathering skill.
    if (creature && creature->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_LOOTABLE) &&
        lootObject.skillId == SKILL_NONE)
    {
        if (!lootObject.IsLootPossible(bot)) //Clear loot if bot can't loot it.
        {
            AI_VALUE(LootObjectStack*, "available loot")->Ignore(lootObject.guid, sPlayerbotAIConfig.lootTargetInspectDelay);
            if (debugLoot)
                sLog.outLoot("bot=%s event=reject guid=%lu reason=loot-policy suppress-seconds=%u",
                    bot->GetName(), lootObject.guid.GetRawValue(), sPlayerbotAIConfig.lootTargetInspectDelay);
            return true;
        }

        if (debugLoot)
            sLog.outLoot("bot=%s event=open-send guid=%lu alive=%d tapped=%d distance2d=%.1f cap=%.1f delay=%u",
                bot->GetName(), lootObject.guid.GetRawValue(),
                creature->IsAlive() ? 1 : 0, creature->IsTappedBy(bot) ? 1 : 0,
                sServerFacade.GetDistance2d(bot, creature), bot->GetMaxLootDistance(creature), sPlayerbotAIConfig.lootDelay);

        WorldPacket packet(CMSG_LOOT, 8);
        packet << lootObject.guid;
        bot->GetSession()->HandleLootOpcode(packet);
        SetDuration(sPlayerbotAIConfig.lootDelay);

        if (IsRealPlayer(bot))
        {
            WorldPacket data(SMSG_EMOTE, 4 + 8);
            data << uint32(EMOTE_ONESHOT_LOOT);
            data << bot->GetObjectGuid();
            bot->GetSession()->SendPacket(data);
        }

        return true;
    }

    if (creature)
    {
        SkillType skill = (SkillType)creature->GetCreatureInfo()->GetRequiredLootSkill();
        if (debugLoot)
            sLog.outLoot("bot=%s event=gather-attempt guid=%lu skill=%u required=%u",
                bot->GetName(), lootObject.guid.GetRawValue(), skill, lootObject.reqSkillValue);
        bool opened = false;
        if (!CanOpenLock(skill, lootObject.reqSkillValue))
        {
            if (debugLoot)
                sLog.outLoot("bot=%s event=reject guid=%lu reason=gather-skill skill=%u",
                    bot->GetName(), lootObject.guid.GetRawValue(), skill);
        }
        else switch (skill)
        {
        case SKILL_ENGINEERING:
            opened = ai->HasSkill(SKILL_ENGINEERING) && ai->CastSpell(ENGINEERING, creature);
            break;
        case SKILL_HERBALISM:
            opened = ai->HasSkill(SKILL_HERBALISM) && ai->CastSpell(32605, creature);
            break;
        case SKILL_MINING:
            opened = ai->HasSkill(SKILL_MINING) && ai->CastSpell(32606, creature);
            break;
        default:
            opened = ai->HasSkill(SKILL_SKINNING) && ai->CastSpell(SKINNING, creature);
            break;
        }

        if (!opened)
        {
            if (debugLoot)
                sLog.outLoot("bot=%s event=gather-retry guid=%lu skill=%u retry-seconds=%u",
                    bot->GetName(), lootObject.guid.GetRawValue(), skill,
                    sPlayerbotAIConfig.lootTargetRetryDelay);
            AI_VALUE(LootObjectStack*, "available loot")->Ignore(
                lootObject.guid, sPlayerbotAIConfig.lootTargetRetryDelay);
            RESET_AI_VALUE(LootObject, "loot target");
        }

        return opened;
    }

    GameObject* go = ai->GetGameObject(lootObject.guid);
    if (go && sServerFacade.GetDistance2d(bot, go) > INTERACTION_DISTANCE)
        return false;

    bool canOpenPersonalQuestLoot = CanOpenActivatedQuestChest(bot, go);
    if (go && (go->IsInUse() || go->GetGoState() == GO_STATE_ACTIVE) &&
        !canOpenPersonalQuestLoot)
    {
        if (debugLoot)
            sLog.outLoot("bot=%s event=reject guid=%lu reason=gameobject-active go-state=%u loot-state=%u quest=%d",
                bot->GetName(), lootObject.guid.GetRawValue(), uint32(go->GetGoState()),
                uint32(go->getLootState()), sObjectMgr.IsGameObjectForQuests(go->GetEntry()) ? 1 : 0);
        return false;
    }

    if (debugLoot && canOpenPersonalQuestLoot)
        sLog.outLoot("bot=%s event=gameobject-personal-quest-reopen guid=%lu go-state=%u loot-state=%u",
            bot->GetName(), lootObject.guid.GetRawValue(), uint32(go->GetGoState()),
            uint32(go->getLootState()));

    if (lootObject.skillId == SKILL_MINING)
        return ai->HasSkill(SKILL_MINING) ? ai->CastSpell(MINING, bot) : false;

    if (lootObject.skillId == SKILL_HERBALISM)
    {
        // herb-like quest objects
        bool isForQuest = false;
        if (go && sObjectMgr.IsGameObjectForQuests(lootObject.guid.GetEntry()))
        {
            if (go->ActivateToQuest(bot))
            {
                std::list<uint32> lootItems = GAI_VALUE2(std::list<uint32>, "entry loot list", -1*int32(go->GetEntry()));
                isForQuest = !lootItems.empty() || go->GetLootState() != GO_READY;
            }
        }

        if (!isForQuest)
        {
            return ai->HasSkill(SKILL_HERBALISM) ? ai->CastSpell(HERB_GATHERING, bot) : false;
        }
    }

    uint32 spellId = GetOpeningSpell(lootObject);
    if (!spellId)
    {
        if (debugLoot)
            sLog.outLoot("bot=%s event=reject guid=%lu reason=no-opening-spell",
                bot->GetName(), lootObject.guid.GetRawValue());
        return false;
    }

    if (!lootObject.IsLootPossible(bot)) //Clear loot if bot can't loot it.
    {
        if (debugLoot)
            sLog.outLoot("bot=%s event=reject guid=%lu reason=gameobject-policy",
                bot->GetName(), lootObject.guid.GetRawValue());
        return true;
    }

    if (debugLoot)
        sLog.outLoot("bot=%s event=gameobject-open guid=%lu spell=%u distance=%.1f",
            bot->GetName(), lootObject.guid.GetRawValue(), spellId,
            go ? sServerFacade.GetDistance2d(bot, go) : -1.0f);

    //Keys need to use the key 
    if (spellId == sPlayerbotAIConfig.openGoSpell && go && lootObject.reqItem && bot->HasItemCount(lootObject.reqItem,1,false))
    {
        return ai->DoSpecificAction("use", Event("do loot", chat->formatQItem(lootObject.reqItem) + " " + chat->formatGameobject(go)));
    }

    bool opened = ai->CastSpell(spellId, bot);
    if (debugLoot)
        sLog.outLoot("bot=%s event=gameobject-cast guid=%lu spell=%u result=%d",
            bot->GetName(), lootObject.guid.GetRawValue(), spellId, opened ? 1 : 0);
    return opened;
}

uint32 OpenLootAction::GetOpeningSpell(LootObject& lootObject)
{
    GameObject* go = ai->GetGameObject(lootObject.guid);
    if (go && sServerFacade.isSpawned(go))
        return GetOpeningSpell(lootObject, go);

    return 0;
}

uint32 OpenLootAction::GetOpeningSpell(LootObject& lootObject, GameObject* go)
{
    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;

		if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
			continue;

		if (spellId == MINING || spellId == HERB_GATHERING)
			continue;

		const SpellEntry* pSpellInfo = sServerFacade.LookupSpellInfo(spellId);
		if (!pSpellInfo)
			continue;

        if (CanOpenLock(lootObject, pSpellInfo, go))
            return spellId;
    }

    for (uint32 spellId = 0; spellId < sServerFacade.GetSpellInfoRows(); spellId++)
    {
        if (spellId == MINING || spellId == HERB_GATHERING)
            continue;

		const SpellEntry* pSpellInfo = sServerFacade.LookupSpellInfo(spellId);
		if (!pSpellInfo)
            continue;

        if (CanOpenLock(lootObject, pSpellInfo, go))
            return spellId;
    }

    return sPlayerbotAIConfig.openGoSpell;
}

bool OpenLootAction::CanOpenLock(LootObject& lootObject, const SpellEntry* pSpellInfo, GameObject* go)
{
    for (int effIndex = 0; effIndex <= EFFECT_INDEX_2; effIndex++)
    {
        if (pSpellInfo->Effect[effIndex] != SPELL_EFFECT_OPEN_LOCK && pSpellInfo->Effect[effIndex] != SPELL_EFFECT_SKINNING)
            return false;

        uint32 lockId = go->GetGOInfo()->GetLockId();
        if (!lockId)
            return false;

        LockEntry const *lockInfo = sLockStore.LookupEntry(lockId);
        if (!lockInfo)
            return false;

        bool reqKey = false;                                    // some locks not have reqs

        for(int j = 0; j < 8; ++j)
        {
            switch(lockInfo->Type[j])
            {
            /*
            case LOCK_KEY_ITEM:
                return true;
            */
            case LOCK_KEY_SKILL:
                {
                    if(uint32(pSpellInfo->EffectMiscValue[effIndex]) != lockInfo->Index[j])
                        continue;

                    uint32 skillId = SkillByLockType(LockType(lockInfo->Index[j]));
                    if (skillId == SKILL_NONE)
                        return true;

                    if (CanOpenLock(skillId, lockInfo->Skill[j]))
                        return true;
                }
            }
        }
    }

    return false;
}

bool OpenLootAction::CanOpenLock(uint32 skillId, uint32 reqSkillValue)
{
    uint32 skillValue = bot->GetSkillValue(skillId);
    return skillValue >= reqSkillValue || !reqSkillValue;
}

bool StoreLootAction::Execute(Event& event)
{
    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
    bool debugLoot = ai->HasStrategy("debug loot", BotState::BOT_STATE_NON_COMBAT);
    WorldPacket p(event.getPacket()); // (8+1+4+1+1+4+4+4+4+4+1)
    ObjectGuid guid;
    uint8 loot_type;
    uint32 gold = 0;
    uint8 items = 0;

    p.rpos(0);
    p >> guid;      // 8 corpse guid
    p >> loot_type; // 1 loot type

    if (p.size() > 10)
    {
        p >> gold;      // 4 money on corpse
        p >> items;     // 1 number of items on corpse
    }
    else
    {
        // Not a loot response but a loot ERROR packet from Player::SendLootError:
        // [guid][uint8 loot_type=0][uint8 errorCode]. A real loot response (even an empty
        // one) always carries the gold + item-count block, so size <= 10 means the server
        // rejected the loot. Decode the trailing error byte so the rejection isn't a mystery.
        uint8 lootError = 0;
        if (p.rpos() < p.size())
            p >> lootError;

        const char* errName =
            lootError == 0 ? "DIDNT_KILL/no-permission" :
            lootError == 4 ? "TOO_FAR" : "other";
        if (debugLoot)
            sLog.outLoot("bot=\"%s\" target_guid=%lu phase=open result=rejected error=%u reason=\"%s\"",
                bot->GetName(), guid.GetRawValue(), lootError, errName);

        // A rejected request can be transient, but it must not be re-added on
        // every nearby-object scan while the server is still rejecting it.
        AI_VALUE(LootObjectStack*, "available loot")->Ignore(guid, sPlayerbotAIConfig.lootTargetRetryDelay);
        RESET_AI_VALUE(LootObject, "loot target");
        return false;
    }

    if (debugLoot)
        sLog.outLoot("bot=%s event=store guid=%lu loot-type=%u gold=%u items=%u",
            bot->GetName(), guid.GetRawValue(), loot_type, gold, items);

    bot->SetLootGuid(guid);

    Loot* loot = sLootMgr.GetLoot(bot);

    if (!loot)
    {
        if (debugLoot)
            sLog.outLoot("bot=\"%s\" target_guid=%lu phase=store result=retry reason=no_native_loot",
                bot->GetName(), guid.GetRawValue());
        // Release the corpse so the bot stops the loot (kneel) animation instead of staying
        // crouched forever, and drop it from the loot stack so it isn't retried next tick.
        WorldPacket release(CMSG_LOOT_RELEASE, 8);
        release << guid;
        bot->GetSession()->HandleLootReleaseOpcode(release);
        AI_VALUE(LootObjectStack*, "available loot")->Ignore(guid, sPlayerbotAIConfig.lootTargetRetryDelay);
        RESET_AI_VALUE(LootObject, "loot target");
        return false;
    }

    uint32 itemsTaken = 0;
    bool transferFailed = false;

    if (gold > 0)
    {
        WorldPacket packet(CMSG_LOOT_MONEY, 0);
        bot->GetSession()->HandleLootMoneyOpcode(packet);
    }

    for (uint8 i = 0; i < items; ++i)
    {
        uint32 itemid;
        uint32 randomPropertyId;
        uint32 itemcount;
        uint8 lootslot_type;
        uint8 itemindex;
        bool grab = false;

        p >> itemindex;
        p >> itemid;
        p >> itemcount;
        p.read_skip<uint32>();  // display id
        p.read_skip<uint32>();  // randomSuffix
        p >> randomPropertyId;  // randomPropertyId
        p >> lootslot_type;     // 0 = can get, 1 = look only, 2 = master get

        ItemQualifier itemQualifier(itemid, ((int32)randomPropertyId));

        if (lootslot_type != LOOT_SLOT_NORMAL
#ifndef MANGOSBOT_ZERO
                && lootslot_type != LOOT_SLOT_OWNER
#endif
            )
        {
            if (debugLoot)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=item result=skipped item_id=%u reason=slot_type slot_type=%u",
                    bot->GetName(), guid.GetRawValue(), itemid, lootslot_type);
            continue;
        }

        if (loot_type != LOOT_SKINNING && !IsLootAllowed(itemQualifier, ai))
        {
            if (debugLoot)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=item result=skipped item_id=%u reason=policy",
                    bot->GetName(), guid.GetRawValue(), itemid);
            continue;
        }

        if (AI_VALUE2(uint32, "stack space for item", itemid) < itemcount)
        {
            if (debugLoot)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=item result=skipped item_id=%u reason=no_space count=%u",
                    bot->GetName(), guid.GetRawValue(), itemid, itemcount);
            continue;
        }

        ItemPrototype const *proto = sItemStorage.LookupEntry<ItemPrototype>(itemid);
        if (!proto)
            continue;

        // Turtle appends player-specific quest items after the shared item
        // slots. Use its native slot resolver rather than the compatibility
        // helper that only indexed loot->items.
        QuestItem* questItem = nullptr;
        LootItem* lootItem = loot->LootItemInSlot(itemindex, bot->GetGUIDLow(), &questItem);

        if (!lootItem)
            continue;

        // Match Turtle's native autostore contract: quest items use is_blocked
        // for other purposes, so only shared blocked items are rejected here.
        // The native handler remains authoritative for all other permissions.
        if (!questItem && lootItem->is_blocked)
        {
            if (debugLoot)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=item result=skipped item_id=%u reason=blocked",
                    bot->GetName(), guid.GetRawValue(), itemid);
            continue;
        }

        Player* master = ai->GetMaster();
        if (sRandomPlayerbotMgr.IsRandomBot(bot) && master)
        {
            uint32 price = itemcount * ItemUsageValue::GetBotBuyPrice(proto, bot) + gold;
            if (price)
                sRandomPlayerbotMgr.AddTradeDiscount(bot, master, price);
        }

        uint32 itemCountBefore = bot->GetItemCount(itemid);

        WorldPacket packet(CMSG_AUTOSTORE_LOOT_ITEM, 1);
        packet << itemindex;
        bot->GetSession()->HandleAutostoreLootItemOpcode(packet);

        uint32 itemCountAfter = bot->GetItemCount(itemid);
        uint32 itemCountTaken = itemCountAfter > itemCountBefore ? itemCountAfter - itemCountBefore : 0;
        if (!itemCountTaken)
        {
            transferFailed = true;
            if (debugLoot)
                sLog.outLoot("bot=\"%s\" target_guid=%lu phase=item result=retry item_id=%u reason=native_transfer_failed",
                    bot->GetName(), guid.GetRawValue(), itemid);
            continue;
        }

        ++itemsTaken;
        if (debugLoot)
            sLog.outLoot("bot=\"%s\" target_guid=%lu phase=item result=taken item_id=%u item_name=\"%s\" count=%u",
                bot->GetName(), guid.GetRawValue(), itemid, proto->Name1, itemCountTaken);

        if (debugLoot)
        {
            for (uint8 questSlot = 0; questSlot < MAX_QUEST_LOG_SIZE; ++questSlot)
            {
                uint32 questId = bot->GetQuestSlotQuestId(questSlot);
                Quest const* quest = sObjectMgr.GetQuestTemplate(questId);
                if (!quest)
                    continue;

                QuestStatusData const& questStatus = bot->getQuestStatusMap()[questId];
                for (uint8 objective = 0; objective < QUEST_OBJECTIVES_COUNT; ++objective)
                {
                    if (quest->ReqItemId[objective] != itemid)
                        continue;

                    sLog.outLoot("bot=\"%s\" target_guid=%lu phase=quest result=progress quest_id=%u quest_name=\"%s\" item_id=%u count=%u required=%u",
                        bot->GetName(), guid.GetRawValue(), questId, quest->GetTitle().c_str(), itemid,
                        questStatus.m_itemcount[objective], quest->ReqItemCount[objective]);
                }
            }
        }

        if (proto->Quality > ITEM_QUALITY_NORMAL && !urand(0, 50) && ai->HasStrategy("emote", BotState::BOT_STATE_NON_COMBAT)) ai->PlayEmote(TEXTEMOTE_CHEER);
        if (proto->Quality >= ITEM_QUALITY_RARE && !urand(0, 1) && ai->HasStrategy("emote", BotState::BOT_STATE_NON_COMBAT)) ai->PlayEmote(TEXTEMOTE_CHEER);

        if (requester && (ai->HasStrategy("debug", BotState::BOT_STATE_NON_COMBAT) || (requester->GetMapId() != bot->GetMapId() || WorldPosition(requester).sqDistance2d(bot) > (sPlayerbotAIConfig.sightDistance * sPlayerbotAIConfig.sightDistance))))
        {
            std::map<std::string, std::string> args;
            args["%item"] = chat->formatItem(itemQualifier);
            ai->TellPlayerNoFacing(requester, BOT_TEXT2("loot_command", args), PlayerbotSecurityLevel::PLAYERBOT_SECURITY_ALLOW_ALL, false);
        }

        sPlayerbotAIConfig.logEvent(ai, "StoreLootAction", proto->Name1, std::to_string(proto->ItemId));

        BroadcastHelper::BroadcastLootingItem(ai, bot, proto, itemQualifier);
    }

    LootObjectStack* lootStack = AI_VALUE(LootObjectStack*, "available loot");
    if (guid.IsCreature() && !transferFailed)
        lootStack->Ignore(guid, sPlayerbotAIConfig.lootTargetInspectDelay);
    else if (transferFailed)
        lootStack->Ignore(guid, sPlayerbotAIConfig.lootTargetRetryDelay);
    else
        lootStack->Remove(guid);
    RESET_AI_VALUE(LootObject, "loot target");
    RESET_AI_VALUE2(bool, "should loot object", std::to_string(guid.GetRawValue()));

    if (debugLoot)
        sLog.outLoot("bot=\"%s\" target_guid=%lu phase=complete result=%s items_taken=%u gold=%u ignore_seconds=%u",
            bot->GetName(), guid.GetRawValue(), transferFailed ? "retry" : "inspected",
            itemsTaken, gold, transferFailed ? sPlayerbotAIConfig.lootTargetRetryDelay :
                (guid.IsCreature() ? sPlayerbotAIConfig.lootTargetInspectDelay : 0));

    // release loot
    WorldPacket packet(CMSG_LOOT_RELEASE, 8);
    packet << guid;
    bot->GetSession()->HandleLootReleaseOpcode(packet);

    ai->AccelerateRespawn(guid);

    return true;
}

bool StoreLootAction::IsLootAllowed(ItemQualifier& itemQualifier, PlayerbotAI *ai)
{
    AiObjectContext *context = ai->GetAiObjectContext();
    
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemQualifier.GetId());
    if (!proto)
        return false;

    std::set<uint32>& lootItems = AI_VALUE(std::set<uint32>&, "always loot list");
    if (lootItems.find(itemQualifier.GetId()) != lootItems.end())
        return true;

    std::set<uint32>& skipItems = AI_VALUE(std::set<uint32>&, "skip loot list");
    if (skipItems.find(itemQualifier.GetId()) != skipItems.end())
        return false;

    uint32 max = proto->MaxCount;
    if (max > 0 && ai->GetBot()->HasItemCount(itemQualifier.GetId(), max, true))
        return false;

    if (proto->StartQuest)
    {
        if (sPlayerbotAIConfig.syncQuestWithPlayer)
            return false; //Quest is autocomplete for the bot so no item needed.
        else
            return true;
    }

    for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
    {
        uint32 entry = ai->GetBot()->GetQuestSlotQuestId(slot);
        Quest const* quest = sObjectMgr.GetQuestTemplate(entry);
        if (!quest)
            continue;

        for (int i = 0; i < 4; i++)
        {
            if (quest->ReqItemId[i] == itemQualifier.GetId())
            {
                if (ai->GetMaster() && sPlayerbotAIConfig.syncQuestWithPlayer)
                    return false; //Quest is autocomplete for the bot so no item needed.

                if (AI_VALUE2(uint32, "item count", proto->Name1) >= quest->ReqItemCount[i])
                    return false;
            }
        }
    }

    //if (proto->Bonding == BIND_QUEST_ITEM ||  //Still testing if it works ok without these lines.
    //    proto->Bonding == BIND_QUEST_ITEM1 || //Eventually this has to be removed.
    //    proto->Class == ITEM_CLASS_QUEST)
    //{

    bool canLoot = LootStrategyValue::CanLoot(itemQualifier, ai);

    //if (canLoot && proto->Bonding == BIND_WHEN_PICKED_UP && ai->HasActivePlayerMaster())
    //    canLoot = sPlayerbotAIConfig.IsInRandomAccountList(sObjectMgr.GetPlayerAccountIdByGUID(ai->GetBot()->GetObjectGuid()));

    return canLoot;
}

bool ReleaseLootAction::Execute(Event& event)
{
    std::list<ObjectGuid> gos = context->GetValue<std::list<ObjectGuid> >("nearest game objects no los")->Get();
    for (std::list<ObjectGuid>::iterator i = gos.begin(); i != gos.end(); i++)
    {
        WorldPacket packet(CMSG_LOOT_RELEASE, 8);
        packet << *i;
        bot->GetSession()->HandleLootReleaseOpcode(packet);
    }

    std::list<ObjectGuid> corpses = context->GetValue<std::list<ObjectGuid> >("nearest corpses")->Get();
    for (std::list<ObjectGuid>::iterator i = corpses.begin(); i != corpses.end(); i++)
    {
        WorldPacket packet(CMSG_LOOT_RELEASE, 8);
        packet << *i;
        bot->GetSession()->HandleLootReleaseOpcode(packet);
    }

    return true;
}
