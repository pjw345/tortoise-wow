Source-extracted trigger/selector regression for the selected-corpse deadlock.
Run CMake/build/CTest in this folder. TRIGGER_SOURCE can point at the original
LootTriggers.cpp: both cases must fail before the fix. Production trigger bodies
and LootAction::Execute run with controlled queue, unit, geometry and session
services. This fixture does not prove live pathfinding, spell/loot completion,
world-grid discovery, or real scheduler execution.

The combat-gate cases extract the complete actual trigger source and execute
native loot selection with controlled player/group/pet combat references.
They cover inherited flags without enemies, actual native combat, threat
references, living/dead/absent victims and attackers, other members/pets,
range/map boundaries, PvP and inactive/master mismatch fallback, mounted and
availability conditions, native candidate safety/empty-list handling and immediate
release when links clear without changing shared/native combat state.
ZERO also reruns the existing movement/reselection cases; other variants keep
their previous inherited-flag gate. An older TRIGGER_SOURCE must fail the
loot_combat_inherited regression. Full world-thread execution, native threat
lifecycle and live gameplay are not simulated by these controlled services.
