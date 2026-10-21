#include "QuestSyncProtocol.h"
#include "QuestSync.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <map>
#include <utility>
using uint16 = uint16_t; using uint32 = uint32_t; using int32 = int32_t; using int64 = int64_t;
constexpr uint32 MAX_QUEST_LOG_SIZE = 25, QUEST_OBJECTIVES_COUNT = 4;
enum { QUEST_STATUS_COMPLETE = 1, QUEST_STATUS_INCOMPLETE = 3, QUEST_STATUS_FAILED = 5 };
enum { QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT = 2, HIGHGUID_PLAYER = 0 };
struct ObjectGuid { uint32 value; ObjectGuid(int, uint32 id) : value(id) {} };
struct QuestStatusData
{
    int m_status = 3; bool m_rewarded = false, m_explored = false;
    std::array<uint32, 4> m_itemcount{}, m_creatureOrGOcount{};
};
struct Quest
{
    std::string title; uint32 level = 20; bool event = false; int32 money = 0;
    std::array<uint32, 4> ReqItemId{}, ReqItemCount{}, ReqCreatureOrGOCount{};
    std::array<int32, 4> ReqCreatureOrGOId{};
    uint32 GetQuestLevel() const { return level; }
    std::string const& GetTitle() const { return title; }
    bool HasSpecialFlag(int) const { return event; }
    int32 GetRewOrReqMoney() const { return money; }
};
class Player
{
public:
    std::string name; bool human = false; int* group = nullptr; uint32 money = 0;
    std::array<uint32, 25> slots{}; std::map<uint32, QuestStatusData> states;
    std::vector<std::string> messages;
    std::string GetName() const { return name; }
    int* GetGroup() const { return group; }
    uint32 GetQuestSlotQuestId(uint16 slot) const { return slots[slot]; }
    auto const& getQuestStatusMap() const { return states; }
    int GetQuestStatus(uint32 id) const { return states.at(id).m_status; }
    uint32 GetMoney() const { return money; }
    void SendAddonMessage(std::string const& prefix, std::string const& body)
    { if (prefix != "PFQBOT") std::abort(); messages.push_back(body); }
};
struct ObjectMgr
{
    std::map<uint32, Quest> quests;
    std::map<uint32, Player*> players;
    Quest const* GetQuestTemplate(uint32 id) const { auto i = quests.find(id); return i == quests.end() ? nullptr : &i->second; }
    Player* GetPlayer(ObjectGuid id) const { auto i = players.find(id.value); return i == players.end() ? nullptr : i->second; }
} sObjectMgr;
bool IsRealPlayer(Player* player) { return player && player->human; }
class PlayerbotAI
{
public:
    Player* bot; Player* master;
    std::atomic<time_t> questSyncLastSent{0};
    Player* GetBot() { return bot; } Player* GetMaster() { return master; }
    bool AdmitQuestSyncSend();
};
#include "NativeQuestSync.inc"
void Check(bool condition, char const* message) { if (!condition) { std::cerr << message << '\n'; std::exit(1); } }
std::string Body(std::vector<std::string> const& packets)
{
    std::string body;
    for (auto const& packet : packets)
    {
        Check(packet.size() + 7 <= 255, "native packet exceeds AddOn limit");
        size_t offset = 0;
        for (int i = 0; i < 7; ++i) offset = packet.find('|', offset) + 1;
        body += packet.substr(offset);
    }
    return body;
}
int main(int argc, char** argv)
{
    using namespace ai::questsync;
    Request request;
    Check(ParseRequest("Q|1|42|Mercy|0|94,248", "Mercy", request), "valid request rejected");
    for (auto const& invalid : {"Q|2|42|Mercy|0|94", "Q|1|0|Mercy|0|94", "Q|1|42|Alon|0|94",
        "Q|1|42|Mercy|0|94,94", "Q|1|42|Mercy|0|-1", "Q|1|42|Mercy|0|4294967296",
        "Q|1|42|Mercy|0|94|extra", "Q|1|42|Mercy|xyz|94"})
        Check(!ParseRequest(invalid, "Mercy", request), "malformed request accepted");
    Check(!ParseRequest(std::string(241, 'x'), "Mercy", request), "oversize request accepted");
    std::string tooMany = "Q|1|42|Mercy|0|";
    for (int i = 1; i <= 26; ++i) tooMany += (i == 1 ? "" : ",") + std::to_string(i);
    Check(!ParseRequest(tooMany, "Mercy", request), "more than 25 history IDs accepted");
    std::atomic<time_t> last{0};
    Check(Admit(last, 100) && !Admit(last, 104) && Admit(last, 105) && !Admit(last, 99), "rate / rollback guard failed");
    ParseRequest("Q|1|42|Mercy|0|94,248", "Mercy", request);
    Check(Pack(request, "").size() == 1, "empty snapshot needs a complete reply");
    std::string maximum(ChunkSize * MaxChunks, 'x');
    Check(Body(Pack(request, maximum)) == maximum, "multipart payload changed");
    Check(Pack(request, maximum + 'x')[0].find("E|") == 0, "oversize snapshot not rejected");
    request.hash = Hash(maximum);
    Check(Pack(request, maximum)[0].find("U|") == 0, "unchanged snapshot resent");
    request.bot = std::string(48, 'x'); request.token = "4294967295"; request.hash = "0";
    Check(Body(Pack(request, maximum)) == maximum, "maximum header exceeds packet limit");
    int group = 1, other = 2;
    Player master; master.name = "Alin"; master.human = true; master.group = &group;
    Player bot; bot.name = "Mercy"; bot.group = &group; bot.slots[0] = 94;
    bot.states[94].m_itemcount[0] = 2;
    bot.states[248].m_rewarded = true;
    sObjectMgr.quests[94].title = "Shared | title";
    sObjectMgr.quests[94].ReqItemId[0] = 60688; sObjectMgr.quests[94].ReqItemCount[0] = 10;
    sObjectMgr.quests[248].title = "History";
    sObjectMgr.players[7] = &master;
    PlayerbotAI ai; ai.bot = &bot; ai.master = &master;
    auto size = bot.states.size();
    Check(SendQueuedSnapshot(&ai, "7:Q|1|42|Mercy|0|94,248"), "authorized owner request failed");
    auto body = Body(master.messages);
    if (argc > 1) { std::ofstream wire(argv[1]); for (auto const& packet : master.messages) wire << Hex(packet) << '\n'; }
    Check(body == "94|I|20|" + Hex("Shared | title") + "|I,1,60688,2,10\n248|R|20|" + Hex("History") + "|\n", "native quest counts / history wrong");
    Check(bot.states.size() == size, "snapshot mutated quest map");
    Check(!SendQueuedSnapshot(&ai, "7:Q|1|43|Mercy|0|94"), "send cooldown bypassed");
    master.group = &other; ai.questSyncLastSent = 0;
    Check(!SendQueuedSnapshot(&ai, "7:Q|1|43|Mercy|0|94"), "departed group requester accepted");
    master.group = &group; ai.master = nullptr;
    Check(!SendQueuedSnapshot(&ai, "7:Q|1|43|Mercy|0|94"), "changed master requester accepted");
    ai.master = &master; sObjectMgr.players.erase(7);
    Check(!SendQueuedSnapshot(&ai, "7:Q|1|43|Mercy|0|94"), "logged-out requester dereferenced");
    sObjectMgr.players[7] = &master;
    bot.slots[1] = 248; bot.states[248].m_status = QUEST_STATUS_INCOMPLETE;
    master.messages.clear();
    Check(SendSnapshot(&ai, &master, "Q|1|44|Mercy|0|94,248"), "repeatable active capture failed");
    Check(Body(master.messages).find("248|I|") != std::string::npos, "reward history hid active quest");
    bot.slots[2] = 250; bot.states[250].m_status = QUEST_STATUS_FAILED;
    sObjectMgr.quests[250].title = "Event and object";
    sObjectMgr.quests[250].ReqCreatureOrGOId[0] = -76; sObjectMgr.quests[250].ReqCreatureOrGOCount[0] = 1;
    sObjectMgr.quests[250].event = true; sObjectMgr.quests[250].money = -100; bot.money = 25;
    ai.questSyncLastSent = 0; master.messages.clear();
    Check(SendSnapshot(&ai, &master, "Q|1|45|Mercy|0|94,248"), "failed/event capture rejected");
    body = Body(master.messages);
    Check(body.find("250|F|") != std::string::npos && body.find("G,1,-76,0,1;E,1,0,0,1;M,1,0,25,100") != std::string::npos,
        "native failed, game-object, exploration or money objective lost");
    std::cout << "Native protocol, limits, owner/group/logout, actual objectives and history passed\n";
}
