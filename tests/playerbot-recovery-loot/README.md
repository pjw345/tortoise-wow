# Consumable recovery and profession command regression checks

The fixture compiles the production recovery methods and the chest branch of
UseAction::UseGameObject against focused adapters. It checks completion, partial
mana/health, simultaneous recovery, explicit wait/stay and seated-master intent,
casting/combat/death/world/teleport states, and profession missing/too-low/exact
skill, ordinary chest/lockpicking and real open failures. Extraction also checks
the real item and item-cheat paths register recovery before the update loop.

Run cmake -S tests/playerbot-recovery-loot -B build/recovery-loot, build, then
ctest --test-dir build/recovery-loot --output-on-failure. These checks do not
replace a full core build or in-game food/drink aura and node interaction tests.


The native gathering fixture also compiles CanRevealGatheringItem and the AI's
start/update methods. It covers exact interval boundaries, bounded repeats,
no restart/catch-up burst, missing/despawned/depleted/in-use nodes, range/skill,
map/instance/group changes, disabled reveal, combat/death/teleport and fishing.
