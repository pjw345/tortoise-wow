# Activated quest chest regression

This fixture compiles the production availability query, activated-chest admission
and group turn selector, native quest slot generation/resolution, successful-take
flags/count updates, the entire native game-object release case, and the native
chest restock/respawn scheduling fragments. Inventory storage, script hooks,
world visibility and the map update clock are represented by fakes; this is not
full server or live gameplay acceptance.

Coverage includes shared Miners' Gear with a remaining different-quest gem,
Cria/Alon receiving no shared item, selection of the bot eligible for the gem,
preserving another player's remaining loot, final-item deactivation and configured
respawn scheduling, FFA per-player copies and counts, native restocking and zero
respawn delay, fresh generation, stale/current eligibility, null/offline players,
slot capacity and already-created views. Repeated queries do not change slots or
counts. The pre-fix adapter is built separately and must fail the empty shared
crate case; CTest marks that nonzero exit as the expected negative control.

Run `cmake -S tests/quest-chest-availability -B /tmp/quest-chest-tests`, build,
then `ctest --test-dir /tmp/quest-chest-tests --output-on-failure`.

No global loot-count adjustment, forced chest despawn, new timer, inventory
mutation or production data change is introduced by this fix.
