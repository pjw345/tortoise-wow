from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
base = root / 'modules/mod-playerbots/src/playerbot'
s = (base / 'PlayerbotAI.cpp').read_text()
start = s.index('void PlayerbotAI::BeginConsumableRecovery(')
end = s.index('void PlayerbotAI::StartGatheringPings(', start)
recovery = s[start:end]
source = (base / 'strategy/actions/UseItemAction.cpp').read_text()
start = source.index('bool UseAction::UseGameObject(')
end = source.index('    else if(gameObject->GetGoType() == GAMEOBJECT_TYPE_DOOR)', start)
loot = source[start:end] + '    return false;\n}\n'
Path(sys.argv[1], 'NativeMethods.inc').write_text(recovery + loot)
# Check all entry points register recovery, not just the test harness method.
assert 'ai->BeginConsumableRecovery(IsFood(proto), IsDrink(proto))' in source
header = (base / 'strategy/actions/UseItemAction.h').read_text()
assert header.count('ai->BeginConsumableRecovery(') == 4
update = s[s.index('void PlayerbotAI::UpdateAI('):s.index('void PlayerbotAI::UpdateAIInternal(')]
assert update.index('UpdateConsumableRecovery();') < update.index('CleanupExpiredValuesIfDue();')
assert 'recoveringFood = recoveringDrink = false;' in s[s.index('void PlayerbotAI::Reset('):s.index('void PlayerbotAI::Reset(')+160]

start = s.index('void PlayerbotAI::StartGatheringPings(')
end = s.index('void PlayerbotAI::Ping(', start)
repeats = s[start:end]
reveal = (base / 'strategy/actions/RevealGatheringItemAction.cpp').read_text()
start = reveal.index('bool ai::CanRevealGatheringItem(')
end = reveal.index('bool RevealGatheringItemAction::Execute(', start)
Path(sys.argv[1], 'NativeGatheringPings.inc').write_text(reveal[start:end] + repeats)
assert 'UpdateGatheringPings(elapsed);' in update
assert 'ai->HasPendingGatheringPings()' in reveal
assert 'ai->StartGatheringPings(go);' in reveal
