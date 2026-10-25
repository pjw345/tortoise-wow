
#include "playerbot/playerbot.h"
#include "GossipHelloAction.h"

#include "playerbot/ServerFacade.h"
#include "AI/ScriptDevAI/ScriptDevAIMgr.h"


using namespace ai;

bool GossipHelloAction::Execute(Event& event)
{
	ObjectGuid guid;

    Player* requester = event.getOwner() ? event.getOwner() : GetMaster();
	WorldPacket &p = event.getPacket();
	if (p.empty())
	{
		if (requester)
			guid = requester->GetSelectionGuid();
	}
	else
	{
		p.rpos(0);
		p >> guid;
	}

	if (!guid)
		return false;

	Creature *pCreature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
	if (!pCreature)
	{
		DEBUG_LOG("[PlayerbotMgr]: HandleMasterIncomingPacket - Received  CMSG_GOSSIP_HELLO %s not found or you can't interact with him.", guid.GetString().c_str());
		return false;
	}

    bool const autonomous = event.getSource().find("rpg action") == 0;
    if (autonomous)
    {
        // Preserve idle RPG admission. An explicit Talk must let the native
        // hello handler ask script/module/Lua hooks to build their own menus.
        GossipMenuItemsMapBounds bounds = sObjectMgr.GetGossipMenuItemsMapBounds(pCreature->GetCreatureInfo()->GossipMenuId);
        if (bounds.first == bounds.second)
            return false;
    }

    std::string text = event.getParam();
	int menuToSelect = -1;
    if (autonomous)
    {
        Creature* pCreature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);

        if (pCreature)
        {
            if (!sScriptDevAIMgr.OnGossipHello(bot, pCreature))
            {
                bot->PrepareGossipMenu(pCreature, pCreature->GetDefaultGossipMenuId());
            }
        }

        if (!ProcessGossip(requester, guid, -1))
            return false;
    }
	else if (text.empty())
	{
        WorldPacket p1;
        p1 << guid;
        bot->GetSession()->HandleGossipHelloOpcode(p1);
        sServerFacade.SetFacingTo(bot, pCreature);

        std::ostringstream out; out << "--- " << pCreature->GetName() << " ---";
        ai->TellPlayerNoFacing(requester, out.str());

        TellGossipMenus(requester);
	}
	else if (!bot->GetPlayerMenu())
	{
	    ai->TellPlayerNoFacing(requester, "I need to talk first");
	    return false;
	}
	else
	{
	    menuToSelect = atoi(text.c_str());
	    if (menuToSelect > 0) menuToSelect--;
#ifdef MANGOSBOT_ZERO
        // The native hello records this identity. A menu from another NPC
        // must not provide sender/action values for the selected creature.
        if (bot->GetSession()->GetCurrentGossipGUID() != guid)
        {
            ai->TellPlayerNoFacing(requester, "I need to talk first");
            return false;
        }
#endif
        if (!ProcessGossip(requester, guid, menuToSelect))
            return false;
	}

	bot->TalkedToCreature(pCreature->GetEntry(), pCreature->GetObjectGuid());
	return true;
}

void GossipHelloAction::TellGossipText(Player* requester, uint32 textId)
{
    if (!textId)
        return;

    GossipText const* text = sObjectMgr.GetGossipText(textId);
    if (text)
    {
        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; i++)
        {
            std::string text0 = text->Options[i].Text_0;
            if (!text0.empty()) ai->TellPlayerNoFacing(requester, text0);
            std::string text1 = text->Options[i].Text_1;
            if (!text1.empty()) ai->TellPlayerNoFacing(requester, text1);
        }
    }
}

void GossipHelloAction::TellGossipMenus(Player* requester)
{
    if (!bot->GetPlayerMenu())
        return;

     GossipMenu& menu = bot->GetPlayerMenu()->GetGossipMenu();

     if (requester)
     {
         Creature* pCreature = bot->GetNPCIfCanInteractWith(requester->GetSelectionGuid(), UNIT_NPC_FLAG_NONE);

         if (pCreature)
         {
             uint32 textId = bot->GetGossipTextId(menu.GetMenuId(), pCreature);
             TellGossipText(requester, textId);
         }
     }

    for (unsigned int i = 0; i < menu.MenuItemCount(); i++)
    {
        GossipMenuItem const& item = menu.GetItem(i);
        std::ostringstream out; out << "[" << (i+1) << "] " << item.m_gMessage;
        ai->TellPlayerNoFacing(requester, out.str());
    }
}

bool GossipHelloAction::ProcessGossip(Player* requester, ObjectGuid creatureGuid, int menuToSelect)
{
    GossipMenu& menu = bot->GetPlayerMenu()->GetGossipMenu();
    // Scripted greetings can legitimately contain no choices, including after
    // their last step. Do not underflow the random bound or index an empty menu.
    if (!menu.MenuItemCount())
    {
        if (menuToSelect != -1)
            ai->TellError(requester, "No gossip options available");
        return false;
    }

    bool noFeedback = (menuToSelect == -1);

    int actualMenuToSelect = menuToSelect;

    if (actualMenuToSelect == -1)
    {
        actualMenuToSelect = urand(0, menu.MenuItemCount() - 1);
    }

    if (actualMenuToSelect < 0 || (unsigned int)actualMenuToSelect >= menu.MenuItemCount())
    {
        ai->TellError(requester, "Unknown gossip option");
        return false;
    }
    GossipMenuItem const& item = menu.GetItem(actualMenuToSelect);
    WorldPacket p;
    std::string code;
    p << creatureGuid;
#ifdef MANGOSBOT_ZERO
    p << actualMenuToSelect;
#else
    p << menu.GetMenuId() << actualMenuToSelect;
#endif
    p << code;
    bot->GetSession()->HandleGossipSelectOptionOpcode(p);

    if(!noFeedback)
        TellGossipMenus(requester);
    return true;
}
