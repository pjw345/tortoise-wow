from pathlib import Path
import sys
core,out,trigger=map(Path,sys.argv[1:]);out.mkdir(parents=True,exist_ok=True)
text=trigger.read_text()
text='\n'.join(line for line in text.splitlines() if not line.startswith('#include') and line.strip()!='using namespace ai;')
(out/'native-triggers.inc').write_text(text)
text=(core/'modules/mod-playerbots/src/playerbot/strategy/actions/LootAction.cpp').read_text()
text=text[text.index('bool LootAction::Execute('):text.index('enum ProfessionSpells')]
(out/'native-selector.inc').write_text(text)
