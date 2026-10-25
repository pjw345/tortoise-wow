from pathlib import Path
import re,sys
r=Path(__file__).resolve().parents[2];b=r/'modules/mod-playerbots/src/playerbot/strategy';o=Path(sys.argv[1])
def block(s,marker):
 start=s.index(marker);masked=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s)
 end=masked.index('{',start)+1;depth=1
 while depth:
  depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
 return s[start:end]
action=(b/'Action.h').read_text();actions=(b/'Action.cpp').read_text();queue=(b/'Queue.cpp').read_text()
declarations=[block((b/'Strategy.h').read_text(),'enum ActionPriority')+';',block(action,'class ActionBasket\n')+';',block((b/'Queue.h').read_text(),'class Queue')+';']
(o/'native-queue-types.inc').write_text('\n'.join(declarations))
parts=[block(actions,'NextAction** NextAction::array(uint32 n, ...)')]
parts += [block(queue,m) for m in ['void Queue::Push(', 'ActionNode* Queue::Pop(', 'ActionBasket* Queue::Peek(', 'int Queue::Size(']]
loot=(b/'generic/LootNonCombatStrategy.cpp').read_text();follow=(b/'generic/FollowMasterStrategy.cpp').read_text();add=(b/'actions/AddLootAction.cpp').read_text()
parts += [block(loot,'void GatherStrategy::InitNonCombatTriggers('),block(loot,'void LootNonCombatStrategy::InitNonCombatTriggers('),block(follow,'void FollowMasterStrategy::InitNonCombatTriggers('),block(add,'bool AddAllLootAction::isUseful('),block(add,'bool AddGatheringLootAction::isUseful(')]
(o/'native-scheduling.inc').write_text('\n'.join(parts))
