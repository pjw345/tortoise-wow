from pathlib import Path
import re,sys
core,out=map(Path,sys.argv[1:3]);out.mkdir(parents=True,exist_ok=True)
root=core/'modules/mod-playerbots/src/playerbot'
header=(root/'LootObjectStack.h').read_text();header=header[header.index('    class LootTarget\n'):header.rfind('\n};')]
(out/'native-queue.h').write_text(header)
source=(root/'LootObjectStack.cpp').read_text()
constructors=source[source.index('LootTarget::LootTarget('):source.index('LootObject::LootObject(Player*')]
queue=source[source.index('bool LootObjectStack::Add('):]
if len(sys.argv)>3 and sys.argv[3]:
    old=Path(sys.argv[3]).read_text();old=old[old.index('bool LootObjectStack::Add('):old.index('void LootObjectStack::Remove(')]
    queue=old+queue[queue.index('void LootObjectStack::Remove('):]
constant=re.search(r'^#define MAX_LOOT_OBJECT_COUNT \d+$',source,re.M)[0]
(out/'native-queue.inc').write_text(constant+'\n'+constructors+'\n'+queue)
native=(core/'src/game/Objects/Creature.h').read_text()
(out/'native-skin-rights.inc').write_text(next(x.strip() for x in native.splitlines() if 'bool IsSkinnableBy(Player const* player) const' in x))

phase_source=source
if len(sys.argv)>4 and sys.argv[4]:
    phase_source=Path(sys.argv[4]).read_text()
phase=phase_source[phase_source.index('void LootObject::Refresh('):phase_source.index('    GameObject* go = ai->GetGameObject(guid);',phase_source.index('void LootObject::Refresh('))]
(out/'native-creature-refresh.inc').write_text(phase+'}\n')
