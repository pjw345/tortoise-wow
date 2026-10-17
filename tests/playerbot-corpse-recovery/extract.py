from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]/'modules/mod-playerbots/src/playerbot/strategy/actions'
s=(root/'ReviveFromCorpseAction.cpp').read_text();a=s.index('bool ReviveFromCorpseAction::IsSafeToAutoResurrect(');b=s.index('bool FindCorpseAction::Execute(',a)
Path(sys.argv[1],'NativeReclaim.inc').write_text(s[a:b])
a=b;b=s.index('    if (master && !manualCorpseRun)',a)
Path(sys.argv[1],'NativeRegroup.inc').write_text(s[a:b]+'    ++legacyCalls; return true;\n}\n')
s=(root/'ReleaseSpiritAction.h').read_text();a=s.index('    class SelfResurrectAction');a=s.index('        bool Execute(Event& event)',a);b=s.index('        bool isPossible()',a)
Path(sys.argv[1],'NativeSelfRes.inc').write_text(s[a:b].replace(' override',''))
