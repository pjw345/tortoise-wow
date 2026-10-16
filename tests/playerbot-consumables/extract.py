from pathlib import Path
import sys
root=Path(__file__).resolve().parents[2]
b=root/'modules/mod-playerbots/src/playerbot'
source=(b/'strategy/actions/UseItemAction.cpp').read_text()
start=source.index('bool UseAction::IsUsefulInventoryBuff(')
end=source.index('bool UseAction::HasItemCooldown(',start)
Path(sys.argv[1],'NativeConsumables.inc').write_text(source[start:end])
h=(b/'strategy/actions/UseItemAction.h').read_text()
start=h.index('    class UseManaPotionAction ')
end=h.index('    class UseHearthStoneAction ',start)
classes=h[start:end]
start=h.index('    class ThrowGrenadeAction ')
end=h.index('    class UseDarkRuneAction ',start)
Path(sys.argv[1],'NativeClasses.inc').write_text(classes+h[start:end])
s=(b/'strategy/actions/SayAction.cpp').read_text()
start=s.index('bool SayAction::Execute(Event& event)')
end=s.index('    std::string text = "";',start)
Path(sys.argv[1],'NativeWarning.inc').write_text(s[start:end]+'    ++warnings; return false;\n}\n')
assert 'time_t lastSaid = AI_VALUE2(time_t, "last said", qualifier)' in s
assert '(time(0) - lastSaid) > 30' in s
assert 'new NextAction("inventory mana potion", ACTION_HIGH + 1), new NextAction("dark rune", ACTION_HIGH)' in (b/'strategy/generic/UsePotionsStrategy.cpp').read_text()
assert 'new NextAction("buff potion", 1.1f)' in (b/'strategy/generic/NonCombatStrategy.cpp').read_text()
for action in ['buff potion','inventory mana potion']:
 assert 'creators["'+action+'"]' in (b/'strategy/actions/ActionContext.h').read_text()

assert 'if (UseInventoryItemOnly() || RequiresItemToUse(proto, ai, bot))' in source
assert h.count('bool UseInventoryItemOnly() const override') == 3
