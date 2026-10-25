#include "playerbot/playerbot.h"
#include "ResourceMarkers.h"
#include "QuestSync.h"
#include "Maps/GridNotifiers.h"
#include "Maps/GridNotifiersImpl.h"
#include "Maps/CellImpl.h"

namespace ai { namespace resources
{
    // Native lock table and current trained skill decide which resource this
    // bot can report. Reporting does not select, open or queue the object.
    char ResourceKind(Player* gatherer, GameObject* go, std::string const& filter)
    {
        if (!go || !go->GetGOInfo() || !go->isSpawned() || go->IsInUse() ||
            go->getLootState() != GO_READY || go->GetGoType() != GAMEOBJECT_TYPE_CHEST) return 0;
        LockEntry const* lock = sLockStore.LookupEntry(go->GetGOInfo()->GetLockId());
        if (!lock) return 0;
        for (int i = 0; i < 8; ++i)
        {
            if (lock->Type[i] != LOCK_KEY_SKILL) continue;
            uint32 skill = SkillByLockType(LockType(lock->Index[i]));
            char kind = skill == SKILL_MINING ? 'O' : skill == SKILL_HERBALISM ? 'H' : 0;
            if (!kind || (filter == "ore" && kind != 'O') || (filter == "herbs" && kind != 'H')) continue;
            if (gatherer->HasSkill(skill) &&
                gatherer->GetSkillValue(skill) >= lock->Skill[i]) return kind;
        }
        return 0;
    }

    std::vector<std::string> BuildRows(Player* viewer, Player* gatherer, Request const& request, uint32 zone)
    {
        std::vector<std::string> rows;
        float probeX = viewer->GetPositionX(), probeY = viewer->GetPositionY();
        if (gatherer->IsInWorld() && viewer->IsInWorld() && gatherer->IsAlive() &&
            gatherer->IsInMap(viewer) && !gatherer->IsBeingTeleported() && !viewer->IsBeingTeleported() &&
            Map2ZoneCoordinates(probeX, probeY, zone))
        {
            float range = std::max(0.0f, std::min(sPlayerbotAIConfig.grindDistance, sPlayerbotAIConfig.reactDistance));
            std::list<GameObject*> objects;
            MaNGOS::AllGameObjectsInRange check(gatherer, range);
            MaNGOS::GameObjectListSearcher<MaNGOS::AllGameObjectsInRange> search(objects, check);
            Cell::VisitAllObjects(gatherer, search, range);
            objects.sort([&](GameObject* a, GameObject* b) { return viewer->GetDistance(a) < viewer->GetDistance(b); });
            for (GameObject* go : objects)
            {
                char kind = ResourceKind(gatherer, go, request.filter);
                if (!kind || !gatherer->IsWithinDistInMap(go, range) || !viewer->IsWithinDistInMap(go, range) ||
                    !go->IsVisibleForInState(gatherer, gatherer, false) || !go->IsVisibleForInState(viewer, viewer, false)) continue;
                float x = go->GetPositionX(), y = go->GetPositionY();
                if (!Map2ZoneCoordinates(x, y, zone) || x < 0 || x > 100 || y < 0 || y > 100) continue;
                std::ostringstream row;
                row << go->GetObjectGuid().GetRawValue() << ',' << go->GetEntry() << ',' << kind << ',' <<
                    std::fixed << std::setprecision(3) << x << ',' << y;
                rows.push_back(row.str());
                if (rows.size() == MaxNodes) break;
            }
        }
        return rows;
    }

    bool SendPlayerSnapshot(Player* player, std::string const& message)
    {
        Request request;
        if (!player || !IsRealPlayer(player) || !ParseRequest(message, request)) return false;
        uint32 zone = player->GetZoneId();
        auto rows = BuildRows(player, player, request, zone);
        for (auto const& packet : Pack(request, player->GetName(), zone, rows)) player->SendAddonMessage("RESBOT", packet);
        return true;
    }

    bool SendQueuedSnapshot(PlayerbotAI* ai, std::string const& message)
    {
        auto split = message.find(':'); uint32_t low; Request request;
        if (split == std::string::npos || !questsync::Number(message.substr(0, split), low) || !low ||
            !ParseRequest(message.substr(split+1), request)) return false;
        Player* master = sObjectMgr.GetPlayer(ObjectGuid(HIGHGUID_PLAYER, low));
        if (!questsync::Authorized(ai, master)) return false;
        Player* bot = ai->GetBot();
        // All world reads stay on the bot's existing serialized command owner.
        // This lease suppresses legacy reveal speech/pings only while the
        // compatible addon is polling; no strategy or tracking aura is changed.
        ai->resourceMarkersUntil = time(0) + 12;
        uint32 zone = master->GetZoneId();
        auto rows = BuildRows(master, bot, request, zone);
        for (auto const& packet : Pack(request, bot->GetName(), zone, rows)) master->SendAddonMessage("RESBOT", packet);
        return true;
    }
}}
