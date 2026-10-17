from pathlib import Path
import sys

root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1])
base = root / "modules/mod-playerbots/src/playerbot/strategy"
cpp = (base / "actions/UseItemAction.cpp").read_text(encoding="utf-8")
header = (base / "actions/UseItemAction.h").read_text(encoding="utf-8")
triggers = (base / "triggers/GenericTriggers.h").read_text(encoding="utf-8")

def section(text, start, end):
    return text[text.index(start):text.index(end, text.index(start))]

out.joinpath("NativeExplosiveClass.inc").write_text(section(header, "    class ThrowGrenadeAction :", "    class UseDarkRuneAction :"), encoding="utf-8")
out.joinpath("NativeExplosives.inc").write_text(section(cpp, "bool ThrowGrenadeAction::UsesOwnedExplosives()", "bool UseSpellItemAction::isUseful()"), encoding="utf-8")
out.joinpath("NativeExplosiveTrigger.inc").write_text(section(triggers, "    class OwnedExplosiveTrigger :", "    class AoeTrigger :"), encoding="utf-8")
# Wiring checks accompany compiled policy tests; actual item consumption still
# requires server-off integration/gameplay validation through the native spell.
assert "if (UseInventoryItemOnly() || RequiresItemToUse(proto, ai, bot))" in cpp
assert "spell->SetCastItem(itemUsed);" in cpp
strategy = (base / "generic/ClassStrategy.cpp").read_text(encoding="utf-8")
assert strategy.count('"owned explosive",\n        NextAction::array(0, new NextAction("throw grenade", ACTION_HIGH), NULL)') == 3
assert 'creators["owned explosive"]' in (base / "triggers/TriggerContext.h").read_text(encoding="utf-8")
