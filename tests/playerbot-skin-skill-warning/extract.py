from pathlib import Path
import sys
core,out=map(Path,sys.argv[1:3]);out.mkdir(exist_ok=True,parents=True)
root=core/'modules/mod-playerbots/src/playerbot'
def block(text,marker):
 start=text.index(marker);cursor=text.index('{',start);end=cursor+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
source=(root/'strategy/actions/AddLootAction.cpp').read_text()
(out/'native-warning.inc').write_text(block(source,'void AddGatheringLootAction::TellInsufficientSkinningSkill('))
entrance=(Path(sys.argv[3]).read_text()if len(sys.argv)>3 and sys.argv[3]else source)
entrance=block(entrance,'bool AddGatheringLootAction::AddLoot(')
entrance=entrance[:entrance.index('    if (loot.skillId == SKILL_NONE)')]
(out/'native-entrance.inc').write_text(entrance+'    ++normalAdmissionCalls; return true;\n}\n')
header=(root/'LootObjectStack.h').read_text()
(out/'native-list.h').write_text(header[header.index('    class LootTarget\n'):header.index('    class LootObjectStack\n')])
queue=(root/'LootObjectStack.cpp').read_text()
(out/'native-list.inc').write_text(queue[queue.index('LootTarget::LootTarget('):queue.index('LootObject::LootObject(Player*')])
phase=queue[queue.index('void LootObject::Refresh('):queue.index('    GameObject* go = ai->GetGameObject(guid);',queue.index('void LootObject::Refresh('))]
(out/'native-phase.inc').write_text(phase+'}\n')
assert 'TellInsufficientSkinningSkill' in (root/'strategy/actions/AddLootAction.h').read_text()
assert 'SPELL_FAILED_LOW_CASTLEVEL' in (core/'src/game/Spells/Spell.cpp').read_text()
