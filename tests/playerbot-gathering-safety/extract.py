from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]/'modules/mod-playerbots/src/playerbot'
def block(text,marker):
 start=text.index(marker);p=text.index('{',start);end=p+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
s=(root/'strategy/actions/AddLootAction.cpp').read_text(encoding='utf-8')
parent=block(s,'bool AddAllLootAction::AddLoot(')
assert 'loot.IsApproachSafe(bot, &blocker)' in parent
assert parent.index('loot.IsApproachSafe(bot, &blocker)') < parent.index('->Add(guid)')
safety=block((root/'LootObjectStack.cpp').read_text(encoding='utf-8'),'bool LootObject::IsApproachSafe(')
safety+='\n'+block((root/'LootObjectStack.cpp').read_text(encoding='utf-8'),'bool LootObject::IsWorldObjectApproachSafe(')
Path(sys.argv[1]).write_text(safety+'\n'+block(s,'bool AddGatheringLootAction::AddLoot(')+'\n',encoding='utf-8',newline='\n')
