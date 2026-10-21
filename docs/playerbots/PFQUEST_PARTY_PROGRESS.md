# pfQuest party quest progress

The server supplies read-only, owner-authorized Playerbot quest snapshots using
`PFQBOT` protocol version 1. This repository owns QuestSync.cpp, QuestSync.h,
QuestSyncProtocol.h, the Playerbot command integration and native tests.

The companion AddOn, Lua tests, installation instructions and downloadable ZIPs
are maintained at [pjw345/pfQuest-partybots](https://github.com/pjw345/pfQuest-partybots).
Download releases from https://github.com/pjw345/pfQuest-partybots/releases.
No AddOn source is vendored here; obtain it from the companion repository.

## Compatibility and installation

- WoW 1.12.1 / Interface 11200 with pfQuest; tracker checks use pfQuest 8.0.1.
- Optional pfQuest-turtle. Existing pfQuest files are not modified.
- Companion version 1.0.0 uses PFQBOT protocol version 1.
- Initial matching server implementation: 591c68d49312c91fde933d38a0613db414471148
  (PR #61). The running combined stack 2151c15b includes this implementation.
- An unmodified server supplies no snapshots; unavailable data is not quest completion.

Exit the client and extract the release's `pfQuest-partybots` folder into
`Interface/AddOns`, beside pfQuest, then restart the client and enable both.
Use `/pfqb` for the comparison or `/pfqb refresh` for an update.

## Coordinated validation

Build/run the standalone native tests in `tests/playerbot-quest-sync`, then
pass their emitted wire fixture to the companion's client suite as documented
in that directory's README. The server tests compile the actual adapter with
host API fakes; the Lua tests consume the emitted packets. Protocol changes
require coordinated client/server changes and a version compatibility decision.

The initial implementation passed native focused tests, eight client cases on
Lua 5.0.3/5.1, full native image compilation and startup/connectivity checks.
Live AddOn transport, graphics and quest-progress acceptance remain pending.
Moving the AddOn to its own repository changes no server runtime source and
requires no server rebuild or restart. Report UI issues in the companion
repository and server snapshot issues in this repository.
