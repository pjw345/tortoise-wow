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

Emergency backup cases distinguish moderate injuries from critical/lethal melee
pressure, available priests from priests occupied elsewhere, and ongoing partial
heals from uncovered emergencies. The actual action guard covers Turtle dummy
Flash of Light/Holy Light and HEAL_MAX_HEALTH (Lay on Hands).

Additional regressions: installed emergency trigger before/after invitation,
low-health lead time without affordable priest healing, and near-full prepared
direct-heal cancellation while preserving a heal under incoming pressure.


Safety regressions cover survival until native heal arrival, damage pushback
represented by a longer remaining timer, timely/late primary ownership, idle
primary cast-time availability, useful friendly/self heal continuity during
enemy retargeting, dead/foreign-map patients and native learned selection range.
The retarget fixture executes the production guard; the other native targeting
and movement dependencies remain outside this isolated fixture.
