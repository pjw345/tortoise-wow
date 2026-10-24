#pragma once
#include <string>
class Player;
class PlayerbotAI;
namespace ai { namespace questsync
{
    bool Authorized(PlayerbotAI* ai, Player* requester);
    bool SendQueuedSnapshot(PlayerbotAI* ai, std::string const& message);
    bool SendSnapshot(PlayerbotAI* ai, Player* requester, std::string const& message);
}}
