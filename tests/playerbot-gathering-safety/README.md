# Native gathering safety regression

Extracts the production AddGatheringLootAction::AddLoot and LootObject::IsApproachSafe.
The fake parent delegates to that native safety helper; extraction also checks
that the production parent validates safety before queue admission. Geometry,
creature discovery, loot eligibility and messages are faked. This does not prove
which filter rejected a particular live node or validate navigation/gameplay.

Follower discovery now scans native visibility and uses native idle aggro plus
the interaction area. Configured combat distance is a minimum for engaged mobs;
zero still opts out. This fixture retains separate solo/leader, gathering range,
skill, bags and LOS controls; native level/aura math is covered in loot-safety.
