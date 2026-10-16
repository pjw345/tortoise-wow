# Owned consumables: next patch acceptance

## Requirements and source evidence

Alon owns 32 Rough Dynamite and crafted it himself. Use eligible lower-tier stock
before higher-tier stock, retaining ordinary engineering item requirements and
safe combat use. Flore should use useful owned buff potions and try a mana potion
before reporting low/out-of-mana during combat. Drinking remains preferred safely
out of combat; no item is manufactured for the new inventory actions.

Read-only local world-data inspection confirmed: Rough Dynamite 4358 is trade
goods/explosives, ItemLevel 10, Engineering 1, on-use damage spell 4054; Coarse
Dynamite 4365 requires Engineering 75. Minor Mana 2455 is generic consumable
subclass 0, ItemLevel 15, required level 5, mana spell 437. Weak Troll Blood 3382
is also subclass 0 and uses regeneration aura 161 via spell 3219. Engineering's
old native action requires level 52/skill 175 and chooses fixed grenades.

## Next focused playtest after deployment

1. Alon: note the current Rough Dynamite count. In a suitable grouped-enemy fight
   with his allowed target, confirm a throw and that the real stack decreases.
   He should exhaust eligible Rough stock before Coarse or higher-level stock.
   The existing AoE strategy/trigger must be enabled; single-target fights need
   not result in a throw.
2. Verify a throw is deferred when range/LOS is invalid, another mob could be
   pulled, a nearby enemy is crowd controlled, Alon is healing or already casting,
   or the explosive is on cooldown. It must not start an independent pull.
3. Flore: while safely stationary and recovered out of combat, confirm one useful
   Weak Troll Blood dose is consumed and its regeneration aura appears. Active
   stronger/equal/weaker buffs must not be needlessly replaced or refreshed.
   Strength-only potions should not be consumed by a ranged Priest.
4. During combat, let her mana fall low with a usable Minor Mana Potion in bags.
   Confirm actual use, stack decrease and mana restoration before an unnecessary
   warning. With no usable potion, a failed cast or cooldown, the original
   throttled warning should still appear. If a small dose leaves her short, a
   subsequent warning is valid. Normal potion cooldowns are not shortened.
5. Repeat without potions, with higher-level/unusable items, with item cheats
   enabled, after relog and while healing. Buffs and explosives must not interrupt
   pending healing. Free-bot behavior and emergency health potions are unchanged.

## Validation boundary

Focused production-method policy tests cover selection/exhaustion, trade,
level/skill, cooldown, skipped spells, buff relevance/stacking, casting/recovery,
AOE safety and warning fallback. Selected native implementation objects are
compiled in an isolated CPU/memory-limited toolchain container. The full action
registry translation unit was stopped to avoid excessive memory use alongside
live play; full server linking/release building and in-game validation remain
separate steps. No live server restart, configuration or database write is part
of preparing this patch. This patch is based on the currently deployed PR #33 revision a9a5f0b2, retaining its drinking/ping/loot changes. These additional consumable changes are not deployed.
