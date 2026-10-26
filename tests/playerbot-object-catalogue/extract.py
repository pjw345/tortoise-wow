from pathlib import Path
import sys
r=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(r/'tests/playerbot-loot-followups'))
# Shared extractor uses native brace-aware method boundaries.
import re
def block(s,marker):
 start=s.index(marker); masked=re.sub(r'//[^\n]*|/\*[\s\S]*?\*/|"(?:\\.|[^"\\])*"',lambda m:' '*len(m[0]),s)
 end=masked.index('{',start)+1;depth=1
 while depth:
  depth+=(masked[end]=='{')-(masked[end]=='}');end+=1
 return s[start:end]
shim=(r/'modules/mod-playerbots/cmangos-compat-shim.h').read_text()
proxy=block(shim,'struct CmangosGOStorageProxy')+';\ninline CmangosGOStorageProxy sGOStorage;\n'
native=(r/'src/game/ObjectMgr.h').read_text()
maximum=block(native,'uint32 GetMaxGameObjectInfoEntry() const')
loot=(r/'modules/mod-playerbots/src/playerbot/strategy/values/LootValues.cpp').read_text()
parts=['struct NativeManager : Manager { '+maximum+' };\nNativeManager sObjectMgr;\n',proxy]
parts += [block(loot,m) for m in ['DropMap* DropMapValue::Calculate()', 'std::list<uint32> EntryLootListValue::Calculate()']]
Path(sys.argv[1]).write_text('\n'.join(parts))
