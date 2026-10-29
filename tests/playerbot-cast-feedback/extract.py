from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2];out=Path(sys.argv[1]);out.mkdir(parents=True,exist_ok=True)
def block(text,marker):
 start=text.index(marker);end=text.index('{',start)+1;depth=1
 while depth:
  depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end]
native=(root/'src/game/Spells/SpellDefines.h').read_text();enum=native[native.index('enum SpellCastResult'):native.index('enum SpellInterruptFlags')]
(out/'enum.inc').write_text(enum)
shim=Path(sys.argv[2]).read_text() if len(sys.argv)>2 and sys.argv[2] else (root/'modules/mod-playerbots/cmangos-compat-shim.h').read_text()
(out/'result-name.inc').write_text(block(shim,'inline char const* GetSpellCastResultString('))
action=(root/'modules/mod-playerbots/src/playerbot/strategy/actions/CastCustomSpellAction.cpp').read_text()
formatter=block(action,'static std::string FormatManualCastFailure(')
guardSource=Path(sys.argv[3]).read_text() if len(sys.argv)>3 and sys.argv[3] else action
guard=guardSource[guardSource.index('    SpellCastResult checkResult'):guardSource.index('    MotionMaster& mm')]
(out/'formatter.inc').write_text(formatter)
(out/'guard.inc').write_text('bool CheckManualCast(){\n'+guard+'    ++castCalls; return true;\n}\n')
