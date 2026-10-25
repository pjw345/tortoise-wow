#pragma once
#include "QuestSyncProtocol.h"
#include <mutex>
class PlayerbotAI;
class Player;
namespace ai { namespace resources
{
    constexpr size_t MaxNodes = 32;
    struct Request { std::string token, filter; };
    inline bool ParseRequest(std::string const& text, Request& out)
    {
        if (text.size() > 40) return false;
        auto f = questsync::Split(text, ':');
        uint32_t token;
        if (f.size() != 4 || f[0] != "R" || f[1] != "1" ||
            !questsync::Number(f[2], token) || !token ||
            (f[3] != "both" && f[3] != "ore" && f[3] != "herbs")) return false;
        out = {f[2], f[3]}; return true;
    }
    inline std::vector<std::string> Pack(Request const& request, std::string const& bot,
        uint32_t zone, std::vector<std::string> const& rows)
    {
        if (rows.size() > MaxNodes || bot.empty() || bot.size() > 48) return {};
        size_t count = std::max<size_t>(1, (rows.size() + 1) / 2);
        std::vector<std::string> result;
        for (size_t i = 0; i < count; ++i)
        {
            std::string packet = "S:1:" + request.token + ":" + bot + ":" + std::to_string(zone) +
                ":" + std::to_string(i + 1) + ":" + std::to_string(count) + ":";
            if (2*i < rows.size()) packet += rows[2*i];
            if (2*i+1 < rows.size()) packet += ";" + rows[2*i+1];
            if (packet.size() > 240) return {};
            result.push_back(packet);
        }
        return result;
    }
    // Session chat only deposits bounded text. The player's own map update
    // consumes it before reading skills, visibility or the world grid.
    class RequestMailbox
    {
    public:
        bool Queue(std::string const& text, time_t now)
        {
            Request request;
            if (!ParseRequest(text, request)) return false;
            std::lock_guard<std::mutex> guard(mutex);
            if (!pending.empty() || !questsync::Admit(lastQueued, now)) return false;
            pending = text; return true;
        }
        std::string Take()
        {
            std::lock_guard<std::mutex> guard(mutex);
            std::string result; result.swap(pending); return result;
        }
    private:
        std::mutex mutex;
        std::atomic<time_t> lastQueued{0};
        std::string pending;
    };
    bool SendPlayerSnapshot(Player* player, std::string const& message);
    bool SendQueuedSnapshot(PlayerbotAI* ai, std::string const& message);
}}
