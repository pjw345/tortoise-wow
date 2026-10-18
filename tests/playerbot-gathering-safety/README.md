# Native gathering safety regression

Extracts the production AddGatheringLootAction::AddLoot and LootObject::IsApproachSafe.
The fake parent delegates to that native safety helper; extraction also checks
that the production parent validates safety before queue admission. Geometry,
creature discovery, loot eligibility and messages are faked. This does not prove
which filter rejected a particular live node or validate navigation/gameplay.
