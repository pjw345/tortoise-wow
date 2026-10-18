from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]/'modules/mod-playerbots/src/playerbot/strategy/actions'
s=(root/'ReviveFromCorpseAction.cpp').read_text();a=s.index('bool ReviveFromCorpseAction::IsSafeToAutoResurrect(');b=s.index('bool FindCorpseAction::Execute(',a)
Path(sys.argv[1],'NativeReclaim.inc').write_text(s[a:b])
a=b;b=s.index('    if (master && !manualCorpseRun)',a)
Path(sys.argv[1],'NativeRegroup.inc').write_text(s[a:b]+'    ++legacyCalls; return true;\n}\n')
ai=(root.parents[1]/'PlayerbotAI.cpp').read_text()
a=ai.index('    if (IsStateActive(BotState::BOT_STATE_DEAD) && sServerFacade.IsAlive(bot))')
assert ai.index('GetValue<bool>("spirit healer requested")->Set(false)',a) < ai.index('ChangeEngine(BotState::BOT_STATE_NON_COMBAT)',a)
revive=(root/'ReviveFromCorpseAction.cpp').read_text()
a=revive.index('    if (event.getSource() == "revive")');b=revive.index('    botdiag::BotActionLog',a)
Path(sys.argv[1],'NativeGraveyardIntent.inc').write_text('void RequestGraveyard(PlayerbotAI* ai, Event& event) {\n'+revive[a:b]+'}\n')
dead=(root.parent/'values/DeadValues.cpp').read_text();a=dead.index('    if (ai->HasActivePlayerMaster())',dead.index('bool ShouldSpiritHealerValue::Calculate('));b=dead.index('    //Nothing to lose',a)
Path(sys.argv[1],'NativeGraveyardContinue.inc').write_text('bool ContinueGraveyard(PlayerbotAI* ai) {\n'+dead[a:b]+'return false;\n}\n')
s=(root/'ReleaseSpiritAction.h').read_text();a=s.index('    class CorpseRunAction');a=s.index('        virtual bool Execute(Event& event)',a);b=s.index('\n    };',a)
Path(sys.argv[1],'NativeCorpseCommand.inc').write_text(s[a:b].replace('virtual ','').replace(' override',''))
s=(root/'ReleaseSpiritAction.h').read_text();a=s.index('    class SelfResurrectAction');a=s.index('        bool Execute(Event& event)',a);b=s.index('        bool isPossible()',a)
Path(sys.argv[1],'NativeSelfRes.inc').write_text(s[a:b].replace(' override',''))
