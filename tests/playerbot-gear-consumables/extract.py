from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
base=root/'modules/mod-playerbots/src/playerbot'
def method(path,marker):
 s=path.read_text(encoding='utf-8-sig');start=s.index(marker);a=s.index('{',start);depth=1;b=a+1
 # These selected methods contain no unmatched brace literals/comments.
 while depth:
  if s[b]=='{':depth+=1
  if s[b]=='}':depth-=1
  b+=1
 return s[start:b]+'\n'
gear=base/'strategy/values/ItemUsageValue.cpp'
pieces=[method(gear,m) for m in ['bool IsBetterEquipment(', 'Item* FindQualifiedEquipment(', 'InventoryResult ItemUsageValue::GetEquipDestination(', 'ItemUsage ItemUsageValue::QueryItemUsageForEquip(']]
pieces.insert(0,method(base/'RandomItemMgr.cpp','bool RandomItemMgr::ShouldEquipArmorForSpec('))
pieces.append(method(base/'strategy/actions/EquipAction.cpp','bool EquipUpgradesAction::Execute('))
pieces.append(method(base/'strategy/actions/EquipAction.cpp','bool EquipAction::EquipItem(PlayerbotAI*'))
(out/'NativeGear.inc').write_text('\n'.join(pieces),encoding='utf-8')
s=(base/'strategy/actions/UseConsumableAction.cpp').read_text(encoding='utf-8-sig')
s='\n'.join(l for l in s.splitlines() if not l.startswith('#include'))
h=(base/'strategy/actions/UseConsumableAction.h').read_text(encoding='utf-8-sig')
h='\n'.join(l for l in h.splitlines() if not l.startswith('#'))
# Expose selection helpers only to this fixture; method bodies stay unmodified.
h=h.replace('private:', 'public:').replace('protected:', 'public:')
(out/'NativeConsumables.inc').write_text(h+'\n'+s,encoding='utf-8')
(out/'NativeStrategyRestore.inc').write_text(method(base/'PlayerbotAI.cpp','void PlayerbotAI::ResetStrategies('),encoding='utf-8')

(out/'NativeRoll.inc').write_text(method(base/'strategy/actions/LootRollAction.cpp','RollVote RollAction::CalculateRollVote('),encoding='utf-8')
