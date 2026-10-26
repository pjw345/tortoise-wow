# Custom game-object catalogue

CMake/CTest extract the production compatibility proxy, native ObjectMgr maximum
getter and DropMap/EntryLootList functions. Sparse native objects and loot stores
are faked. This exercises the previously missing upstream catalogue path rather
than injecting a ready-made list into ShouldLootObject. Replacing the proxy bound
with its old 200000 stub fails the test. Run alongside playerbot-loot-followups
for policy overrides, mixed/unknown sources and normal loot behavior. Native
syntax/build and live acceptance are separate. No live bot value debug call is used.
