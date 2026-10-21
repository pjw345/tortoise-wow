# Playerbot quest-sync regressions

Configure this directory as a standalone CMake project, build and run CTest.
`extract.py` compiles the production `QuestSync.cpp` adapter with host API fakes;
`QuestSyncProtocol.h` is included directly. No game server or database is needed.

Run `QuestSyncTest <wire.hex>` to save its actual native packets as hex lines.
Pass that fixture to `test_client.py --wire <wire.hex>` to check the shipped Lua
decoder against the server's output. Client tests require Python's Lupa `lua51`
module. `--deps <directory>` can select a local dependency directory.

Optional `--lua50 <executable>` runs the same complete cases with Lua 5.0.3.
Optional `--pfquest-tracker <tracker.lua>` extracts the installed pfQuest's real
`ButtonEvent` and `DoLayout`, preserving their implementation in the UI tests.
Only their host APIs and local input values are faked. No pfQuest source is
modified or copied into this repository.

These focused checks do not replace the full native build, actual client AddOn
delivery/rendering or live quest-progress acceptance.
