from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1])
base = root / "modules/mod-playerbots/src/playerbot/strategy"
actions = (base / "actions/GenericSpellActions.cpp").read_text(encoding="utf-8")
header = (base / "actions/GenericSpellActions.h").read_text(encoding="utf-8")
unit = (root / "src/game/Objects/Unit.cpp").read_text(encoding="utf-8")
spell = (root / "src/game/Spells/Spell.cpp").read_text(encoding="utf-8")


def block(text, marker):
    start = text.index(marker)
    begin = text.index("{", start)
    depth = 1
    end = begin + 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]


methods = [block(unit, name) for name in [
    "bool GlobalCooldownMgr::HasGlobalCooldown(",
    "void GlobalCooldownMgr::AddGlobalCooldown(uint32",
    "void Unit::_UpdateAutoRepeatSpell()",
]]
# Use the actual strict GCD rejection block; other native cast checks are
# outside this focused fixture and must still pass in the real server.
gcd_check = block(spell, "if (strict && HasGlobalCooldown())")
methods += ["SpellCastResult Spell::CheckCast(bool strict) { " + gcd_check + " return SPELL_CAST_OK; }"]
methods += [block(actions, name) for name in [
    "NextAction** CastSpellAction::getPrerequisites()",
    "bool InterruptCurrentSpellAction::isUseful()",
    "bool InterruptCurrentSpellAction::Execute(Event& event)",
]]
out.joinpath("NativeWand.inc").write_text("\n\n".join(methods), encoding="utf-8")
out.joinpath("NativeInterruptClass.inc").write_text(
    block(header, "    class InterruptCurrentSpellAction :") + ";\n", encoding="utf-8")

# The prerequisite must run before cast eligibility, and Engine keeps the
# original action queued after inserting prerequisites.
engine = (base / "Engine.cpp").read_text(encoding="utf-8")
start = engine.index("bool Engine::DoNextAction(")
walk = engine[start:engine.index("void Engine::", start)]
assert walk.index("actionNode->getPrerequisites()") < walk.index("action->isPossible()")
assert "PushAgain(actionNode, relevance + 0.01, event);" in walk
assert 'creators["interrupt current spell"]' in (base / "actions/ActionContext.h").read_text(encoding="utf-8")
