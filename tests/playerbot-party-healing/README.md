# Party healing regression

Build with CMake and run CTest. The fixture extracts production PlayerbotAI
coverage/backup helpers, the coordinated selector branch, heal trigger,
heal-action checks and native interrupt action. World objects and bonus/cost
helpers are bounded fakes; this proves policy behavior, not live bonus values,
spell scripts, timing under server load, or gameplay acceptance.

Run alongside tests/playerbot-master-focus, tests/playerbot-wand-handoff and
tests/party-battle-log. The master-focus test covers the backup Paladin's healer
peel exception and return to master focus; wand tests cover all three expansion
macros. The first live acceptance fight must compare mana spent, late cancelled
heals, overheal, primary/backup healing and defensive target changes.
