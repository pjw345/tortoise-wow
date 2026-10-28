Executes the production BoxFormation/CircleFormation, base angle/offset and
native roster helper, FormationValue constructor/load/save/reset, and formation
chat handler. Uses the native MAX_GROUP_SIZE definition. World/roles/DBC/map/chat
services are controlled; other mode constructors are registration-only stubs.
Existing circle/follow suites exercise their real geometry/movement consumers.

Cases cover two front/three rear members, coordinate/facing changes, repeated
getters with moved bots, group-reference permutation, spacing, eligibility and
membership changes, role changes and caster masters, 1–40 group members, invalid
targets/groups/ranges, ground/flight and native combat/ghost fallback. Selection
round-trips the actual formation name without executing a database session. The
old native loader is an expected-failing selection control. These are geometry
and native-call contract tests, not live pathfinding, sockets, NPC reach or
exhaustive client/combat behavior. No server build or live setting is changed.

The role adapter now executes production IsRanged/IsHeal/IsTank and the actual
TankAssistStrategy::GetType with the native StrategyType enum. Engine strategy
presence, human fallback roles and aura state are controlled services. The
shared-assist case models all companion combat sets advertising the tank bit
alongside ranged/healer types and must fail against deployed ceddc25d.
