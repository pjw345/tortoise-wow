from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2];out=Path(sys.argv[1]);out.mkdir(exist_ok=True,parents=True)
def block(text,marker):
 start=text.index(marker);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
base=root/'modules/mod-playerbots/src/playerbot/strategy/actions'
move=Path(sys.argv[2]).read_text() if len(sys.argv)>2 and sys.argv[2] else (base/'MovementActions.cpp').read_text()
loot=Path(sys.argv[3]).read_text() if len(sys.argv)>3 and sys.argv[3] else (base/'LootAction.cpp').read_text()
(out/'native.inc').write_text(block(move,'bool MoveToLootAction::Execute(')+'\n'+block(loot,'bool OpenLootAction::Execute('))
