# Owned engineering explosives

This isolated C++17 fixture extracts the current native ThrowGrenadeAction class,
its selector/safety methods, UseItemIdAction usefulness/possibility and the owned
explosive trigger. It compiles with Vanilla/TBC/WotLK preprocessor definitions.
Core services and world objects are stubbed; this is not a full core build.

Cases cover Engineering 1 Rough Dynamite at level 25, real item requirements,
lowest-tier inventory order, exhausted/traded/forbidden stacks, cooldown/skip lists,
missing spell/range/radius, single-target combat scheduling, CC and unengaged blast
bystanders, target/LOS/range/fleeing policy, casting/mount/healer roles, native owned
item possession despite item cheats, and unchanged free-bot/PvP grenade selection.
Extraction also checks trigger registration/scheduling for all three variants and
the item-binding hook. Real spell execution and count reduction need gameplay.

    cmake -S tests/playerbot-owned-explosives -B build-owned-explosives
    cmake --build build-owned-explosives --config Release
    ctest --test-dir build-owned-explosives -C Release --output-on-failure
