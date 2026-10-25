from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1])
def function(source,signature):
    start=source.index(signature);begin=source.index('{',start);end=begin+1;depth=1
    while depth:
        if source[end]=='{':depth+=1
        elif source[end]=='}':depth-=1
        end+=1
    return source[start:end]+'\n'
actions=root/'modules/mod-playerbots/src/playerbot/strategy/actions'
s=(actions/'GossipHelloAction.cpp').read_text()
(out/'NativeGossipAction.inc').write_text(s[s.index('bool GossipHelloAction::Execute'):])
s=(actions/'GossipHelloAction.h').read_text().replace('#pragma once','').replace('#include "GenericActions.h"','')
(out/'NativeGossipActionClass.inc').write_text(s)
s=(root/'src/game/Handlers/NPCHandler.cpp').read_text()
(out/'NativeGossipHandlers.inc').write_text(function(s,'void WorldSession::HandleGossipHelloOpcode(')+function(s,'void WorldSession::HandleGossipSelectOptionOpcode('))
s=(root/'src/scripts/miscellaneous/random_scripts_3.cpp').read_text()
(out/'NativeMaltimor.inc').write_text(function(s,'bool GossipHello_npc_maltimor_gartside(')+function(s,'bool GossipSelect_npc_maltimor_gartside('))
# Registration and native module/Lua hooks remain in the real dispatch path.
assert 'newscript->pGossipHello = &GossipHello_npc_maltimor_gartside;' in s
s=(root/'src/game/ScriptMgr.cpp').read_text()
assert 'pPlayer->PlayerTalkClass->ClearMenus();' in function(s,'bool ScriptMgr::OnGossipHello(Player* pPlayer, Creature* pCreature)')
assert 'e->OnGossipHello(pPlayer, pCreature)' in s
