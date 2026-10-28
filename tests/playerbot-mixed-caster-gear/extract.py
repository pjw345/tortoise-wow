from pathlib import Path
import sys,re
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1]);out.mkdir(exist_ok=True,parents=True)
source=Path(sys.argv[2]).read_text() if len(sys.argv)>2 and sys.argv[2] else (root/'modules/mod-playerbots/src/playerbot/RandomItemMgr.cpp').read_text()
def block(text,marker):
 start=text.index(marker);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
method=block(source,'uint32 RandomItemMgr::CalculateStatWeight(')
prelude=method[method.index('    uint32 specType'):method.index('#ifdef MANGOSBOT_TWO')]
flags=re.findall(r'    bool (?:noCaster|hasMana) = [^;]+;',method)
assert len(flags)==4
nativeflags='#ifdef MANGOSBOT_TWO\n'+'\n'.join(flags[:2])+'\n#else\n'+'\n'.join(flags[2:])+'\n#endif\n'
basic=method[method.index('    int32 basicStatsWeight'):method.index('    // check defensive stats')]
defense=method[method.index('    uint32 defenseStats'):method.index('    // check weapon dps')]
tail=method[method.index('    // check for caster item'):]
# Stat-only, unenchanted armor adapter: skipped spell/weapon/socket paths have
# zero inputs, matching the two observed boot templates. Qualification and
# comparison execute native bodies; this is not the whole CalculateStatWeight.
adapter='uint32 RandomItemMgr::StatOnlyWeight(uint8 playerclass,uint8 spec,const ItemPrototype*proto){\n'+prelude+nativeflags+'bool isWhitelist=false;ItemSpecType itSpec;\n'+basic+defense+'uint32 socketBonus=0;\n'+tail
gear=(root/'modules/mod-playerbots/src/playerbot/strategy/values/ItemUsageValue.cpp').read_text()
(out/'native.inc').write_text(block(source,'uint32 RandomItemMgr::CalculateSingleStatWeight(')+'\n'+adapter+'\n'+block(gear,'bool IsBetterEquipment(')+'\n')
