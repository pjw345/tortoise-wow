"""Compile the actual native attachment/cost methods against diagnostic-only fakes."""
from pathlib import Path
import sys

source = (Path(__file__).resolve().parents[2] / 'src/game/BattleLog.cpp').read_text(encoding='utf-8')
methods = []
for signature in ('bool AttachCast(', 'void Resource(', 'void ItemSpent('):
    start = source.index(signature)
    opening = source.index('{', start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    methods.append(source[start:end])
Path(sys.argv[1]).write_text('\n'.join(methods) + '\n', encoding='utf-8', newline='\n')

start=source.index('void Observe(Player* p)');opening=source.index('{',start);end=opening+1;depth=1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
Path(sys.argv[1]).with_name('ObserveMethod.h').write_text(source[start:end],encoding='utf-8',newline='\n')
