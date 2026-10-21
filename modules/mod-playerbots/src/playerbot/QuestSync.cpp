#include "playerbot/playerbot.h"
#include "QuestSync.h"
#include "QuestSyncProtocol.h"

bool PlayerbotAI::AdmitQuestSyncSend()
{
    return ai::questsync::Admit(questSyncLastSent, time(0));
}

namespace ai { namespace questsync
{
    bool SendQueuedSnapshot(PlayerbotAI* ai, std::string const& message)
    {
        size_t split = message.find(':');
        uint32_t low;
        if (split == std::string::npos || !Number(message.substr(0, split), low) || !low) return false;
        Player* requester = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, low));
        return SendSnapshot(ai, requester, message.substr(split + 1));
    }

    bool Authorized(PlayerbotAI* ai, Player* requester)
    {
        return requester && IsRealPlayer(requester) && ai->GetMaster() == requester &&
            ai->GetBot()->GetGroup() && ai->GetBot()->GetGroup() == requester->GetGroup();
    }

    // Called by the existing queued quest action on the bot's serialized owner.
    // No DB read, foreign player's quest traversal, mutation or subscription.
    bool SendSnapshot(PlayerbotAI* ai, Player* requester, std::string const& message)
    {
        Player* bot = ai->GetBot();
        Request request;
        if (!Authorized(ai, requester) || !ParseRequest(message, bot->GetName(), request) ||
            !ai->AdmitQuestSyncSend()) return false;
        std::set<uint32> active, wanted(request.questIds.begin(), request.questIds.end());
        for (uint16 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
            if (uint32 id = bot->GetQuestSlotQuestId(slot)) { active.insert(id); wanted.insert(id); }
        if (wanted.size() > MaxRows) return false;
        std::ostringstream snapshot;
        auto const& states = bot->getQuestStatusMap();
        for (uint32 id : wanted)
        {
            Quest const* quest = sObjectMgr.GetQuestTemplate(id);
            if (!quest) continue;
            auto found = states.find(id);
            QuestStatusData const* state = found == states.end() ? nullptr : &found->second;
            char status = active.count(id) ? (bot->GetQuestStatus(id) == QUEST_STATUS_COMPLETE ? 'C' :
                bot->GetQuestStatus(id) == QUEST_STATUS_FAILED ? 'F' : 'I') :
                (state && state->m_rewarded ? 'R' : 'N');
            snapshot << id << '|' << status << '|' << quest->GetQuestLevel() << '|'
                << Hex(quest->GetTitle()) << '|';
            if (active.count(id) && state)
            {
                bool first = true;
                auto objective = [&](char kind, uint32 index, int64 entry, uint32 current, uint32 required)
                {
                    if (!first) snapshot << ';';
                    first = false;
                    snapshot << kind << ',' << index << ',' << entry << ',' << current << ',' << required;
                };
                for (uint32 i = 0; i < QUEST_OBJECTIVES_COUNT; ++i)
                {
                    if (quest->ReqItemId[i] && quest->ReqItemCount[i])
                        objective('I', i + 1, quest->ReqItemId[i], state->m_itemcount[i], quest->ReqItemCount[i]);
                    if (quest->ReqCreatureOrGOId[i] && quest->ReqCreatureOrGOCount[i])
                        objective(quest->ReqCreatureOrGOId[i] < 0 ? 'G' : 'K', i + 1,
                            quest->ReqCreatureOrGOId[i], state->m_creatureOrGOcount[i], quest->ReqCreatureOrGOCount[i]);
                }
                if (quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
                    objective('E', 1, 0, state->m_explored ? 1 : 0, 1);
                int32 money = quest->GetRewOrReqMoney();
                if (money < 0)
                    objective('M', 1, 0, bot->GetMoney(), uint32(-int64(money)));
            }
            snapshot << '\n';
        }
        // Native direct AddOn delivery: LANG_ADDON, own recipient identity, no
        // guild broadcast or human-chat response. Unchanged snapshots cost one packet.
        for (auto const& packet : Pack(request, snapshot.str()))
            requester->SendAddonMessage(Prefix, packet);
        return true;
    }
}}
