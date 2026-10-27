#include "playerbot/playerbot.h"
#include "TradeAction.h"
#include "playerbot/strategy/ItemVisitors.h"
#include "playerbot/strategy/values/ItemCountValue.h"

using namespace ai;

bool TradeAction::Execute(Event& event)
{
    std::string text = event.getParam();
    bool const nonTradedOnly = event.getSource() == "nt";
    Player* player = nullptr;
    for (auto const& guid : chat->parseGameobjects(text))
    {
        if (guid.IsPlayer())
            player = sObjectMgr.GetPlayer(guid);
    }
    if (!player)
        player = event.getOwner() ? event.getOwner() : GetMaster();
    if (!player || (bot->GetTrader() && bot->GetTrader() != player) ||
        (player->GetTrader() && player->GetTrader() != bot))
        return false;

    IterateItemsMask mask = IterateItemsMask((uint8)IterateItemsMask::ITERATE_ITEMS_IN_EQUIP | (uint8)IterateItemsMask::ITERATE_ITEMS_IN_BAGS);
    if (nonTradedOnly && ai->InventoryParseItems(text, mask).empty())
        return false;

    bool initiated = false;
    if (!bot->GetTrader())
    {
        WorldPacket packet(CMSG_INITIATE_TRADE);
        packet << player->GetObjectGuid();
        bot->GetSession()->HandleInitiateTradeOpcode(packet);
#ifdef MANGOSBOT_ZERO
        // Turtle's native initiation creates both TradeData objects synchronously.
        // Keep its distance, eligibility and script checks authoritative, then
        // open the window before sending its first item update.
        if (bot->GetTrader() != player || player->GetTrader() != bot ||
            !bot->GetTradeData() || !player->GetTradeData())
            return false;
        WorldPacket begin(CMSG_BEGIN_TRADE);
        bot->GetSession()->HandleBeginTradeOpcode(begin);
        initiated = true;
#else
        // Other cores retain their existing initiation/acknowledgement flow.
        return true;
#endif
    }

    // A native handler may cancel a trade. Never populate a stale or different
    // partner's trade while processing the remainder of this command.
    if (bot->GetTrader() != player || player->GetTrader() != bot ||
        !bot->GetTradeData() || !player->GetTradeData())
        return false;

    uint32 copper = nonTradedOnly ? 0 : chat->parseMoney(text);
    if (copper > 0)
    {
        WorldPacket packet(CMSG_SET_TRADE_GOLD, 4);
        packet << copper;
        bot->GetSession()->HandleSetTradeGoldOpcode(packet);
    }

    size_t pos = text.rfind(" ");
    int count = pos != std::string::npos ? atoi(text.substr(pos + 1).c_str()) : 1;
    int traded = 0;
    bool offered = false;
    // Native initiation runs script hooks; do not retain inventory pointers
    // across that boundary. Resolve the current owned items again now.
    std::list<Item*> found = ai->InventoryParseItems(text, mask);
    for (Item* item : found)
    {
        TradeData* trade = bot->GetTradeData();
        if (!trade || bot->GetTrader() != player)
            break;
        int8 slot = nonTradedOnly || !item->CanBeTraded() ? TRADE_SLOT_NONTRADED : -1;
        if (item->IsInTrade() &&
            !(nonTradedOnly && trade->GetItem(TRADE_SLOT_NONTRADED) == item))
            continue;
        if (TradeItem(*item, slot))
        {
            offered = true;
            if (nonTradedOnly || (slot != TRADE_SLOT_NONTRADED && ++traded >= count))
                break;
        }
    }
    return nonTradedOnly ? offered : offered || initiated || copper > 0;
}

bool TradeAction::TradeItem(const Item& item, int8 slot)
{
    TradeData* trade = bot->GetTradeData();
    if (!trade || !bot->GetTrader() || !bot->GetTrader()->GetTradeData())
        return false;
    Item* itemPtr = const_cast<Item*>(&item);
    int8 tradeSlot = -1;
    if (slot == TRADE_SLOT_NONTRADED)
    {
        Item* current = trade->GetItem(TRADE_SLOT_NONTRADED);
        if (current)
            return current == itemPtr;
        tradeSlot = TRADE_SLOT_NONTRADED;
    }
    else
    {
        if (slot >= 0 && slot < TRADE_SLOT_TRADED_COUNT && !trade->GetItem(TradeSlots(slot)))
            tradeSlot = slot;
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot == -1; ++i)
        {
            if (trade->GetItem(TradeSlots(i)) == itemPtr)
            {
                WorldPacket packet(CMSG_CLEAR_TRADE_ITEM, 1);
                packet << i;
                bot->GetSession()->HandleClearTradeItemOpcode(packet);
                return bot->GetTradeData() && !bot->GetTradeData()->GetItem(TradeSlots(i));
            }
        }
        for (uint8 i = 0; i < TRADE_SLOT_TRADED_COUNT && tradeSlot == -1; ++i)
        {
            if (!trade->GetItem(TradeSlots(i)))
                tradeSlot = i;
        }
    }
    if (tradeSlot == -1)
        return false;

    // Let the native handler validate ownership/bank/duplicate restrictions and
    // reset acceptance/enchant state. Prefilling TradeData would reject this
    // same item as a duplicate before those native side effects could run.
    WorldPacket packet(CMSG_SET_TRADE_ITEM, 3);
    packet << uint8(tradeSlot) << uint8(item.GetBagSlot()) << uint8(item.GetSlot());
    bot->GetSession()->HandleSetTradeItemOpcode(packet);
    return bot->GetTradeData() && bot->GetTradeData()->GetItem(TradeSlots(tradeSlot)) == itemPtr;
}
