from pathlib import Path
import sys
r=Path(__file__).resolve().parents[2];out=Path(sys.argv[1])
def extract(s,marker):
    start=s.index(marker);begin=s.index('{',start);i=begin+1;depth=1
    while depth:
        if s[i]=='{':depth+=1
        elif s[i]=='}':depth-=1
        i+=1
    return s[start:i]
cpp=(r/'modules/mod-playerbots/src/playerbot/ServerFacade.cpp').read_text()
(out/'NativeMoving.inc').write_text(extract(cpp,'bool ServerFacade::isMoving('))
s=(r/'src/game/Objects/Unit.h').read_text();(out/'NativeStopped.inc').write_text(extract(s,'bool IsStopped() const'))
s=(r/'src/game/Objects/Object.h').read_text();(out/'NativeObjectMoving.inc').write_text(extract(s,'bool IsMoving() const'))
(out/'NativeMovementFlags.inc').write_text(extract(s,'enum MovementFlags\n')+';')
s=(r/'src/game/Objects/UnitDefines.h').read_text();(out/'NativeUnitState.inc').write_text(extract(s,'enum UnitState\n')+';')
s=(r/'modules/mod-playerbots/src/playerbot/PlayerbotAI.cpp').read_text();start=s.index('bool PlayerbotAI::CastSpell(uint32 spellId, GameObject*')
block=extract(s[start:],'if (sServerFacade.isMoving(bot) &&')
(out/'NativeCastStop.inc').write_text(block)
spell=(r/'src/game/Spells/Spell.cpp').read_text();a=spell.index('if (m_caster->IsPlayer() && m_timer != 0');b=spell.index('{',a)
(out/'NativeInterruptCondition.inc').write_text(spell[a+3:b].strip()[1:-1])
