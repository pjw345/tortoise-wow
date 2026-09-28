#pragma once
#include "playerbot.h"

namespace ai
{
    class ItemQualifier;

    // A quest chest can remain activated after one group member opens it.
    // The core deliberately permits later group members to open that same
    // chest so FillNotNormalLootFor can create their personal quest loot.
    bool CanOpenActivatedQuestChest(Player* bot, GameObject* go);

    class LootObject
    {
    public:
        LootObject() : skillId(0), reqSkillValue(0), reqItem(0) {}
        LootObject(Player* bot, ObjectGuid guid);
        LootObject(const LootObject& other);

    public:
        bool IsEmpty() { return !guid; }
        bool IsLootPossible(Player* bot, bool* suppressRediscovery = nullptr);
        void Refresh(Player* bot, ObjectGuid guid, bool debug = false);
        WorldObject* GetWorldObject(Player* bot);
        ObjectGuid guid;

        uint32 skillId;
        uint32 reqSkillValue;
        uint32 reqItem;
    };

    class LootTarget
    {
    public:
        LootTarget(ObjectGuid guid);
        LootTarget(LootTarget const& other);

    public:
        LootTarget& operator=(LootTarget const& other);
        bool operator< (const LootTarget& other) const;

    public:
        ObjectGuid guid;
        time_t asOfTime;
    };

    class LootTargetList : public std::set<LootTarget>
    {
    public:
        void shrink(time_t fromTime);
    };

    class LootObjectStack
    {
    public:
        LootObjectStack(Player* bot) : bot(bot) {}

    public:
        bool Add(ObjectGuid guid);
        void Remove(ObjectGuid guid);
        void Ignore(ObjectGuid guid, uint32 seconds);
        void Clear();
        bool CanLoot(float maxDistance);
        LootObject GetLoot(float maxDistance = 0);

    public:
        std::vector<LootObject> OrderByDistance(float maxDistance = 0);

    private:
        void PruneIgnored();

        Player* bot;
        LootTargetList availableLoot;
        std::map<ObjectGuid, time_t> ignoredLoot;
    };

};
