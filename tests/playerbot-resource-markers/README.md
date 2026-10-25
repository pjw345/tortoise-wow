# Group resource feed regression tests

Configure/build with CMake, then run CTest. The protocol fixture compiles the
production header. The world fixture extracts the production send/eligibility
functions and quest-sync authorization, while faking the world, grid and DBC
services. It covers authorization after queueing, skill thresholds, filter/state,
native visibility delegation, range, maps, dead/teleporting bots, empty snapshots
and nearest-node bounds. It cannot certify actual DBC/map data, native visibility
implementation or multithreaded live execution. The changed translation units
also require syntax/full-build checks with real core headers. Run the companion
MangosBot Lua 5.0 tests and perform live acceptance separately.

Player/self coverage also checks native player-only eligibility without a group,
machine rejection, empty self snapshots, concurrent/single-pending request mailbox,
five-second admission and source-extracted native chat relay suppression. Requests
are queued on the session handler and consumed by the player-owned manager update.
