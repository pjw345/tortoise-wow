#pragma once
#include "QuestSyncProtocol.h"
class PlayerbotAI;
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
    bool SendQueuedSnapshot(PlayerbotAI* ai, std::string const& message);
}}
