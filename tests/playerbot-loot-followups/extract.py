from pathlib import Path
import sys
import re

root = Path(__file__).resolve().parents[2] / 'modules/mod-playerbots/src/playerbot'
def block(text, marker):
    masked = re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',
                    lambda match: ' ' * len(match[0]), text)
    start = text.index(marker)
    end = masked.index('{', start) + 1
    depth = 1
    while depth:
        depth += (masked[end] == '{') - (masked[end] == '}')
        end += 1
    return text[start:end]

source = (root / 'strategy/actions/LootAction.cpp').read_text(encoding='utf-8')
execute = block(source, 'bool StoreLootAction::Execute(')
release = execute.rindex('HandleLootReleaseOpcode(packet)')
assert release < execute.index('QueueNextMiningAttempt(requester, guid)')
assert 'if (!transferFailed && itemsTaken > 0)' in execute[release:]
methods = [block(source, 'bool StoreLootAction::' + name + '(') for name in
           ['IsLootExcludedByDefault', 'IsLootAllowed', 'QueueNextMiningAttempt']]
methods += [block((root / 'strategy/values/LootValues.cpp').read_text(encoding='utf-8'),
                  'bool ShouldLootObject::Calculate(')]
Path(sys.argv[1]).write_text('\n\n'.join(methods), encoding='utf-8', newline='\n')
