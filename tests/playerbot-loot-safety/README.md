# Loot destination safety regressions

CMake extracts the production IsApproachSafe method and the admission portion
of MoveToLootAction::Execute before native movement dispatch. The harness mocks
the core hostile query and movement boundary, not the policy implementation.
It checks active/inactive masters, safe/unsafe destinations, queued target danger,
deferral and recovery, missing objects and existing leader/solo/config opt-outs.
The core hostile-query filtering and real pathfinding remain native and are not
replaced by these tests. No live dungeon run or full server build is asserted.

Run cmake -S tests/playerbot-loot-safety -B build/loot-safety, build it, then
ctest --test-dir build/loot-safety --output-on-failure.

Level-aware cases also compile the production Creature::GetAttackDistance body.
They cover 27-versus-13 and 10-versus-20, native caps/minimum, server aggro rate,
detection/detected-range auras, custom/disabled detection, non-initiating mobs,
the interaction-area allowance, larger candidate scans, engaged combat minima,
wall/floor LOS and three-dimensional separation. Discovery/geometry and world
services remain fakes; this does not certify route-wide safety or every custom
scripted encounter. The candidate window uses native visibility/area overrides.

The default detection fixture is 20 yards, matching Creature construction in
this fork; the older native formula comment saying roughly 18 is not its value.

Building-floor cases distinguish an idle mob directly overhead behind a solid
floor from an open vertical connection or nearby stairs/ramp, with identical
experience levels. LOS is supplied by a geometry fake: this checks policy, not
the installed VMAP collision data. Engaged mobs remain conservatively protected.
