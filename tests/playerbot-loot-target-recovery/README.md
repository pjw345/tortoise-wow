Source-extracted trigger/selector regression for the selected-corpse deadlock.
Run CMake/build/CTest in this folder. TRIGGER_SOURCE can point at the original
LootTriggers.cpp: both cases must fail before the fix. Production trigger bodies
and LootAction::Execute run with controlled queue, unit, geometry and session
services. This fixture does not prove live pathfinding, spell/loot completion,
world-grid discovery, or real scheduler execution.
