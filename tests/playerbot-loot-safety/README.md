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
