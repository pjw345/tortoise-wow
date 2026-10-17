# Owned explosives candidate, 2026-10-17

Stacked on PR #44 so the currently tested healer reserve remains present.
This focused PR replaces the PvE companion's hardcoded high-level grenade choice;
it does not merge the potion/buff changes in PR #34.

- At level 25 / Engineering 1, an owned Rough Dynamite is eligible when native
  CanUseItem accepts it. Manufacturing skill is not substituted for actual item
  use requirements.
- Lower usable ItemLevel is consumed before higher tiers; unusable, traded,
  empty, skipped or cooling-down items are ignored.
- AoE strategy must be enabled. Its new owned-explosive trigger checks every five
  seconds and does not require three enemies. Legacy AoE/free-bot/PvP rules stay.
- This is a targeted throw: no initial pull, no chase, no CC damage or additional
  pull from bystanders within the item's native blast radius. Unsupported target
  shapes and missing radius/range data are rejected.
- Native item binding applies even with item cheats. The server owns item count
  reduction and cooldowns. Healers/casts/mounts retain their existing duties.

Validation before release: extracted native fixture passed on Windows/MSVC for
Vanilla/TBC/WotLK definitions. CI provides Linux and Windows runners; outcomes must
be checked separately. Full VM build is deferred while game services are running.

Gameplay acceptance after a server-off build:
1. With AoE enabled and Rough Dynamite in Alon's bags, fight a stationary, isolated
   group attacker in throw range. Check throw animation/damage and stack decreases.
2. Confirm repeated combat respects native cooldowns. Exhaust the rough stack and
   confirm the next owned usable tier is used.
3. Put an unengaged mob or crowd-controlled enemy in the blast and verify no throw.
4. Move the target out of range/LOS; verify no throw/chase caused by this action.
5. Verify healing, fleeing restrictions, mana reserve and ordinary free-bot/PvP
   behavior. Potions remain a separate unresolved gameplay investigation.


Read-only world-data check (2026-10-17): item 4358 requires Engineering 1, 4365
requires 75 and 4374 requires 120. Spells 4054/4061 use damage effect 2 at implicit
target 16 (enemy AoE at destination), range index 4 / radius index 8. Spell 4066
adds aura effect 6 at the same destination target, range index 3 / radius index 15.
This confirms these three basic explosives match the supported native target
geometry; it does not confirm a successful in-game throw or consumption.
